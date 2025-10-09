#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "lis_ocr.h"
#include "lis_algo.h"
#include "log_print.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"


#define EV_result_READY (1 << 0) // result ready

#define WAIT_PIC_TIME (pdMS_TO_TICKS(3000)) // ms
#define WAIT_CTRL_ACK (pdMS_TO_TICKS(1500))  // ms
#define RESULT_SIZE (1024)
#define OCR_WIDTH (180)
#define OCR_HEIGHT (128)
#define CAM_WIDTH (240)
#define CAM_HEIGHT (320)
#define OCR_DEPTH (1)                                    //
#define OCR_RX_SIZE (OCR_WIDTH * OCR_HEIGHT * OCR_DEPTH) //
#define CAM_RX_SIZE (CAM_WIDTH * CAM_HEIGHT * OCR_DEPTH)

#define SCAN_KEY_PAD            CSK_IOMUX_PAD_B
#define SCAN_KEY_PIN            8

#define SCAN_EVT_START          (1 << 0)
#define SCAN_EVT_STOP           (1 << 1)
#define SCAN_EVT_RESULT         (1 << 2)

static struct
{
    int ocr_st;
    scan_mode_e mode;
    uint32_t handle;
    char *result;
    SemaphoreHandle_t ctrl_sem;
} ocr;

static const char OCR_TAG[] = "ocr";
static EventGroupHandle_t scan_key_evt_hdl;

static void _scan_key_event(uint32_t event, void* workspace)
{
    BaseType_t yield = pdFALSE;
    if (event & (1 << SCAN_KEY_PIN)) {
        if (GPIO_PinRead(GPIOB(), 1 << SCAN_KEY_PIN)) {
            xEventGroupSetBitsFromISR(scan_key_evt_hdl, SCAN_EVT_STOP, &yield);
        }
        else {
            xEventGroupSetBitsFromISR(scan_key_evt_hdl, SCAN_EVT_START, &yield);
        }
    }
}

// 模式配置
lis_err_t lis_ocr_set_scan_mode(scan_mode_e mode)
{
    lis_err_t ret = lis_err_ok;
    func_ack_t ocr_ioctl_ack;
    func_descriptors_t ocr_ctrl = {
        .func = OCR_IOCTL_SET,
        .ocr.arg = OCR_SCAN_MODE,
        .ocr.value = mode,
    };

    xSemaphoreTake(ocr.ctrl_sem, portMAX_DELAY);
    if (lsf_cp2ap_func(ocr.handle, (uint8_t *)&ocr_ctrl, sizeof(func_descriptors_t),
                                 (uint8_t *)&ocr_ioctl_ack, sizeof(func_ack_t), WAIT_CTRL_ACK) > 0)
    {
        ocr.mode = mode;
    }
    else
    {
        ret = lis_err_err;
        ESP_LOGE(OCR_TAG, "lis_ocr_set_scan_mode fail");
    }
    xSemaphoreGive(ocr.ctrl_sem);
    return ret;
}

lis_err_t lis_ocr_set_scan_led(scan_led_e val)
{
    lis_err_t ret = lis_err_ok;
    func_ack_t ocr_ioctl_ack;
    func_descriptors_t ocr_ctrl = {
        .func = OCR_IOCTL_SET,
        .ocr.arg = OCR_SCAN_LED,
        .ocr.value = val,
    };

    xSemaphoreTake(ocr.ctrl_sem, portMAX_DELAY);
    if (lsf_cp2ap_func(ocr.handle, (uint8_t *)&ocr_ctrl, sizeof(func_descriptors_t),
                                 (uint8_t *)&ocr_ioctl_ack, sizeof(func_ack_t), WAIT_CTRL_ACK) > 0)
    {
        ocr.mode = val;
    }
    else
    {
        ret = lis_err_err;
        ESP_LOGE(OCR_TAG, "lis_ocr_set_scan_mode fail");
    }
    xSemaphoreGive(ocr.ctrl_sem);
    return ret;
}

