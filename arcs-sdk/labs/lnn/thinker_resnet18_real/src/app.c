/*
 * Copyright (c) 2026 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "lnn_real_cp"

#include <IOMuxManager.h>
#include <board.h>
#include <cache.h>
#include <lisa_camera.h>
#include <lisa_device.h>
#include <lisa_display.h>
#include <lisa_log.h>
#include <lisa_thread.h>
#ifdef CONFIG_LISA_TOUCH_DEVICE
#include <lisa_touch.h>
#endif
#include <lv_port_disp.h>
#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lnn_resnet18_real_cp.h"

static lisa_device_t *display_device;
static lisa_device_t *camera_device;
#ifdef CONFIG_LISA_TOUCH_DEVICE
static lisa_device_t *touch_device;
#endif
static lv_obj_t *preview_obj;
static lv_obj_t *result_label;
static lv_obj_t *status_label;
static lv_obj_t *recognize_btn;
static lv_obj_t *preview_btn;
static lv_img_dsc_t preview_dsc;
#ifdef CONFIG_LISA_TOUCH_DEVICE
static lv_indev_drv_t touch_indev_drv;
#endif

static uint16_t preview_buf[PREVIEW_WIDTH * PREVIEW_HEIGHT]
    __attribute__((section(".psram.bss"), aligned(64)));

static volatile bool preview_dirty;

/* 帧率与耗时统计（每 5s 打印一次） */
#define FPS_STAT_WINDOW_MS 5000U
static volatile uint32_t stat_capture_cnt;
static volatile uint32_t stat_scale_ms;
static volatile uint32_t stat_scale_cnt;
static volatile uint32_t stat_flush_cnt;
static volatile uint32_t stat_lvhandler_ms;
static volatile uint32_t stat_lvhandler_cnt;
static volatile uint32_t stat_uitask_cnt;
static volatile uint32_t stat_wait_ms;
static volatile uint32_t stat_wait_max;
static volatile uint32_t stat_poll_ms;
static uint32_t stat_window_start;
static volatile bool preview_frozen;
static volatile bool recognize_requested;
static volatile bool request_in_flight;
#ifdef CONFIG_LISA_TOUCH_DEVICE
static volatile uint16_t touch_last_x;
static volatile uint16_t touch_last_y;
static volatile uint8_t touch_last_state = LV_INDEV_STATE_REL;
#endif

static uint32_t min_u32(uint32_t a, uint32_t b)
{
    return a < b ? a : b;
}

#ifdef CONFIG_LISA_TOUCH_DEVICE
static void touch_transform_coordinates(lv_coord_t *x, lv_coord_t *y)
{
    if (x == NULL || y == NULL) {
        return;
    }

#if CONFIG_LV_POINTER_SWAP_XY
    lv_coord_t cur_x = *y;
    lv_coord_t cur_y = *x;
#else
    lv_coord_t cur_x = *x;
    lv_coord_t cur_y = *y;
#endif

#if CONFIG_LV_POINTER_INVERT_X
    cur_x = (lv_coord_t)SCREEN_WIDTH - cur_x;
#endif

#if CONFIG_LV_POINTER_INVERT_Y
    cur_y = (lv_coord_t)SCREEN_HEIGHT - cur_y;
#endif

    *x = cur_x;
    *y = cur_y;
}

static void touch_interrupt_callback(const lisa_touch_event_t *event, void *user_data)
{
    (void)user_data;

    if (event != NULL && event->type == LISA_TOUCH_EVENT_PRESS && event->point_count > 0U) {
        lv_coord_t x = (lv_coord_t)event->points[0].x;
        lv_coord_t y = (lv_coord_t)event->points[0].y;

        touch_transform_coordinates(&x, &y);
        touch_last_x = (uint16_t)x;
        touch_last_y = (uint16_t)y;
        touch_last_state = LV_INDEV_STATE_PR;
    } else {
        touch_last_state = LV_INDEV_STATE_REL;
    }
}

