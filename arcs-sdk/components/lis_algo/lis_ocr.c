#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include "lis_ocr.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"
#include "lisa_device.h"
#ifdef CONFIG_LISA_CAMERA_DEVICE
#include "lisa_camera.h"
#include "lisa_gpio.h"
#include "IOMuxManager.h"
#include "systick.h"
#else
#include "lisa_sdmmc.h"
#endif
#include "acomp_cv.h"
#include "acomp_stream_ipc.h"
#ifdef CONFIG_ACOMP_RESMGR
#include "resmgr.h"
#endif

#define TAG "lis_ocr"
#include "lisa_log.h"

#include "queue.h"

#define RESULT_SIZE (1024)

/* Image size (OCR_IMG_WIDTH/HEIGHT from lis_ocr.h) */
#define SCAN_IMAGE_SIZE          ((unsigned long)OCR_IMG_WIDTH * OCR_IMG_HEIGHT)  /* 23040 */

#ifdef CONFIG_LISA_CAMERA_DEVICE
/* Camera hardware pins */
#define CAMERA_MCLK_PAD         CSK_IOMUX_PAD_A
#define CAMERA_MCLK_PIN         26
#define CAMERA_PWDN_PIN         9
#define CAMERA_XCLK_FREQ_HZ    (25 * 1000 * 1000)

/* Scan key GPIO for startup detection (GPIOB pin8) */
#define SCAN_KEY_PIN  8

/* SPI camera bus pins */
#define SPI_CAM_PAD             CSK_IOMUX_PAD_A
#define SPI_CAM_CLK_PIN         25
#define SPI_CAM_MOSI_PIN        24
#define SPI_CAM_CS_PIN          22
#define SPI_CAM_DMA_CHANNEL     0

/* Scan LED control (PA23, same as old AP core) */
#define SCAN_LED_PAD            CSK_IOMUX_PAD_A
#define SCAN_LED_PIN            23
#define SCAN_LED_DELAY_US       50
#define SCAN_LED_BRIGHTNESS_DEFAULT 10
#else
/* eMMC scan key events */
#define SCAN_EVT_START          (1 << 0)
#define SCAN_EVT_STOP           (1 << 1)
#define SCAN_EVT_RESULT         (1 << 2)

/* eMMC test image configuration */
#define IMAGE_EMMC_ADDR          0x9600000
#define IMAGE_MAX_COUNT          100
#define SECTOR_SIZE              512
#endif

/* CV stream frame header: [status(4B)][fuid(4B)][w(4B)][h(4B)][fb_addr(4B)] */
#define CV_FRAME_HDR_SIZE        (5 * sizeof(uint32_t))

/* CV stream TX configuration (M2R: CP→AP image stream) */
#define CV_STREAM_TX_CH_INDEX    0
#define CV_STREAM_TX_CH_CNAME    "stream.cv_image"
#define CV_STREAM_TX_BUF_SIZE    CV_FRAME_HDR_SIZE
#define CV_STREAM_TX_NUM_DESCS   4

/* CV status (matches AP side cv_status_e) */
#define CV_STATUS_BEGIN     0
#define CV_STATUS_CONTINUE  1
#define CV_STATUS_END       2

/* Resource storage type (matches acomp_res_item_attr_t.hdr.storage) */
#define RES_STORAGE_FLASH   0
#define RES_STORAGE_SD      1

/* Image save to SD card (async queue + write task) */
#define IMG_SAVE_DIR          "/SD:/user"
#define IMG_SAVE_QUEUE_LEN    8
#define IMG_SAVE_TASK_STACK   4096

typedef enum {
    IMG_SAVE_MSG_RAW_START,   /* open raw file for new session */
    IMG_SAVE_MSG_RAW_DATA,    /* append one raw frame */
    IMG_SAVE_MSG_RAW_END,     /* close raw file */
    IMG_SAVE_MSG_SAVE,        /* save single image (stitch/cutline) */
    IMG_SAVE_MSG_EXIT,        /* sentinel: task should exit */
} img_save_msg_type_e;

typedef struct {
    img_save_msg_type_e msg_type;
    void *buffer;
    uint32_t len;
    uint16_t width;
    uint16_t height;
    uint8_t img_type;
} img_save_msg_t;

static QueueHandle_t s_img_save_queue = NULL;
static TaskHandle_t s_img_save_task_hdl = NULL;
static volatile bool s_img_save_enabled = false;

/* eMMC resource addresses for CV models (configured via Kconfig) */
#define CUTLINE_EMMC_ADDR     CONFIG_ACOMP_CV_RES_CUTLINE_EMMC_ADDR
#define CUTLINE_RES_SIZE      CONFIG_ACOMP_CV_RES_CUTLINE_SIZE
#define OCR_THINKER_EMMC_ADDR CONFIG_ACOMP_CV_RES_OCR_EMMC_ADDR
#define OCR_THINKER_RES_SIZE  CONFIG_ACOMP_CV_RES_OCR_SIZE


/* Startup type: 1=normal, 2=scan_key (fast scan) */
#define SYSTEM_START_UP_TYPE_NORMAL    1
#define SYSTEM_START_UP_TYPE_SCAN_KEY  2

static int s_start_up_type = SYSTEM_START_UP_TYPE_NORMAL;

static struct
{
    int ocr_st;
    scan_mode_e mode;
    lis_ocr_handmode handmode;
    lis_ocr_status status;
    char *result;
} ocr;

static const char OCR_TAG[] = "ocr";

static ocr_result_callback_t g_ocr_rslt_cb = NULL;

static TaskHandle_t s_capture_task_hdl = NULL;

#ifdef CONFIG_LISA_CAMERA_DEVICE
static volatile bool s_capture_running = false;
static lisa_device_t *s_led_gpio_dev = NULL;
static uint8_t s_led_brightness = SCAN_LED_BRIGHTNESS_DEFAULT;
static int s_led_last_state = -1;
#endif

#ifdef CONFIG_LISA_CAMERA_DEVICE
static void pending_fb_release(uint32_t addr);
#endif