lis_err_t lis_stitch_set_roi(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    lis_err_t ret = lis_err_ok;
    func_ack_t ocr_ioctl_ack;
    func_descriptors_t ocr_ctrl = {
        .func = OCR_IOCTL_SET,
        .ocr.arg = OCR_STITCH_ROI,
        .ocr.ocr_roi = {
            .x = x,
            .y = y,
            .w = w,
            .h = h,
        },
    };

    xSemaphoreTake(ocr.ctrl_sem, portMAX_DELAY);
    if (lsf_cp2ap_func(ocr.handle, (uint8_t *)&ocr_ctrl, sizeof(func_descriptors_t),
                                 (uint8_t *)&ocr_ioctl_ack, sizeof(func_ack_t), WAIT_CTRL_ACK) < 0)
    {
        ret = lis_err_err;
        ESP_LOGE(OCR_TAG, "lis_stitch_set_roi fail");
    }
    xSemaphoreGive(ocr.ctrl_sem);
    return ret;
}

lis_err_t lis_ocr_set_exposure(uint8_t val)
{
    lis_err_t ret = lis_err_ok;
    func_ack_t ocr_ioctl_ack;
    func_descriptors_t ocr_ctrl = {
        .func = OCR_IOCTL_SET,
        .ocr.arg = OCR_SENSOR_GAIN,
        .ocr.value = val,
    };

    CLOGD("lis_ocr_set_exposure val:%d", val);

    xSemaphoreTake(ocr.ctrl_sem, portMAX_DELAY);
    if (lsf_cp2ap_func(ocr.handle, (uint8_t *)&ocr_ctrl, sizeof(func_descriptors_t),
                                 (uint8_t *)&ocr_ioctl_ack, sizeof(func_ack_t), WAIT_CTRL_ACK) < 0)
    {
        ret = lis_err_err;
        ESP_LOGE(OCR_TAG, "lis_stitch_set_roi fail");
    }
    xSemaphoreGive(ocr.ctrl_sem);

    return ret;
}

lis_err_t lis_ocr_get_sys_startup(int *val)
{
    lis_err_t ret = lis_err_ok;
    func_ack_t ocr_ioctl_ack;
    func_descriptors_t ocr_ctrl = {
        .func = OCR_IOCTL_SYS_START_UP,
    };

    xSemaphoreTake(ocr.ctrl_sem, portMAX_DELAY);
    if (lsf_cp2ap_func(ocr.handle, (uint8_t *)&ocr_ctrl, sizeof(func_descriptors_t),
                                 (uint8_t *)&ocr_ioctl_ack, sizeof(func_ack_t), WAIT_CTRL_ACK) < 0)
    {
        ret = lis_err_err;
        ESP_LOGE(OCR_TAG, "lis_ocr_start fail");
    }
    *val = ocr_ioctl_ack.status;
    xSemaphoreGive(ocr.ctrl_sem);
    return ret;
}

// 产测或调试接口
lis_err_t lis_ocr_start(void)
{
    lis_err_t ret = lis_err_ok;
    func_ack_t ocr_ioctl_ack;
    func_descriptors_t ocr_ctrl = {
        .func = OCR_IOCTL_START,
    };

    xSemaphoreTake(ocr.ctrl_sem, portMAX_DELAY);
    if (lsf_cp2ap_func(ocr.handle, (uint8_t *)&ocr_ctrl, sizeof(func_descriptors_t),
                                 (uint8_t *)&ocr_ioctl_ack, sizeof(func_ack_t), WAIT_CTRL_ACK) < 0)
    {
        ret = lis_err_err;
        ESP_LOGE(OCR_TAG, "lis_ocr_start fail");
    }
    xSemaphoreGive(ocr.ctrl_sem);
    return ret;
}
lis_err_t lis_ocr_stop(void)
{
    lis_err_t ret = lis_err_ok;
    func_ack_t ocr_ioctl_ack;
    func_descriptors_t ocr_ctrl = {
        .func = OCR_IOCTL_STOP,
    };

    xSemaphoreTake(ocr.ctrl_sem, portMAX_DELAY);
    if (lsf_cp2ap_func(ocr.handle, (uint8_t *)&ocr_ctrl, sizeof(func_descriptors_t),
                                 (uint8_t *)&ocr_ioctl_ack, sizeof(func_ack_t), WAIT_CTRL_ACK) < 0)
    {
        ret = lis_err_err;
        ESP_LOGE(OCR_TAG, "lis_ocr_stop fail");
    }
    xSemaphoreGive(ocr.ctrl_sem);
    return ret;
}
// 获取ocr的状态
lis_ocr_status lis_ocr_get_status(void)
{
    lis_ocr_status st = LIS_OCR_STATE_IDLE;
    func_ack_t ocr_ioctl_ack;
    func_descriptors_t ocr_ctrl = {
        .func = OCR_IOCTL_GET,
        .ocr.arg = OCR_STATUS,
    };

    xSemaphoreTake(ocr.ctrl_sem, portMAX_DELAY);
    if (lsf_cp2ap_func(ocr.handle, (uint8_t *)&ocr_ctrl, sizeof(func_descriptors_t),
                                 (uint8_t *)&ocr_ioctl_ack, sizeof(func_ack_t), WAIT_CTRL_ACK) > 0)
    {
        st = ocr_ioctl_ack.status;
    }
    else
    {
        st = LIS_OCR_STATE_ERR;
        ESP_LOGE(OCR_TAG, "lis_ocr_get_status fail");
    }
    xSemaphoreGive(ocr.ctrl_sem);
    return st;
}