static void touch_interrupt_read(struct _lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    (void)indev_drv;

    data->point.x = (lv_coord_t)touch_last_x;
    data->point.y = (lv_coord_t)touch_last_y;
    data->state = touch_last_state;
    data->continue_reading = 0;
}

static int touch_interrupt_init(lisa_device_t *touch_dev)
{
    int ret = lisa_touch_set_callback(touch_dev, touch_interrupt_callback, NULL);
    if (ret != LISA_DEVICE_OK) {
        LOGE("touch callback set failed: %d", ret);
        return -1;
    }

    ret = lisa_touch_set_int_mode(touch_dev, LISA_TOUCH_INT_MODE_INTERRUPT);
    if (ret != LISA_DEVICE_OK) {
        LOGE("touch interrupt mode set failed: %d", ret);
        return -1;
    }

    ret = lisa_touch_enable(touch_dev);
    if (ret != LISA_DEVICE_OK) {
        LOGE("touch enable failed: %d", ret);
        return -1;
    }

    lisa_touch_capabilities_t caps = {0};
    ret = lisa_touch_get_capabilities(touch_dev, &caps);
    if (ret == LISA_DEVICE_OK) {
        LOGI("touch caps: max_x=%u max_y=%u max_points=%u", caps.max_x, caps.max_y, caps.max_points);
    }

    touch_device = touch_dev;
    lv_indev_drv_init(&touch_indev_drv);
    touch_indev_drv.type = LV_INDEV_TYPE_POINTER;
    touch_indev_drv.read_cb = touch_interrupt_read;
    lv_indev_drv_register(&touch_indev_drv);
    LOGI("touch interrupt mode enabled");
    return 0;
}
#endif

static rgb565_window_t make_rgb565_center_window(const uint16_t *src, uint32_t width, uint32_t height)
{
    uint32_t window_width = min_u32(width, CAMERA_WINDOW_WIDTH);
    uint32_t window_height = min_u32(height, CAMERA_WINDOW_HEIGHT);
    uint32_t window_size = min_u32(window_width, window_height);

    rgb565_window_t window = {
        .pixels = src,
        .stride = width,
        .x = (width - window_size) / 2U,
        .y = (height - window_size) / 2U,
        .width = window_size,
        .height = window_size,
    };

    return window;
}

static uint16_t rgb565_window_get_pixel(const rgb565_window_t *window, uint32_t x, uint32_t y)
{
    /* 传感器相对屏幕旋转 90°（逆时针）：输出坐标 (x,y) 采样源窗口 (W-1-y, x)，
     * 预览与 NN 输入共用同一映射，识别方向与显示方向一致 */
    uint32_t sx = window->width - 1U - y;
    uint32_t sy = x;
    return window->pixels[(window->y + sy) * window->stride + window->x + sx];
}

static void scale_rgb565_to_preview(const rgb565_window_t *src)
{
    for (uint32_t y = 0; y < PREVIEW_HEIGHT; y++) {
        for (uint32_t x = 0; x < PREVIEW_WIDTH; x++) {
            uint32_t src_x = x * src->width / PREVIEW_WIDTH;
            uint32_t src_y = y * src->height / PREVIEW_HEIGHT;
            preview_buf[y * PREVIEW_WIDTH + x] = rgb565_window_get_pixel(src, src_x, src_y);
        }
    }

    HAL_FlushDCache_by_Addr((uint32_t *)preview_buf, sizeof(preview_buf));
    preview_dirty = true;
}