/* acomp CV event callback */
static void ocr_event_cb(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{
    if (event & CV_CB_EVENT_OCR_RESULT) {
        if (event_data && event_data_len > 0 && ocr.result != NULL) {
            uint32_t copy_len = event_data_len < RESULT_SIZE - 1
                              ? event_data_len : RESULT_SIZE - 1;
            memcpy(ocr.result, event_data, copy_len);
            ocr.result[copy_len] = '\0';
            ocr.status = LIS_OCR_STATE_RESULT;
            lis_ocr_result_callback(ocr.result);
        }
    }
    if (event & CV_CB_EVENT_STATUS) {
        if (event_data && event_data_len >= sizeof(uint32_t)) {
            ocr.status = *(uint32_t *)event_data;
        }
    }
#ifdef CONFIG_LISA_CAMERA_DEVICE
    if (event & CV_CB_EVENT_FRAME_DONE) {
        if (event_data && event_data_len >= sizeof(uint32_t)) {
            uint32_t fb_addr = *(uint32_t *)event_data;
            pending_fb_release(fb_addr);
        }
    }
#endif
    if (event & CV_CB_EVENT_IMG_SAVE) {
        if (!s_img_save_enabled) return;
        if (event_data && event_data_len >= sizeof(cv_img_save_info_t)) {
            cv_img_save_info_t *img = (cv_img_save_info_t *)event_data;
            uint32_t img_size = (uint32_t)img->width * img->height;

            HAL_InvalidateDCache_by_Addr((uint32_t *)(uintptr_t)img->img_addr, img_size);

            void *buf = psram_malloc(img_size);
            if (buf) {
                memcpy(buf, (void *)(uintptr_t)img->img_addr, img_size);
                img_save_msg_t msg = {
                    .msg_type = IMG_SAVE_MSG_SAVE,
                    .buffer = buf,
                    .len = img_size,
                    .width = img->width,
                    .height = img->height,
                    .img_type = img->img_type,
                };
                if (xQueueSend(s_img_save_queue, &msg, 0) != pdPASS) {
                    psram_free(buf);
                    LOGW(OCR_TAG, "img_save queue full, drop type=%d", img->img_type);
                }
            } else {
            LOGE(OCR_TAG, "img_save alloc %u bytes fail", img_size);
            }
        }
    }
}

/* --- stream TX / image source / cv prepare helpers --- */

#ifdef CONFIG_LISA_CAMERA_DEVICE
static lisa_device_t *s_camera_dev = NULL;
#else
static lisa_device_t *s_sdmmc_dev = NULL;
static EventGroupHandle_t scan_key_evt_hdl;
#endif

#ifdef CONFIG_LISA_CAMERA_DEVICE
/* Pending FB management for zero-copy frame transfer */
#define MAX_PENDING_FBS  3
static struct {
    lisa_camera_fb_t *fb;
    uint32_t addr;
} s_pending_fbs[MAX_PENDING_FBS];

static void pending_fb_push(lisa_camera_fb_t *fb)
{
    taskENTER_CRITICAL();
    for (int i = 0; i < MAX_PENDING_FBS; i++) {
        if (s_pending_fbs[i].fb == NULL) {
            s_pending_fbs[i].fb = fb;
            s_pending_fbs[i].addr = (uint32_t)(uintptr_t)fb->buf;
            taskEXIT_CRITICAL();
            return;
        }
    }
    taskEXIT_CRITICAL();
    ESP_LOGE(OCR_TAG, "pending_fb_push: no slot, force release fb=%p", fb->buf);
    lisa_camera_release_fb(s_camera_dev, fb);
}

static void pending_fb_release(uint32_t addr)
{
    lisa_camera_fb_t *fb_to_release = NULL;
    taskENTER_CRITICAL();
    for (int i = 0; i < MAX_PENDING_FBS; i++) {
        if (s_pending_fbs[i].addr == addr && s_pending_fbs[i].fb != NULL) {
            fb_to_release = s_pending_fbs[i].fb;
            s_pending_fbs[i].fb = NULL;
            s_pending_fbs[i].addr = 0;
            break;
        }
    }
    taskEXIT_CRITICAL();
    if (fb_to_release) {
        lisa_camera_release_fb(s_camera_dev, fb_to_release);
    } else {
        ESP_LOGW(OCR_TAG, "pending_fb_release: addr=0x%08x not found", addr);
    }
}

static void pending_fb_release_all(void)
{
    lisa_camera_fb_t *fbs[MAX_PENDING_FBS] = {NULL};
    taskENTER_CRITICAL();
    for (int i = 0; i < MAX_PENDING_FBS; i++) {
        fbs[i] = s_pending_fbs[i].fb;
        s_pending_fbs[i].fb = NULL;
        s_pending_fbs[i].addr = 0;
    }
    taskEXIT_CRITICAL();
    for (int i = 0; i < MAX_PENDING_FBS; i++) {
        if (fbs[i] != NULL) {
            lisa_camera_release_fb(s_camera_dev, fbs[i]);
        }
    }
}
#endif

static void img_save_write_task(void *param)
{
    img_save_msg_t msg;
    int session_id = -1;
    FILE *raw_fp = NULL;

    while (1) {
        if (xQueueReceive(s_img_save_queue, &msg, portMAX_DELAY) != pdPASS) {
            continue;
        }

        switch (msg.msg_type) {
        case IMG_SAVE_MSG_RAW_START: {
            session_id++;

            /* Create session directory: /SD:/user/000/ */
            char dir[40];
            snprintf(dir, sizeof(dir), "%s/%03d", IMG_SAVE_DIR, session_id);
            mkdir(dir, 0);

            char path[80];
            snprintf(path, sizeof(path), "%s/raw_%ux%u.yuv",
                     dir, msg.width, msg.height);
            raw_fp = fopen(path, "wb");
            if (!raw_fp) {
                ESP_LOGE(OCR_TAG, "img_save: open %s failed", path);
            } else {
                ESP_LOGI(OCR_TAG, "img_save: session %d start -> %s", session_id, path);
            }
            break;
        }
        case IMG_SAVE_MSG_RAW_DATA:
            if (raw_fp && msg.buffer) {
                fwrite(msg.buffer, 1, msg.len, raw_fp);
            }
            if (msg.buffer) psram_free(msg.buffer);
            break;

        case IMG_SAVE_MSG_RAW_END:
            if (raw_fp) {
                fclose(raw_fp);
                ESP_LOGI(OCR_TAG, "img_save: session %d raw end", session_id);
                raw_fp = NULL;
            }
            break;

        case IMG_SAVE_MSG_SAVE: {
            const char *type_str;
            switch (msg.img_type) {
            case CV_IMG_TYPE_STITCH:  type_str = "stitch";  break;
            case CV_IMG_TYPE_CUTLINE: type_str = "cutline"; break;
            default:                  type_str = "unknown"; break;
            }
            char path[80];
            snprintf(path, sizeof(path), "%s/%03d/%s_%ux%u.yuv",
                     IMG_SAVE_DIR, session_id, type_str, msg.width, msg.height);
            FILE *fp = fopen(path, "wb");
            if (fp) {
                size_t written = fwrite(msg.buffer, 1, msg.len, fp);
                fclose(fp);
                ESP_LOGI(OCR_TAG, "img_save: %s (%u bytes)", path, (unsigned)written);
            } else {
                ESP_LOGE(OCR_TAG, "img_save: open %s failed", path);
            }
            if (msg.buffer) psram_free(msg.buffer);
            break;
        }
        case IMG_SAVE_MSG_EXIT:
            goto exit;
        }
    }

exit:
    if (raw_fp) {
        fclose(raw_fp);
    }
    /* Drain remaining messages */
    while (xQueueReceive(s_img_save_queue, &msg, 0) == pdPASS) {
        if (msg.buffer) psram_free(msg.buffer);
    }
    s_img_save_task_hdl = NULL;
    vTaskDelete(NULL);
}

static int lis_ocr_stream_init(void)
{
    acomp_stream_chn_create_desc_t desc = {
        .cname = CV_STREAM_TX_CH_CNAME,
        .direction = ACOMP_STREAM_DIRECTION_M2R,
        .index = CV_STREAM_TX_CH_INDEX,
        .buffer_size = CV_STREAM_TX_BUF_SIZE,
        .num_descs = CV_STREAM_TX_NUM_DESCS,
        .kick_policy = 1,
    };
    return acomp_cv_stream_ch_enable(CV_STREAM_TX_CH_INDEX, &desc);
}

static int lis_ocr_send_frame(uint32_t fb_addr, int fuid,
                              uint16_t w, uint16_t h, uint32_t cv_status)
{
    uint32_t buf_size = 0;
    uint16_t desc_idx = 0;

    uint8_t *buf = acomp_cv_stream_tx_buffer_alloc(CV_STREAM_TX_CH_INDEX, &buf_size, &desc_idx);
    if (!buf || buf_size < CV_FRAME_HDR_SIZE) {
        ESP_LOGE(OCR_TAG, "stream tx alloc fail: buf=%p size=%u need=%u",
                 buf, buf_size, (unsigned)CV_FRAME_HDR_SIZE);
        return -1;
    }

    uint32_t *hdr = (uint32_t *)buf;
    hdr[0] = cv_status;
    hdr[1] = (uint32_t)fuid;
    hdr[2] = w;
    hdr[3] = h;
    hdr[4] = fb_addr;

    return acomp_cv_stream_tx_buffer_submit(CV_STREAM_TX_CH_INDEX, buf,
        CV_FRAME_HDR_SIZE, desc_idx);
}

#ifdef CONFIG_LISA_CAMERA_DEVICE

/* ---------- Scan key startup detection ---------- */

static void scankey_gpio_check(void)
{
    lisa_device_t *gpio_dev = lisa_device_get("gpiob");
    if (!gpio_dev) {
        ESP_LOGE(OCR_TAG, "gpiob device not found for scan key check");
        return;
    }
    int ret = lisa_gpio_configure(gpio_dev, SCAN_KEY_PIN, LISA_GPIO_INPUT);
    if (ret != 0) {
        ESP_LOGE(OCR_TAG, "scan key gpio config fail: %d", ret);
        return;
    }
    int level = lisa_gpio_read_pin(gpio_dev, SCAN_KEY_PIN);
    if (level == LISA_GPIO_LOW) {
        s_start_up_type = SYSTEM_START_UP_TYPE_SCAN_KEY;
    }
    ESP_LOGI(OCR_TAG, "scan key level: %d, start_up_type: %d", level, s_start_up_type);
}

static void scan_led_switch(bool on);

/* ---------- Scan LED control ---------- */

static int scan_led_init(void)
{
    s_led_gpio_dev = lisa_device_get("gpioa");
    if (!s_led_gpio_dev) {
        ESP_LOGE(OCR_TAG, "gpioa device not found");
        return -1;
    }

    int ret = lisa_gpio_configure(s_led_gpio_dev, SCAN_LED_PIN,
                                  LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_LOW);
    if (ret != 0) {
        ESP_LOGE(OCR_TAG, "scan led gpio config fail: %d", ret);
        return ret;
    }

    s_led_brightness = SCAN_LED_BRIGHTNESS_DEFAULT;
    s_led_last_state = -1;
    ESP_LOGI(OCR_TAG, "scan led init ok (PA%d, brightness=%d)", SCAN_LED_PIN, s_led_brightness);
    return 0;
}

static void scan_led_switch(bool on)
{
    if (s_led_last_state == (int)on) {
        return;
    }
    s_led_last_state = (int)on;

    if (!on) {
        lisa_gpio_write_pin(s_led_gpio_dev, SCAN_LED_PIN, LISA_GPIO_LOW);
    } else {
        /* 软件模拟 PWM：通过亮度等级控制脉冲次数 */
        for (int i = 0; i < s_led_brightness; i++) {
            lisa_gpio_write_pin(s_led_gpio_dev, SCAN_LED_PIN, LISA_GPIO_LOW);
            SysTick_Delay_Us(SCAN_LED_DELAY_US);
            lisa_gpio_write_pin(s_led_gpio_dev, SCAN_LED_PIN, LISA_GPIO_HIGH);
            SysTick_Delay_Us(SCAN_LED_DELAY_US);
        }
    }
}

/* ---------- Camera init ---------- */

static int lis_ocr_camera_init(void)
{
    s_camera_dev = lisa_device_get("camera");
    if (!s_camera_dev) {
        ESP_LOGE(OCR_TAG, "camera device not found");
        return -1;
    }

    /* 1. Setup: PWDN, MCLK, sensor probe, fb alloc */
    lisa_camera_config_t cam_cfg = {
        .hw_config = {
            .mclk_pad = CAMERA_MCLK_PAD,
            .mclk_pin = CAMERA_MCLK_PIN,
            .pwdn_gpio_dev = lisa_device_get("gpiob"),
            .pwdn_pin = CAMERA_PWDN_PIN,
            .pwdn_delay_us = 1,
            .xclk_delay_us = 1,
            .i2c_dev = lisa_device_get("i2c1"),
        },
        .xclk_freq_hz = CAMERA_XCLK_FREQ_HZ,
        .fb_count = 3,
        .enable_hmirror = false,
        .enable_vflip = false,
        .enable_colorbar = false,
    };
    int ret = lisa_camera_setup(s_camera_dev, &cam_cfg);
    if (ret != 0) {
        ESP_LOGE(OCR_TAG, "camera setup fail: %d", ret);
        return ret;
    }

    lisa_camera_bus_config_t bus_cfg = {
        .bus_type = LISA_CAMERA_BUS_SPI,
        .config.spi = {
            .spi_dev = lisa_device_get("spi0"),
            .spi_freq = 0,
            .spi_mode = 1,          /* CPOL=0, CPHA=1 */
            .spi_bit_order = 1,     /* LSB first */
            .cs_gpio = lisa_device_get("gpioa"),
            .cs_pin = SPI_CAM_CS_PIN,
        },
        .dma_channel = SPI_CAM_DMA_CHANNEL,
        .pixel_format = LISA_CAMERA_PIXFMT_GRAY,
        .width = OCR_IMG_WIDTH,
        .height = OCR_IMG_HEIGHT,
    };
    ret = lisa_camera_attach_bus(s_camera_dev, &bus_cfg);
    if (ret != 0) {
        ESP_LOGE(OCR_TAG, "attach bus fail: %d", ret);
        return ret;
    }

    ESP_LOGI(OCR_TAG, "camera init ok");
    return 0;
}

static void lis_ocr_capture_task(void *param)
{
    ESP_LOGI(OCR_TAG, "capture task ready");

    while (1) {
        /* 等待启动信号 */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        int fuid = 0;
        bool first_frame = true;

        s_img_save_enabled = (ocr.mode == e_scan_mode_debug);

        if (s_img_save_enabled) {
            /* Notify write task: open raw file for this session */
            img_save_msg_t start_msg = {
                .msg_type = IMG_SAVE_MSG_RAW_START,
                .width = OCR_IMG_WIDTH,
                .height = OCR_IMG_HEIGHT,
            };
            xQueueSend(s_img_save_queue, &start_msg, portMAX_DELAY);
        }

        ESP_LOGI(OCR_TAG, "capture loop begin, save=%d", s_img_save_enabled);
        TickType_t cap_start_tick = xTaskGetTickCount();
        TickType_t cap_end_tick = cap_start_tick;

        while (s_capture_running) {
            lisa_camera_fb_t *fb = NULL;
            int ret = lisa_camera_capture(s_camera_dev, &fb);
            if (ret != 0 || !fb) {
                if (!s_capture_running) break;
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }

            cap_end_tick = xTaskGetTickCount();
            /* Discard first frame (camera data not stable) */
            if (first_frame) {
                first_frame = false;
                lisa_camera_release_fb(s_camera_dev, fb);
                continue;
            }

            uint32_t status = (fuid == 0) ? CV_STATUS_BEGIN : CV_STATUS_CONTINUE;

            /* Save raw frame via async queue (memcpy before push, FB still valid) */
            if (s_img_save_enabled) {
                size_t raw_size = (size_t)fb->width * fb->height;
                void *raw_buf = psram_malloc(raw_size);
                if (raw_buf) {
                    memcpy(raw_buf, fb->buf, raw_size);
                    img_save_msg_t data_msg = {
                        .msg_type = IMG_SAVE_MSG_RAW_DATA,
                        .buffer = raw_buf,
                        .len = raw_size,
                    };
                    if (xQueueSend(s_img_save_queue, &data_msg, 0) != pdPASS) {
                        psram_free(raw_buf);
                    }
                }
            }

            uint32_t fb_addr = (uint32_t)(uintptr_t)fb->buf;
            pending_fb_push(fb);
            ret = lis_ocr_send_frame(fb_addr, fuid++, fb->width, fb->height, status);
            if (ret != 0) {
                ESP_LOGE(OCR_TAG, "send frame[%u] fail: %d", fuid - 1, ret);
                pending_fb_release(fb_addr);
            }
        }

        uint32_t elapsed_ms = (cap_end_tick - cap_start_tick) * portTICK_PERIOD_MS;
        uint32_t fps_x10 = elapsed_ms > 0 ? (fuid * 10000 / elapsed_ms) : 0;
        ESP_LOGI(OCR_TAG, "capture done: frames=%u, time=%ums, fps=%u.%u",
                 fuid, elapsed_ms, fps_x10 / 10, fps_x10 % 10);

        if (s_img_save_enabled) {
            /* Notify write task: close raw file */
            img_save_msg_t end_msg = { .msg_type = IMG_SAVE_MSG_RAW_END };
            xQueueSend(s_img_save_queue, &end_msg, portMAX_DELAY);
        }
    }
}

#else /* !CONFIG_LISA_CAMERA_DEVICE — eMMC mock path */

static int lis_ocr_read_emmc_image(uint8_t *dst, int index)
{
    if (!s_sdmmc_dev) {
        s_sdmmc_dev = lisa_device_get("sdmmc0");
        if (!s_sdmmc_dev) {
            ESP_LOGE(OCR_TAG, "sdmmc0 device not found");
            return -1;
        }
    }

    uint32_t byte_addr = IMAGE_EMMC_ADDR + (SCAN_IMAGE_SIZE * index);
    uint32_t sector = byte_addr / SECTOR_SIZE;
    uint32_t sector_count = (SCAN_IMAGE_SIZE + SECTOR_SIZE - 1) / SECTOR_SIZE;

    int ret = lisa_sdmmc_read(s_sdmmc_dev, dst, sector, sector_count);
    if (ret != 0) {
        ESP_LOGE(OCR_TAG, "sdmmc read fail: sector=%u count=%u ret=%d", sector, sector_count, ret);
        return -1;
    }
    return 0;
}

#endif /* CONFIG_LISA_CAMERA_DEVICE */

/* Resource indices must match AP side cv_algo_res_type_e */
#define CV_RES_STITCH   0   /* CV_ALGO_STITCH_MODEL — no model file needed */
#define CV_RES_CUTLINE  1   /* CV_ALGO_RES_CUTLINE_MODEL */
#define CV_RES_OCR      2   /* CV_ALGO_RES_OCR_MODEL */
#define CV_RES_COUNT    3   /* CV_ALGO_RES_MAX_CNT */
#define CUTLINE_SIZE    462320

static int lis_ocr_cv_prepare(void)
{
    uint32_t size = sizeof(acomp_ipc_prepare_t) + (sizeof(acomp_res_item_t) * CV_RES_COUNT);
    acomp_ipc_prepare_t *prepare = os_mem_alloc(size);
    if (!prepare) {
        ESP_LOGE(OCR_TAG, "alloc prepare fail");
        return -1;
    }

    memset(prepare, 0, size);
    prepare->number = CV_RES_COUNT;

    /* Stitch model: no external resource needed, send empty placeholder */
    prepare->item[CV_RES_STITCH].index = CV_RES_STITCH;

    /* Cutline model */
    prepare->item[CV_RES_CUTLINE].index = CV_RES_CUTLINE;
#ifdef CONFIG_ACOMP_RESMGR
    /* Try flash resmgr first, fallback to eMMC */
    uint32_t cutline_flash_size = CUTLINE_SIZE;
    void *cutline_flash_addr = resmgr_get_item(RES_CV_CUTLINE, &cutline_flash_size);
    if (cutline_flash_addr && cutline_flash_size > 0) {
        prepare->item[CV_RES_CUTLINE].attr.hdr.storage = RES_STORAGE_FLASH;
        prepare->item[CV_RES_CUTLINE].addr = (uint32_t)(uintptr_t)cutline_flash_addr;
        prepare->item[CV_RES_CUTLINE].size = cutline_flash_size;
        LOGI("cutline: flash XIP addr=%p size=%u",
                 cutline_flash_addr, cutline_flash_size);
    } else
#endif
    {
        prepare->item[CV_RES_CUTLINE].attr.hdr.storage = RES_STORAGE_SD;
        prepare->item[CV_RES_CUTLINE].addr = CUTLINE_EMMC_ADDR;
        prepare->item[CV_RES_CUTLINE].size = CUTLINE_RES_SIZE;
        LOGI("cutline: eMMC addr=0x%x size=%u",
                 CUTLINE_EMMC_ADDR, CUTLINE_RES_SIZE);
    }

    /* OCR model: always from eMMC (too large for flash) */
    prepare->item[CV_RES_OCR].index = CV_RES_OCR;
    prepare->item[CV_RES_OCR].attr.hdr.storage = RES_STORAGE_SD;
    prepare->item[CV_RES_OCR].addr = OCR_THINKER_EMMC_ADDR;
    prepare->item[CV_RES_OCR].size = OCR_THINKER_RES_SIZE;
    LOGI("ocr: eMMC addr=0x%x size=%u",
             OCR_THINKER_EMMC_ADDR, OCR_THINKER_RES_SIZE);

    int ret = acomp_cv_prepare(prepare);
    os_mem_free(prepare);
    return ret;
}

lis_err_t lis_ocr_set_scan_mode(scan_mode_e mode)
{
    ocr.mode = mode;
    if (!ocr.ocr_st) {
        /* CV 未初始化，仅缓存模式值，init 后生效 */
        return lis_err_ok;
    }
    int ret = acomp_cv_set_scan_mode((uint8_t)mode);
    if (ret != 0) {
        ESP_LOGE(OCR_TAG, "lis_ocr_set_scan_mode fail");
        return lis_err_err;
    }
    return lis_err_ok;
}

lis_err_t lis_ocr_set_scan_led(scan_led_e onoff, int level)
{
#ifdef CONFIG_LISA_CAMERA_DEVICE
    if (level > 0 && level <= 16) {
        if (s_led_brightness != (uint8_t)level) {
            s_led_brightness = (uint8_t)level;
            /* 亮度变更时重置状态，使 switch 重新应用 */
            s_led_last_state = -1;
        }
    }
    scan_led_switch(onoff == SCAN_LED_ON);
    return lis_err_ok;
#else
    (void)onoff;
    (void)level;
    return lis_err_ok;
#endif
}

/* TODO: stub - waiting for AP-side sub-command support */
lis_err_t lis_ocr_sensor_set_reg(int reg, int val)
{
    ESP_LOGW(OCR_TAG, "lis_ocr_sensor_set_reg: stub, not yet implemented in new arch");
    return lis_err_ok;
}

/* TODO: stub - waiting for AP-side sub-command support */
lis_err_t lis_ocr_sensor_get_reg(int reg, int *val)
{
    ESP_LOGW(OCR_TAG, "lis_ocr_sensor_get_reg: stub, not yet implemented in new arch");
    if (val) *val = 0;
    return lis_err_ok;
}

/* TODO: acomp_cv.h does not expose set_roi API yet */
lis_err_t lis_stitch_set_roi(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    ESP_LOGW(OCR_TAG, "lis_stitch_set_roi: stub, not yet implemented in new arch");
    return lis_err_ok;
}

/* TODO: stub - waiting for AP-side sub-command support */
lis_err_t lis_ocr_set_exposure(uint8_t val)
{
    ESP_LOGW(OCR_TAG, "lis_ocr_set_exposure: stub, not yet implemented in new arch");
    return lis_err_ok;
}

lis_err_t lis_ocr_get_sys_startup(int *val)
{
    if (val) *val = s_start_up_type;
    return lis_err_ok;
}

static void img_save_cleanup_old(void)
{
    DIR *dir = opendir(IMG_SAVE_DIR);
    if (!dir) return;

    struct dirent *ent;
    char subdir[40];
    char path[80];
    while ((ent = readdir(dir)) != NULL) {
        if (strlen(ent->d_name) == 3 &&
            ent->d_name[0] >= '0' && ent->d_name[0] <= '9' &&
            ent->d_name[1] >= '0' && ent->d_name[1] <= '9' &&
            ent->d_name[2] >= '0' && ent->d_name[2] <= '9') {
            snprintf(subdir, sizeof(subdir), "%s/%s", IMG_SAVE_DIR, ent->d_name);
            DIR *sub = opendir(subdir);
            if (sub) {
                struct dirent *f;
                while ((f = readdir(sub)) != NULL) {
                    snprintf(path, sizeof(path), "%s/%s", subdir, f->d_name);
                    remove(path);
                }
                closedir(sub);
            }
            remove(subdir);
        }
    }
    closedir(dir);
}

lis_err_t lis_ocr_start(void)
{
    static bool s_cleanup_done = false;
    if (!s_cleanup_done && ocr.mode == e_scan_mode_debug) {
        s_cleanup_done = true;
        img_save_cleanup_old();
    }

#ifdef CONFIG_LISA_CAMERA_DEVICE
    if (s_capture_running) {
        ESP_LOGW(OCR_TAG, "already running");
        return lis_err_busy;
    }

    /* 开启扫描灯 */
    scan_led_switch(true);

    int ret = acomp_cv_start();
    if (ret != 0) {
        ESP_LOGE(OCR_TAG, "cv start fail: %d", ret);
        scan_led_switch(false);
        return lis_err_err;
    }

    ret = lisa_camera_start(s_camera_dev);
    if (ret != 0) {
        ESP_LOGE(OCR_TAG, "camera start fail: %d", ret);
        acomp_cv_stop();
        scan_led_switch(false);
        return lis_err_err;
    }

    s_capture_running = true;
    xTaskNotifyGive(s_capture_task_hdl);

    ESP_LOGI(OCR_TAG, "ocr started");
    return lis_err_ok;
#else
    xEventGroupSetBits(scan_key_evt_hdl, SCAN_EVT_START);
    int ret = acomp_cv_start();
    if (ret != 0) {
        ESP_LOGE(OCR_TAG, "lis_ocr_start fail");
        return lis_err_err;
    }
    return lis_err_ok;
#endif
}

lis_err_t lis_ocr_stop(void)
{
#ifdef CONFIG_LISA_CAMERA_DEVICE
    if (!s_capture_running) {
        return lis_err_ok;
    }

    /* 1. 通知捕获任务退出循环 */
    s_capture_running = false;

    /* 2. 停止摄像头，capture 会立即返回错误，捕获循环退出；同时释放 DMA 通道 0 */
    lisa_camera_stop(s_camera_dev);

    /* 3. 等待捕获循环退出（camera stop 后 capture 立即失败，任务回到 NotifyTake 等待） */
    vTaskDelay(pdMS_TO_TICKS(50));

    /* 4. 发送 END 帧通知 AP 侧开始 CV 处理（fb_addr=0 表示无像素数据） */
    lis_ocr_send_frame(0, 0, OCR_IMG_WIDTH, OCR_IMG_HEIGHT, CV_STATUS_END);

    /* 4.5 释放所有未归还的 pending FBs */
    pending_fb_release_all();

    /* 5. 关闭扫描灯 */
    scan_led_switch(false);

    /* 6. 通知 AP 侧停止接收新帧（不影响已排队的 CV 推理处理） */
    acomp_cv_stop();

    ESP_LOGI(OCR_TAG, "ocr stopped");
    return lis_err_ok;
#else
    /* 通知 mock 任务停止发帧 */
    xEventGroupSetBits(scan_key_evt_hdl, SCAN_EVT_STOP);
    int ret = acomp_cv_stop();
    if (ret != 0) {
        ESP_LOGE(OCR_TAG, "lis_ocr_stop fail");
        return lis_err_err;
    }
    return lis_err_ok;
#endif
}

lis_ocr_status lis_ocr_get_status(void)
{
    return ocr.status;
}

/* TODO: stub - waiting for AP-side sub-command support */
int lis_ocr_get_scan_key_duration(void)
{
    ESP_LOGW(OCR_TAG, "lis_ocr_get_scan_key_duration: stub, not yet implemented in new arch");
    return 0;
}

lis_err_t lis_ocr_set_handmode(lis_ocr_handmode mode)
{
    ocr.handmode = mode;
    if (!ocr.ocr_st) {
        /* CV 未初始化，仅缓存模式值，init 后生效 */
        return lis_err_ok;
    }
    int ret = acomp_cv_set_lr_mode((uint8_t)mode);
    if (ret != 0) {
        ESP_LOGE(OCR_TAG, "lis_ocr_set_handmode fail");
        return lis_err_err;
    }
    return lis_err_ok;
}

/* TODO: stub - waiting for AP-side sub-command support */
lis_err_t lis_ocr_get_handmode(lis_ocr_handmode *mode)
{
    ESP_LOGW(OCR_TAG, "lis_ocr_get_handmode: stub, not yet implemented in new arch");
    if (mode) *mode = LIS_OCR_MODE_RIGHT_HAND;
    return lis_err_ok;
}

/* stub - not supported in new architecture yet */
lis_err_t lis_ocr_get_perframe_image(char *buf, int *buf_size)
{
    if (buf_size) *buf_size = 0;
    return lis_err_ok;
}

/* stub - not supported in new architecture yet */
lis_err_t lis_ocr_get_stitched_image(char *buf, int *buf_size)
{
    if (buf_size) *buf_size = 0;
    return lis_err_ok;
}

void lis_ocr_result_callback(void *rslt)
{
    ESP_LOGI(OCR_TAG, "result_callback: rslt=%p, cb=%p, txt=%.32s",
             rslt, g_ocr_rslt_cb, rslt ? (char *)rslt : "(null)");
    if (g_ocr_rslt_cb && rslt)
        g_ocr_rslt_cb(rslt, strlen(rslt));
}

int32_t lis_ocr_result_register(ocr_result_callback_t cb)
{
    if (cb) g_ocr_rslt_cb = cb;
    return 0;
}

char *lis_ocr_get_result(void)
{
    return ocr.result;
}

#ifndef CONFIG_LISA_CAMERA_DEVICE
static void lis_ocr_mock_task(void *param);
#endif

/*
 * 初始化拆分为两阶段，使camera硬件初始化（~113ms I2C sensor寄存器写入）
 * 与AP侧启动并行执行，节省~55ms：
 *
 * lis_ocr_init_hw(): Phase 1 — 纯硬件初始化（不依赖IPC）
 * lis_ocr_init():    Phase 2+3 — IPC调用 + stream通道
 */

static bool s_hw_init_done = false;

lis_err_t lis_ocr_init_hw(void)
{
    if (s_hw_init_done) return lis_err_ok;

    memset(&ocr, 0, sizeof(ocr));
    ocr.mode = e_scan_mode_singleline;

    ocr.result = os_mem_alloc(RESULT_SIZE);
    if (!ocr.result) {
        ESP_LOGE(OCR_TAG, "lis_ocr_init_hw fail: alloc result");
        return lis_err_err;
    }

    int ret = scan_led_init();
    if (ret != 0) {
        ESP_LOGW(OCR_TAG, "scan led init fail: %d (non-fatal)", ret);
    }

#ifdef CONFIG_LISA_CAMERA_DEVICE
    scankey_gpio_check();

    ret = lis_ocr_camera_init();
    if (ret != 0) {
        ESP_LOGE(OCR_TAG, "camera init fail: %d", ret);
        return lis_err_err;
    }
#endif

    s_hw_init_done = true;

    return lis_err_ok;
}

lis_err_t lis_ocr_init(void)
{
    /* 确保 Phase 1 已执行（兼容不拆分调用的场景） */
    lis_err_t hw_ret = lis_ocr_init_hw();
    if (hw_ret != lis_err_ok) return hw_ret;

    int ret;

#ifdef CONFIG_ACOMP_RESMGR
    int resmgr_ret = resmgr_init(CONFIG_ACOMP_RESMGR_FLASH_ADDR);
    if (resmgr_ret != 0) {
        ESP_LOGW(OCR_TAG, "resmgr_init failed (ret=%d), will use eMMC for all models", resmgr_ret);
    }
#endif

    ret = acomp_cv_init();
    if (ret != 0) {
        ESP_LOGE(OCR_TAG, "lis_ocr_init fail: acomp_cv_init %d", ret);
        goto err_free;
    }

    ret = acomp_cv_add_callback(CV_CB_EVENT_OCR_RESULT | CV_CB_EVENT_STATUS | CV_CB_EVENT_FRAME_DONE | CV_CB_EVENT_IMG_SAVE,
                                ocr_event_cb, NULL);
    if (ret != 0) {
        ESP_LOGE(OCR_TAG, "lis_ocr_init fail: add_callback %d", ret);
        goto err_free;
    }

    ret = lis_ocr_cv_prepare();
    if (ret != 0) {
        ESP_LOGE(OCR_TAG, "lis_ocr_init fail: cv_prepare %d", ret);
        goto err_free;
    }

    /* Notify AP of boot type for OCR model loading strategy */
    acomp_cv_set_boot_type(s_start_up_type == SYSTEM_START_UP_TYPE_SCAN_KEY ? 1 : 0);


    ret = lis_ocr_stream_init();
    if (ret != 0) {
        ESP_LOGE(OCR_TAG, "stream init fail: %d", ret);
        goto err_free;
    }

    /* Image save: start async write task */
    s_img_save_queue = xQueueCreate(IMG_SAVE_QUEUE_LEN, sizeof(img_save_msg_t));
    if (!s_img_save_queue) {
        ESP_LOGE(OCR_TAG, "img_save queue create fail");
        goto err_free;
    }
    xTaskCreate(img_save_write_task, "img_save", IMG_SAVE_TASK_STACK, NULL, 5, &s_img_save_task_hdl);
    if (!s_img_save_task_hdl) {
        ESP_LOGE(OCR_TAG, "img_save task create fail");
        goto err_free;
    }

#ifdef CONFIG_LISA_CAMERA_DEVICE
    BaseType_t xret = xTaskCreate(lis_ocr_capture_task, "ocr_cap", 1024, NULL,
                                  configMAX_PRIORITIES - 1, &s_capture_task_hdl);
    if (xret != pdPASS) {
        ESP_LOGE(OCR_TAG, "create capture task fail");
        goto err_free;
    }
#else
    BaseType_t xret = xTaskCreate(lis_ocr_mock_task, "ocr_mock", 1024, NULL,
                                  configMAX_PRIORITIES - 2, &s_capture_task_hdl);
    if (xret != pdPASS) {
        ESP_LOGE(OCR_TAG, "create capture task fail");
        goto err_free;
    }
#endif

    ocr.ocr_st = 1;
    ESP_LOGI(OCR_TAG, "lis_ocr_init ok");

#ifdef CONFIG_LISA_CAMERA_DEVICE
    if (s_start_up_type == SYSTEM_START_UP_TYPE_SCAN_KEY) {
        lis_ocr_start();
    }
#endif

    return lis_err_ok;

err_free:
    if (s_img_save_task_hdl) {
        vTaskDelete(s_img_save_task_hdl);
        s_img_save_task_hdl = NULL;
    }
    if (s_img_save_queue) {
        vQueueDelete(s_img_save_queue);
        s_img_save_queue = NULL;
    }
    os_mem_free(ocr.result);
    ocr.result = NULL;
    return lis_err_err;
}

void lis_ocr_deinit(void)
{
    if (ocr.ocr_st) {
        lis_ocr_stop();
        ocr.ocr_st = 0;
        if (s_capture_task_hdl) {
            vTaskDelete(s_capture_task_hdl);
            s_capture_task_hdl = NULL;
        }
#ifndef CONFIG_LISA_CAMERA_DEVICE
        if (scan_key_evt_hdl) {
            vEventGroupDelete(scan_key_evt_hdl);
            scan_key_evt_hdl = NULL;
        }
#endif
        if (s_img_save_task_hdl && s_img_save_queue) {
            /* Send sentinel to let task exit gracefully */
            img_save_msg_t sentinel = { .msg_type = IMG_SAVE_MSG_EXIT };
            xQueueSend(s_img_save_queue, &sentinel, portMAX_DELAY);
            /* Wait for task to finish current fwrite and exit */
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        if (s_img_save_task_hdl) {
            /* Fallback: force delete if task didn't exit */
            vTaskDelete(s_img_save_task_hdl);
            s_img_save_task_hdl = NULL;
        }
        if (s_img_save_queue) {
            vQueueDelete(s_img_save_queue);
            s_img_save_queue = NULL;
        }
        acomp_cv_remove_callback(ocr_event_cb);
        acomp_cv_cleanup();
        os_mem_free(ocr.result);
        ocr.result = NULL;
    }
    ESP_LOGW(OCR_TAG, "lis_ocr_deinit ok");
}

#ifndef CONFIG_LISA_CAMERA_DEVICE
static void lis_ocr_mock_task(void *param)
{
    /* eMMC mock 路径：事件驱动循环，用于验证 CV 算法 */
    scan_key_evt_hdl = xEventGroupCreate();

    uint8_t *img_buf = psram_malloc(SCAN_IMAGE_SIZE);
    if (!img_buf) {
        ESP_LOGE(OCR_TAG, "alloc img_buf fail");
        vTaskDelete(NULL);
        return;
    }

    bool running = false;
    int ret;

    while (1) {
        EventBits_t evt_bits = xEventGroupWaitBits(scan_key_evt_hdl,
            SCAN_EVT_START | SCAN_EVT_STOP | SCAN_EVT_RESULT, false, false, portMAX_DELAY);

        if (evt_bits & SCAN_EVT_START) {
            xEventGroupClearBits(scan_key_evt_hdl, SCAN_EVT_START);

            if (running) {
                ESP_LOGW(OCR_TAG, "already running, ignore start");
                continue;
            }

            ESP_LOGI(OCR_TAG, "=== CV eMMC test start ===");
            running = true;

            xEventGroupClearBits(scan_key_evt_hdl, SCAN_EVT_STOP);

            int send_retries = 0;
            bool stopped_externally = false;
            for (int i = 0; i < IMAGE_MAX_COUNT; i++) {

                /* 检查是否被外部 lis_ocr_stop() 中断 */
                EventBits_t stop_bits = xEventGroupGetBits(scan_key_evt_hdl);
                if (stop_bits & SCAN_EVT_STOP) {
                    ESP_LOGI(OCR_TAG, "mock stopped externally at frame[%d]", i);
                    stopped_externally = true;
                    break;
                }

                if (lis_ocr_read_emmc_image(img_buf, i) != 0) {
                    ESP_LOGE(OCR_TAG, "read image[%d] fail", i);
                    break;
                }

                uint32_t status;
                if (i == 0) {
                    status = CV_STATUS_BEGIN;
                } else if (i == IMAGE_MAX_COUNT - 1) {
                    status = CV_STATUS_END;
                } else {
                    status = CV_STATUS_CONTINUE;
                }

                ret = lis_ocr_send_frame((uint32_t)(uintptr_t)img_buf, i, OCR_IMG_WIDTH, OCR_IMG_HEIGHT, status);
                if (ret != 0) {
                    if (++send_retries >= 3) {
                        ESP_LOGE(OCR_TAG, "frame[%d] send retries exhausted", i);
                        break;
                    }
                    ESP_LOGE(OCR_TAG, "send frame[%d] fail: %d, retry %d", i, ret, send_retries);
                    vTaskDelay(pdMS_TO_TICKS(10));
                    i--;  /* retry */
                    continue;
                }
                send_retries = 0;

                ESP_LOGD(OCR_TAG, "sent frame[%d] status=%u", i, status);
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            if (!stopped_externally) {
                /* 自然结束，需要主动停止 CV */
                acomp_cv_stop();
            }
            running = false;
            xEventGroupClearBits(scan_key_evt_hdl, SCAN_EVT_START | SCAN_EVT_STOP);
            vTaskDelay(pdMS_TO_TICKS(100));
            xEventGroupSetBits(scan_key_evt_hdl, SCAN_EVT_RESULT);

        } else if (evt_bits & SCAN_EVT_STOP) {
            xEventGroupClearBits(scan_key_evt_hdl, SCAN_EVT_STOP);
            /* 外部已调用 lis_ocr_stop()（含 acomp_cv_stop），此处仅清理状态 */
            running = false;

        } else if (evt_bits & SCAN_EVT_RESULT) {
            lis_ocr_status ocr_status = lis_ocr_get_status();
            if (ocr_status != LIS_OCR_STATE_RESULT) {
                vTaskDelay(pdMS_TO_TICKS(10));
            } else {
                xEventGroupClearBits(scan_key_evt_hdl, SCAN_EVT_RESULT);
                char *rslt = lis_ocr_get_result();
                ESP_LOGI(OCR_TAG, "=== OCR result: %s ===", rslt);
            }
        }
    }

    psram_free(img_buf);
    lis_ocr_deinit();
}
#endif

// void lis_ocr_task(void)
// {
//     lis_ocr_init();
// }