// 获取ocr 扫描按键时间
int lis_ocr_get_scan_key_duration(void)
{
    int ret = 0;
    func_ack_t ocr_ioctl_ack;
    func_descriptors_t ocr_ctrl = {
        .func = OCR_IOCTL_GET,
        .ocr.arg = OCR_SCAN_KEY_DURATION,
    };

    xSemaphoreTake(ocr.ctrl_sem, portMAX_DELAY);
    if (lsf_cp2ap_func(ocr.handle, (uint8_t *)&ocr_ctrl, sizeof(func_descriptors_t),
                                 (uint8_t *)&ocr_ioctl_ack, sizeof(func_ack_t), WAIT_CTRL_ACK) > 0)
    {
        ret = ocr_ioctl_ack.status;
    }
    else
    {
        ret = -1;
        ESP_LOGE(OCR_TAG, "lis_ocr_get_status fail");
    }
    xSemaphoreGive(ocr.ctrl_sem);
    return ret;
}

// 设置OCR扫描左右手模式,默认右手模式
lis_err_t lis_ocr_set_handmode(lis_ocr_handmode mode)
{
    lis_err_t ret = lis_err_ok;
    func_ack_t ocr_ioctl_ack;
    func_descriptors_t ocr_ctrl = {
        .func = OCR_IOCTL_SET,
        .ocr.arg = OCR_HANDMODE,
        .ocr.value = mode,
    };
    xSemaphoreTake(ocr.ctrl_sem, portMAX_DELAY);
    if (lsf_cp2ap_func(ocr.handle, (uint8_t *)&ocr_ctrl, sizeof(func_descriptors_t),
                                 (uint8_t *)&ocr_ioctl_ack, sizeof(func_ack_t), WAIT_CTRL_ACK) < 0)
    {
        ret = lis_err_err;
        ESP_LOGE(OCR_TAG, "lis_ocr_set_handmode fail");
    }
    xSemaphoreGive(ocr.ctrl_sem);
    return ret;
}

lis_err_t lis_ocr_get_handmode(lis_ocr_handmode *mode)
{
    lis_err_t ret = lis_err_ok;
    func_ack_t ocr_ioctl_ack;
    func_descriptors_t ocr_ctrl = {
        .func = OCR_IOCTL_GET,
        .ocr.arg = OCR_HANDMODE,
    };
    xSemaphoreTake(ocr.ctrl_sem, portMAX_DELAY);
    if (lsf_cp2ap_func(ocr.handle, (uint8_t *)&ocr_ctrl, sizeof(func_descriptors_t),
                                 (uint8_t *)&ocr_ioctl_ack, sizeof(func_ack_t), WAIT_CTRL_ACK) > 0)
    {
        *mode = ocr_ioctl_ack.status;
    }
    else
    {
        ret = lis_err_err;
        ESP_LOGE(OCR_TAG, "lis_ocr_get_status fail");
    }
    xSemaphoreGive(ocr.ctrl_sem);
    return ret;
}

static uint16_t data_pack_check(const void *dat, uint32_t len)
{
    if (NULL == dat)
    {
        ESP_LOGE(OCR_TAG, "image pack check null\n");
        return 0;
    }
    uint16_t check = 0;
    uint8_t *data = (uint8_t *)dat;
    while (len--)
        check += *data++;
    return check;
}