static void update_control_states(bool ap_ready)
{
    bool run_enabled = ap_ready && !recognize_requested && !request_in_flight && !preview_frozen;

    if (run_enabled) {
        lv_obj_clear_state(recognize_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(recognize_btn, LV_STATE_DISABLED);
    }

    if (preview_frozen) {
        lv_obj_clear_state(preview_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(preview_btn, LV_STATE_DISABLED);
    }
}

static void recognize_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (!lnn_ap_ready_for_request() || recognize_requested || request_in_flight || preview_frozen) {
        return;
    }

    recognize_requested = true;
    lv_label_set_text(status_label, "capturing...");
    lv_label_set_text(result_label, "--");
    update_control_states(false);
}

#ifndef CONFIG_LISA_TOUCH_DEVICE
/* 无触摸板型：每 2s 自动触发一次识别（预览冻结 3s 后自动恢复） */
static void auto_recognize_timer_cb(lv_timer_t *t)
{
    (void)t;

    if (request_in_flight) {
        return;
    }

    if (!lnn_ap_ready_for_request() || recognize_requested) {
        return;
    }

    recognize_requested = true;
    lv_label_set_text(status_label, "capturing...");
    lv_label_set_text(result_label, "--");
    update_control_states(false);
}
#endif

static void preview_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    preview_frozen = false;
    recognize_requested = false;
    lnn_ap_mark_status_dirty();
    update_control_states(false);
}