// 获取拼接前摄像头采集的每帧图片
lis_err_t lis_ocr_get_perframe_image(char *buf, int *buf_size)
{
    lis_err_t ret = lis_err_ok;
    int pic_size = e_scan_mode_adjust == ocr.mode ? CAM_RX_SIZE : OCR_RX_SIZE;
    func_descriptors_t ocr_ctrl = {
        .func = OCR_IOCTL_GET,
        .ocr.arg = e_scan_mode_adjust == ocr.mode ? OCR_IMG_320X240 : OCR_IMG_128X180,
    };

    *buf_size = 0;
    if (NULL == buf)
    {
        *buf_size = pic_size;
        ESP_LOGE(OCR_TAG, "lis_ocr_get_perframe_image fail, buf null");
        return lis_err_err;
    }
    xSemaphoreTake(ocr.ctrl_sem, portMAX_DELAY);
#if 0
    *buf_size = lsf_cp2ap_func(ocr.handle, (uint8_t *)&ocr_ctrl, sizeof(func_descriptors_t),
                                         (uint8_t *)buf, pic_size, WAIT_CTRL_ACK);
    if (*buf_size < 0)
    {
        ret = lis_err_err;
        ESP_LOGE(OCR_TAG, "lis_ocr_get_perframe_image fail");
    }

    uint16_t esp_checksum = data_pack_check(buf, *buf_size);
    uint16_t checksum = 0;
    int rx_len = spi_trans_recv_data(ocr.handle, (uint8_t *)&checksum, sizeof(uint16_t), WAIT_CTRL_ACK);
    if (rx_len < 0)
    {
        ret = lis_err_err;
        ESP_LOGE(OCR_TAG, "lis_ocr_get_perframe_image fail");
    }
    if(checksum != esp_checksum) {
        ret = lis_err_err;
        ESP_LOGE(OCR_TAG, "checksum error, csk:0x%x, esp:0x%x", checksum, esp_checksum);
    }
#endif
    xSemaphoreGive(ocr.ctrl_sem);
    return ret;
}
// 获取经过拼接算法拼接后送入OCR的图片
// TODO 拼接长度
lis_err_t lis_ocr_get_stitched_image(char *buf, int *buf_size)
{
    lis_err_t ret = lis_err_ok;

    func_descriptors_t ocr_ctrl = {
        .func = OCR_IOCTL_GET,
        .ocr.arg = OCR_IMG_STICH,
    };
    xSemaphoreTake(ocr.ctrl_sem, portMAX_DELAY);
    if (lsf_cp2ap_func(ocr.handle, (uint8_t *)&ocr_ctrl, sizeof(func_descriptors_t),
                                 (uint8_t *)buf, CAM_RX_SIZE, WAIT_CTRL_ACK) < 0)
    {
        ret = lis_err_err;
        ESP_LOGE(OCR_TAG, "lis_ocr_get_stitched_image fail");
    }
    xSemaphoreGive(ocr.ctrl_sem);
    return ret;
}


ocr_result_callback_t g_ocr_rslt_cb = NULL;
void lis_ocr_result_callback(void *rslt)
{
    if(g_ocr_rslt_cb)
        g_ocr_rslt_cb(rslt, strlen(rslt));
}

int32_t lis_ocr_result_register(ocr_result_callback_t cb)
{
    if(cb) g_ocr_rslt_cb = cb;
}

// 当lis_ocr_status_get获取到OCR_STATE_RESULT后，会调用该接口获取OCR结果
char *lis_ocr_get_result(void)
{
    func_ack_t ocr_ioctl_ack;
    func_descriptors_t ocr_ctrl = {
        .func = OCR_IOCTL_RESULT,
        .ocr.arg = OCR_RESULT,
    };

    xSemaphoreTake(ocr.ctrl_sem, portMAX_DELAY);
    if (lsf_cp2ap_func(ocr.handle, (uint8_t *)&ocr_ctrl, sizeof(func_descriptors_t),
                                 (uint8_t *)&ocr_ioctl_ack, sizeof(func_ack_t), WAIT_CTRL_ACK) < 0)
    {
        memset(ocr.result, 0, RESULT_SIZE);
        ESP_LOGE(OCR_TAG, "ocr get result fail");
    } else {
        memcpy(ocr.result, ocr_ioctl_ack.result, RESULT_SIZE);
    }
    // ESP_LOGD(OCR_TAG, "low ocr result:%s\n", ocr.result?:"");
    xSemaphoreGive(ocr.ctrl_sem);
    return ocr.result;
}

lis_err_t lis_ocr_init(void)
{
    memset(&ocr, 0, sizeof(ocr));
    ocr.ctrl_sem = xSemaphoreCreateBinary();
    ocr.handle = TYPE_OCR;
    ocr.mode = e_scan_mode_factory;
#if TAOYUN_OS
    ocr.result = os_mem_alloc(RESULT_SIZE);
#else
    ocr.result = heap_caps_malloc(RESULT_SIZE, MALLOC_CAP_SPIRAM);
#endif

    if (ocr.result && ocr.ctrl_sem)
    {
        ocr.ocr_st = 1;
        xSemaphoreGive(ocr.ctrl_sem);
        ESP_LOGI(OCR_TAG, "lis_ocr_init ok");
    }
    else
    {
        if (ocr.result)
        {
#if TAOYUN_OS
            os_mem_free(ocr.result);
#else
            heap_caps_free(ocr.result);
#endif
        }
        ESP_LOGE(OCR_TAG, "lis_ocr_init fail");
        return lis_err_err;
    }

    return lis_err_ok;
}

void lis_ocr_deinit(void)
{
    if (ocr.ocr_st)
    {
        ocr.ocr_st = 0;
#if TAOYUN_OS
        os_mem_free(ocr.result);
#else
        heap_caps_free(ocr.result);
#endif
        vSemaphoreDelete(ocr.ctrl_sem);
    }
    ESP_LOGW(OCR_TAG, "lis_ocr_deinit ok");
    return;
}

// OCR 测试任务
void lis_ocr_task(void)
{
    int count = 0;

    IOMuxManager_PinConfigure(SCAN_KEY_PAD, SCAN_KEY_PIN, CSK_IOMUX_FUNC_DEFAULT);
    GPIO_SetCallback(GPIOB(), 1 << SCAN_KEY_PIN, _scan_key_event, NULL);
    GPIO_SetDir(GPIOB(), 1 << SCAN_KEY_PIN, CSK_GPIO_DIR_INPUT);
    GPIO_Control(GPIOB(), CSK_GPIO_DEBOUNCE_DISABLE | CSK_GPIO_SET_INTR_DUAL_EDGE |
                                                CSK_GPIO_INTR_ENABLE, (1UL << SCAN_KEY_PIN));
    scan_key_evt_hdl = xEventGroupCreate();

    lis_ocr_init();

    while (1) {
        EventBits_t evt_bits = xEventGroupWaitBits(scan_key_evt_hdl, SCAN_EVT_START | SCAN_EVT_STOP | SCAN_EVT_RESULT, false, false, portMAX_DELAY);
        if (evt_bits & SCAN_EVT_START)
        {
            xEventGroupClearBits(scan_key_evt_hdl, SCAN_EVT_START);
            xEventGroupSetBits(scan_key_evt_hdl, SCAN_EVT_RESULT);
            lis_ocr_start();
        }
        else if (evt_bits & SCAN_EVT_STOP)
        {
            xEventGroupClearBits(scan_key_evt_hdl, SCAN_EVT_STOP);
            xEventGroupSetBits(scan_key_evt_hdl, SCAN_EVT_RESULT);
            lis_ocr_stop();
        }
        else if (evt_bits & SCAN_EVT_RESULT)
        {
            lis_ocr_status ocr_status = lis_ocr_get_status();
            if (ocr_status != LIS_OCR_STATE_RESULT) {
                vTaskDelay(10);
            }
            else {
                xEventGroupClearBits(scan_key_evt_hdl, SCAN_EVT_RESULT);
                char * rslt = lis_ocr_get_result();
                ESP_LOGD(OCR_TAG, "ocr rslt:%s", rslt);
            }
        }
    }
    // lis_ocr_start();
    // while (1)
    // {
    //     ESP_LOGD(OCR_TAG, "ocr status:%d", lis_ocr_get_status());
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    //     if(count++ > 3) break;
    // }
    // lis_ocr_stop();

    // while(1)
    // {
    //     if(lis_ocr_get_status() == LIS_OCR_STATE_RESULT) {
    //         char * rslt = lis_ocr_get_result();
    //         ESP_LOGD(OCR_TAG, "ocr rslt:%s", rslt);
    //         break;
    //     }
    //     vTaskDelay(10);
    // }
    lis_ocr_deinit();
}