static lv_obj_t *create_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    preview_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    preview_dsc.header.w = PREVIEW_WIDTH;
    preview_dsc.header.h = PREVIEW_HEIGHT;
    preview_dsc.data = (uint8_t *)preview_buf;
    preview_dsc.data_size = sizeof(preview_buf);

    preview_obj = lv_img_create(screen);
    lv_obj_set_pos(preview_obj, 0, 0);
    lv_obj_set_size(preview_obj, PREVIEW_WIDTH, PREVIEW_HEIGHT);
    lv_img_set_src(preview_obj, &preview_dsc);

    lv_obj_t *panel = lv_obj_create(screen);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x202020), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 0, LV_PART_MAIN);
    lv_obj_set_pos(panel, PREVIEW_WIDTH, 0);
    lv_obj_set_size(panel, SIDE_PANEL_WIDTH, SCREEN_HEIGHT);

    status_label = lv_label_create(panel);
    lv_obj_set_width(status_label, SIDE_PANEL_WIDTH - 8);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(status_label, "waiting AP");
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 8);

    result_label = lv_label_create(panel);
    lv_obj_set_width(result_label, SIDE_PANEL_WIDTH - 8);
    lv_obj_set_style_text_color(result_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_label_set_long_mode(result_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(result_label, "--");
    lv_obj_align(result_label, LV_ALIGN_CENTER, 0, -18);

    recognize_btn = lv_btn_create(panel);
    lv_obj_set_size(recognize_btn, SIDE_PANEL_WIDTH - 10, 36);
    lv_obj_align(recognize_btn, LV_ALIGN_BOTTOM_MID, 0, -52);
    lv_obj_add_event_cb(recognize_btn, recognize_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_state(recognize_btn, LV_STATE_DISABLED);

    lv_obj_t *btn_label = lv_label_create(recognize_btn);
    lv_label_set_text(btn_label, "RUN");
    lv_obj_center(btn_label);

    preview_btn = lv_btn_create(panel);
    lv_obj_set_size(preview_btn, SIDE_PANEL_WIDTH - 10, 36);
    lv_obj_align(preview_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(preview_btn, preview_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_state(preview_btn, LV_STATE_DISABLED);

    btn_label = lv_label_create(preview_btn);
    lv_label_set_text(btn_label, "PREVIEW");
    lv_obj_center(btn_label);

    return screen;
}

static void refresh_status_from_ap(void)
{
    static uint32_t last_logged_seq = UINT32_MAX;
    static uint32_t last_logged_state = UINT32_MAX;
    lnn_ap_status_t status;

    lnn_ap_read_status(&status);
    if (status.magic != LNN_RESNET18_REAL_IPC_MAGIC) {
        request_in_flight = false;
        lv_label_set_text(status_label, "AP offline");
        update_control_states(false);
        return;
    }

    if (status.seq != last_logged_seq || status.state != last_logged_state) {
        LOGI("ap shared status seq=%u state=%u message=%s", status.seq, status.state, status.message);
        last_logged_seq = status.seq;
        last_logged_state = status.state;
    }

    switch (status.state) {
    case LNN_RESNET18_REAL_STATE_BOOTING:
        lv_label_set_text(status_label, "AP booting");
        update_control_states(false);
        break;
    case LNN_RESNET18_REAL_STATE_IDLE:
        request_in_flight = false;
        lv_label_set_text(status_label, "ready");
        update_control_states(true);
        break;
    case LNN_RESNET18_REAL_STATE_REQUEST:
        request_in_flight = true;
        lv_label_set_text(status_label, "request sent");
        update_control_states(false);
        break;
    case LNN_RESNET18_REAL_STATE_BUSY:
        request_in_flight = true;
        lv_label_set_text(status_label, "recognizing...");
        update_control_states(false);
        break;
    case LNN_RESNET18_REAL_STATE_DONE:
        request_in_flight = false;
        lv_label_set_text_fmt(status_label, "score: %d", (int)status.result_score);
        lv_label_set_text_fmt(result_label, "%s", status.result_text);
        update_control_states(true);
        break;
    case LNN_RESNET18_REAL_STATE_ERROR:
        request_in_flight = false;
        lv_label_set_text(status_label, status.message);
        update_control_states(false);
        break;
    default:
        lv_label_set_text(status_label, "unknown");
        update_control_states(false);
        break;
    }
}

void lnn_task_ui(void *arg)
{
    (void)arg;

    lv_obj_t *screen = create_screen();
    lv_scr_load(screen);
    lv_task_handler();
    lisa_display_blanking_off(display_device);
    TickType_t last_result_poll = 0;

    while (1) {
        if (preview_dirty) {
            preview_dirty = false;
            stat_flush_cnt++;
            lv_img_set_src(preview_obj, &preview_dsc);
            lv_obj_invalidate(preview_obj);
        }

        stat_uitask_cnt++;
        TickType_t now = xTaskGetTickCount();
        if (lnn_ap_take_status_dirty() || (now - last_result_poll) >= pdMS_TO_TICKS(RESULT_POLL_INTERVAL_MS)) {
            last_result_poll = now;
            uint32_t t_poll = (uint32_t)xTaskGetTickCount();
            refresh_status_from_ap();
            stat_poll_ms += (uint32_t)xTaskGetTickCount() - t_poll;
        }

        uint32_t t_lv = (uint32_t)xTaskGetTickCount();
        uint32_t wait_time = lv_task_handler();
        stat_lvhandler_ms += (uint32_t)xTaskGetTickCount() - t_lv;
        stat_lvhandler_cnt++;
        /* 睡眠上限 15ms：LVGL 返回的 time_till_next 在无脏区时可能偏大，
         * 免费跑高循环频率可及时消费 preview_dirty，预览不再卡顿 */
        if (wait_time > 15U || wait_time == 0U) {
            wait_time = (wait_time == 0U) ? 5U : 15U;
        }
        stat_wait_ms += wait_time;
        if (wait_time > stat_wait_max) {
            stat_wait_max = wait_time;
        }
        lisa_thread_mdelay(wait_time);
    }
}

int lnn_display_touch_init(void)
{
    lv_init();

    lisa_device_t *gpioa_dev = lisa_device_get("gpioa");
    lisa_device_t *gpiob_dev = lisa_device_get("gpiob");

    lisa_display_config_t display_config = {
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        /* arcs_mini: LCD 挂 spi0，CS/CD/TE 在 gpioa(PAD_A)，RST 在 gpiob，
         * 背光 PWM 通道 1 高有效（对齐产品 service_brightness 配置） */
        .bus_config = {.spi_4wire =
                           {
                               .spi_dev = lisa_device_get("spi0"),
                               .cs_gpio = gpioa_dev,
                               .cs_pin = LCD_CS_PIN,
                               .dc_gpio = gpioa_dev,
                               .dc_pin = LCD_CD_PIN,
                               .spi_freq = 50 * 1000 * 1000,
                           }},
        .te_gpio = gpioa_dev,
        .te_pin = LCD_TE_PIN,
        .backlight = {.type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
                      .blacklight_polarity = LISA_DISPLAY_BLACKLIGHT_POLARITY_HIGH,
                      .config.pwm = {.channel = 1, .dev = lisa_device_get("pwm0"), .freq = 2000}},
        .rst_gpio = gpiob_dev,
        .rst_pin = LCD_RST_PIN,
    };

    display_device = lisa_device_get("display");
    if (!display_device) {
        LOGE("failed to get display device");
        return -1;
    }

    int ret = lisa_display_attach_bus(display_device, &display_config);
    if (ret != 0) {
        LOGE("display attach bus failed: %d", ret);
        return -1;
    }
    lv_port_disp_init(display_device);

#ifdef CONFIG_LISA_TOUCH_DEVICE
    /* arcs_mini 无触摸屏：跳过触摸初始化，用定时自动识别代替按键 */
    touch_device = lisa_device_get(TOUCH_DEVICE);
    if (!lisa_device_ready(touch_device)) {
        LOGE("%s device not ready", TOUCH_DEVICE);
        return -1;
    }

    lisa_device_t *i2c_dev = lisa_device_get(I2C_DEVICE);
    if (!lisa_device_ready(i2c_dev)) {
        LOGE("%s device not ready", I2C_DEVICE);
        return -1;
    }

    lisa_touch_bus_config_t touch_config = {
        .bus_type = LISA_TOUCH_BUS_I2C,
        .config = {.i2c =
                       {
                           .i2c_dev = i2c_dev,
                           .int_gpio = gpioa_dev,
                           .int_pin = TOUCH_INT_PIN,
                           .rst_gpio = gpioa_dev,
                           .rst_pin = TOUCH_RST_PIN,
                       }},
    };

    ret = lisa_touch_attach_bus(touch_device, &touch_config);
    if (ret != 0) {
        LOGE("touch attach bus failed: %d", ret);
        return -1;
    }

    ret = touch_interrupt_init(touch_device);
    if (ret != 0) {
        return ret;
    }
#else
    lv_timer_t *auto_timer = lv_timer_create(auto_recognize_timer_cb, 2000, NULL);
    if (!auto_timer) {
        return -1;
    }
#endif

    lisa_display_set_brightness(display_device, 90);

    return 0;
}

int lnn_camera_init(void)
{
    camera_device = lisa_device_get(CAMERA_DEVICE);
    if (!lisa_device_ready(camera_device)) {
        LOGE("%s device not ready", CAMERA_DEVICE);
        return -1;
    }

    lisa_device_t *i2c_dev = lisa_device_get(I2C_DEVICE);
    if (!lisa_device_ready(i2c_dev)) {
        LOGE("%s device not ready", I2C_DEVICE);
        return -1;
    }

    lisa_camera_config_t config = {
        .hw_config = {
            .mclk_pad = CSK_IOMUX_PAD_A,
            .mclk_pin = CAM_MCLK_PIN,
#ifdef CAM_PWDN_PIN
            .pwdn_gpio_dev = lisa_device_get("gpiob"),
            .pwdn_pin = CAM_PWDN_PIN,
#else
            /* arcs_mini 板型摄像头无 PWDN 引脚 */
            .pwdn_gpio_dev = NULL,
            .pwdn_pin = -1,
#endif
            .pwdn_delay_us = 0,
            .xclk_delay_us = 0,
            .i2c_dev = i2c_dev,
        },
        .xclk_freq_hz = 18000000,
        .fb_count = 3,
        .enable_hmirror = false,
        .enable_vflip = false,
        .enable_colorbar = false,
    };

    int ret = lisa_camera_setup(camera_device, &config);
    if (ret != LISA_DEVICE_OK) {
        LOGE("camera setup failed: %d", ret);
        return -1;
    }

    ret = lisa_camera_set_pixformat(camera_device, LISA_CAMERA_PIXFMT_RGB565);
    if (ret != LISA_DEVICE_OK) {
        LOGW("set RGB565 failed: %d, use sensor default", ret);
    }

    lisa_camera_bus_config_t bus_config = {
        .dma_channel = CAMERA_DMA_CHANNEL,
        .bus_type = LISA_CAMERA_BUS_DVP,
        .config.dvp = {
            .dvp_dev = lisa_device_get(DVP_DEVICE),
            .dvp_freq = config.xclk_freq_hz,
            .data_align = 1,
            .line_offset = 0,
            .pixel_offset = 0,
            .pclk_polarity = 0,
            .vsync_polarity = 1,
            .hsync_polarity = 1,
        },
    };
    lisa_camera_get_framesize(camera_device, &bus_config.width, &bus_config.height);
    bus_config.pixel_format = lisa_camera_get_pixformat(camera_device);

    ret = lisa_camera_attach_bus(camera_device, &bus_config);
    if (ret != LISA_DEVICE_OK) {
        LOGE("camera attach bus failed: %d", ret);
        return -1;
    }

    ret = lisa_camera_start(camera_device);
    if (ret != LISA_DEVICE_OK) {
        LOGE("camera start failed: %d", ret);
        return -1;
    }

    LOGI("camera started: %ux%u format=%d", bus_config.width, bus_config.height, bus_config.pixel_format);
    return 0;
}

void lnn_task_camera(void *arg)
{
    (void)arg;

    while (1) {
        lisa_camera_fb_t *fb = NULL;
        int ret = lisa_camera_capture(camera_device, &fb);
        if (ret != LISA_DEVICE_OK || fb == NULL) {
            LOGE("capture failed: %d", ret);
            lisa_thread_mdelay(100);
            continue;
        }

        stat_capture_cnt++;
        uint32_t now = (uint32_t)xTaskGetTickCount();

        if (fb->format == LISA_CAMERA_PIXFMT_RGB565 && fb->buf != NULL) {
            rgb565_window_t window = make_rgb565_center_window((const uint16_t *)fb->buf, fb->width, fb->height);

            uint32_t t0 = (uint32_t)xTaskGetTickCount();
            if (recognize_requested) {
                scale_rgb565_to_preview(&window);
                if (lnn_ap_submit_window_for_inference(&window) == 0) {
                    recognize_requested = false;
                    request_in_flight = true;   /* 预览继续，结果异步刷新标签 */
                    lnn_ap_mark_status_dirty();
                } else {
                    recognize_requested = false;
                    preview_frozen = false;
                    lnn_ap_mark_status_dirty();
                }
            } else if (!preview_frozen) {
                scale_rgb565_to_preview(&window);
            }
            stat_scale_ms += (uint32_t)xTaskGetTickCount() - t0;
            stat_scale_cnt++;
        } else {
            LOGW("unsupported camera frame format=%d", fb->format);
        }

        lisa_camera_release_fb(camera_device, fb);

        if (stat_window_start == 0U) {
            stat_window_start = now;
        } else if (now - stat_window_start >= FPS_STAT_WINDOW_MS) {
            LOGI("fps-stat: capture=%u.%02u fps, scale=%ums/call, ui-refresh=%u, busy=%u%%, "
                 "lv_handler=%ums x%u, ui-loop=%u, sleep=%u(max%u), poll=%ums",
                 (unsigned)((uint64_t)stat_capture_cnt * 100000U / FPS_STAT_WINDOW_MS / 100U),
                 (unsigned)((uint64_t)stat_capture_cnt * 100000U / FPS_STAT_WINDOW_MS % 100U),
                 stat_scale_cnt ? stat_scale_ms / stat_scale_cnt : 0U,
                 stat_flush_cnt,
                 stat_scale_ms * 100U / FPS_STAT_WINDOW_MS,
                 stat_lvhandler_cnt ? stat_lvhandler_ms / stat_lvhandler_cnt : 0U,
                 stat_lvhandler_cnt,
                 stat_uitask_cnt,
                 stat_wait_ms,
                 stat_wait_max,
                 stat_poll_ms);
            stat_capture_cnt = 0;
            stat_scale_ms = 0;
            stat_scale_cnt = 0;
            stat_flush_cnt = 0;
            stat_lvhandler_ms = 0;
            stat_lvhandler_cnt = 0;
            stat_uitask_cnt = 0;
            stat_wait_ms = 0;
            stat_wait_max = 0;
            stat_poll_ms = 0;
            stat_window_start = now;
        }

        vTaskDelay(pdMS_TO_TICKS(3));
    }
}
