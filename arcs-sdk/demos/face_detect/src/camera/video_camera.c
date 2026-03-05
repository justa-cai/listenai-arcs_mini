/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA Camera 摄像头驱动示例
 *
 * 演示内容：
 * 1. 初始化摄像头设备并配置总线接口（DVP）
 * 2. 配置摄像头参数（分辨率、像素格式等）
 * 3. 捕获图像帧并通过串口发送
 *
 * 注意：本示例需要实际连接摄像头硬件才能正常运行
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "lisa_device.h"
#include "lisa_camera.h"

#include "FreeRTOS.h"
#include "task.h"

#include "lisa_gpio.h"
#include "lisa_uart.h"
#include "IOMuxManager.h"

#include "video_camera.h"

#define TAG "video_camera"
#include <lisa_log.h>

/* 摄像头设备名称 */
#define CAMERA_DEVICE    "camera"

/* DVP设备名称 */
#define DVP_DEVICE       "dvp0"

/* DMA 通道 */
#define DMA_CHANNEL      2

/* Static variables */
static lisa_device_t *camera_dev = NULL;
static bool is_initialized = false;
static bool is_capturing = false;

/* Callback */
static camera_frame_callback_t frame_callback = NULL;
static void *callback_user_data = NULL;

/* ========================================================================
 * DVP 引脚配置
 * ======================================================================== */

#ifdef CONFIG_BOARD_ARCS_EVB

#define CAM_PWDN_PIN    7

#define CAM_HSYNC_PIN   10
#define CAM_VSYNC_PIN   11
#define CAM_PCLK_PIN    12
#define CAM_MCLK_PIN    26
#define CAM_D0_PIN      13
#define CAM_D1_PIN      14
#define CAM_D2_PIN      15
#define CAM_D3_PIN      16
#define CAM_D4_PIN      17
#define CAM_D5_PIN      18
#define CAM_D6_PIN      19
#define CAM_D7_PIN      20

// void lisa_gpiob_pinmux(void)
// {
//     IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, CAM_PWDN_PIN, CSK_IOMUX_FUNC_DEFAULT);
// }

void lisa_uart1_pinmux()
{
    // IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_ALTER3);
}

// void lisa_i2c0_pinmux(void)
// {
//     IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22, 8);
//     IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 23, 8);
// }

void lisa_dvp_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_HSYNC_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_VSYNC_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_PCLK_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D0_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D1_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D2_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D3_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D4_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D5_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D6_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D7_PIN, CSK_IOMUX_FUNC_ALTER16);
}
#endif

static void video_camera_release_fb(void *data)
{

    lisa_camera_fb_t *fb = (lisa_camera_fb_t *)data;

    if (camera_dev) {
        /* Release frame */
        lisa_camera_release_fb(camera_dev, fb);
    }
}

static void capture_task(void *arg)
{
    (void)arg;
    int ret = 0;

    LOGI("Capture task started");

    while (1) {
        camera_frame_t frame;
        lisa_camera_fb_t *fb = NULL;

        if (!is_capturing) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /* Get frame from camera */
        ret = lisa_camera_capture(camera_dev, &fb);
         if (ret != LISA_DEVICE_OK || fb == NULL) {
            LOGE("Capture failed: %d", ret);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* Fill frame structure */
        frame.buffer = fb->buf;
        frame.length = fb->len;
        frame.width = fb->width;
        frame.height = fb->height;
        frame.format = fb->format;
        frame.func = video_camera_release_fb;
        frame.func_param = (void *)fb;

        /* Call callback if registered */
        if (frame_callback) {
            frame_callback(&frame, callback_user_data);
        }
    }

    LOGI("Capture task stopped");
}

int video_camera_init(camera_config_t *config)
{
    int ret = 0;

    LISA_LOGI(TAG, "camera config: w:%d, h:%d, format:%d", config->width, config->height, config->format);

    if (is_initialized) {
        LOGW("Camera already initialized");
        return 0;
    }

    if(!config)
    {
        LOGE("camera config is NULL");
        return -1;
    }

    /* Get camera device */
    camera_dev = lisa_device_get(CAMERA_DEVICE);
    if (!lisa_device_ready(camera_dev)) {
        LOGE("Camera device not ready");
        return -1;
    }
    LOGI("%s device ready", CAMERA_DEVICE);

    /* Configure GPIO for camera power control */
    lisa_device_t *gpioa = lisa_device_get("gpioa");
    if (gpioa) {
        lisa_gpio_configure(gpioa, 23, LISA_GPIO_CONFIG_OUTPUT_HIGH);
    }

    /* Get I2C device for camera configuration */
    lisa_device_t *i2c_dev = lisa_device_get("i2c0");
    if (!lisa_device_ready(i2c_dev)) {
        LOGE("I2C device not ready");
        return -1;
    }

    /* 配置摄像头参数 */
    lisa_camera_config_t camera_config = {
    .hw_config = {
            .mclk_pad = CSK_IOMUX_PAD_A,
            .mclk_pin = CAM_MCLK_PIN,
            .pwdn_gpio_dev = lisa_device_get("gpiob"),
            .pwdn_pin = CAM_PWDN_PIN,
            .pwdn_delay_us = 0,
            .xclk_delay_us = 0,
            .i2c_dev = i2c_dev,
        },
        .xclk_freq_hz = 8000000,
        .fb_count = 4,
        .enable_hmirror = false,
        .enable_vflip = false,
        .enable_colorbar = false,
    };

    LOGI("Setting up camera...");
    ret = lisa_camera_setup(camera_dev, &camera_config);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to setup camera: %d", ret);
        return -1;
    }

    lisa_camera_capabilities_t caps;
    ret = lisa_camera_get_capabilities(camera_dev, &caps);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to get capabilities: %d", ret);
        return -1;
    }
    LOGI("Camera capabilities: max_width=%u, max_height=%u, supported_formats=0x%08X",
         caps.max_width, caps.max_height, caps.supported_formats);

    uint16_t x = 320 - (config->width / 2);
    uint16_t y = 240 - (config->height / 2);

    LISA_LOGI(TAG, "crop:x:%u, y:%u, w:%d, h:%d", x, y, config->width, config->height);

    lisa_camera_crop_t crop = {
        .x = x,
        .y = y,
        .width = config->width,
        .height = config->height,
    };
    ret = lisa_camera_set_crop(camera_dev, &crop);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to set crop: %d", ret);
        return -1;
    }

    ret = lisa_camera_set_pixformat(camera_dev, config->format);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to set pixformat: %d, ret:%d", config->format, ret);
        return -1;
    }

    ret = lisa_camera_set_hmirror(camera_dev, true);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to set hmirror: %d, ret:%d", true, ret);
        return -1;
    }

    video_camera_set_gray(true);

    /* 配置 DVP 总线接口 */
    lisa_camera_bus_config_t bus_config = {
        .dma_channel = DMA_CHANNEL,
        .bus_type = LISA_CAMERA_BUS_DVP,
        .config.dvp = {
            .dvp_dev        = lisa_device_get("dvp0"),
            .dvp_freq = 8000000,
            .data_align     = 1,
            .line_offset    = 0,
            .pixel_offset   = 0,
            .pclk_polarity  = 0,
            .vsync_polarity = 1,
            .hsync_polarity = 1,
        }
    };
    lisa_camera_get_framesize(camera_dev, &bus_config.width, &bus_config.height);
    bus_config.pixel_format = lisa_camera_get_pixformat(camera_dev);

    ret = lisa_camera_attach_bus(camera_dev, &bus_config);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to attach bus: %d", ret);
        return -1;
    }

    is_initialized = true;

    /* Create capture task */
    xTaskCreate(capture_task, "camera_task", 4096, NULL, configMAX_PRIORITIES - 1, NULL);

    LOGI("Camera capture started with internal task");

    return 0;
}

int video_camera_deinit(void)
{
    if (!is_initialized) {
        return 0;
    }

    if (is_capturing) {
        video_camera_stop_capture();
    }

    if (camera_dev) {
        lisa_camera_deinit(camera_dev);
    }

    is_initialized = false;
    camera_dev = NULL;

    LOGI("video_camera deinitialized");
    return 0;
}

int video_camera_register_callback(camera_frame_callback_t callback, void *user_data)
{
    if (!callback) {
        LOGE("Invalid callback");
        return -1;
    }

    frame_callback = callback;
    callback_user_data = user_data;
    LOGI("Frame callback registered");

    return 0;
}

int video_camera_start_capture(void)
{
    if (!is_initialized) {
        return -1;
    }

    if (is_capturing) {
        LOGW("Camera already capturing");
        return 0;
    }

    /* Start camera hardware */
    int ret = lisa_camera_start(camera_dev);
    if (ret != 0) {
        LOGE("Camera start failed: %d", ret);
        return ret;
    }

    is_capturing = true;

    return 0;
}

int video_camera_stop_capture(void)
{
    if (!is_capturing) {
        return 0;
    }

    is_capturing = false;

    if (camera_dev) {
        lisa_camera_stop(camera_dev);
    }

    LOGI("Camera capture stopped");

    return 0;
}

static volatile bool is_set = false;
int video_camera_set_gray(bool enable)
{
    int ret = 0;

    if (enable) {
        if (is_set == true) {
            LOGE("camera_set_gray already on\r\n");
            return -1;
        }

        video_camera_stop_capture();
        
        ret = lisa_camera_set_reg(camera_dev, 0XFE, 0xff, 0x00); // select page 0
        if (ret != LISA_DEVICE_OK) {
            LOGE("%s, %d, Failed to set pixformat: GRAY ret:%d", __FUNCTION__, __LINE__, ret);
            video_camera_start_capture();
            return -1;
        }
        
        ret = lisa_camera_set_reg(camera_dev, 0x43, 0x02, 0x02); // CbCr fixed enable
        if (ret != LISA_DEVICE_OK) {
            LOGE("%s, %d, Failed to set pixformat: GRAY ret:%d", __FUNCTION__, __LINE__, ret);
            video_camera_start_capture();
            return -1;
        }
        
        ret = lisa_camera_set_reg(camera_dev, 0xda, 0xff, 0x00); // Cb fixed 0x00
        if (ret != LISA_DEVICE_OK) {
            LOGE("%s, %d, Failed to set pixformat: GRAY ret:%d", __FUNCTION__, __LINE__, ret);
            video_camera_start_capture();
            return -1;
        }
                
        ret = lisa_camera_set_reg(camera_dev, 0xdb, 0xff, 0x00); // Cr fixed 0x00
        if (ret != LISA_DEVICE_OK) {
            LOGE("%s, %d, Failed to set pixformat: GRAY ret:%d", __FUNCTION__, __LINE__, ret);
            video_camera_start_capture();
            return -1;
        }

        is_set = true;
        video_camera_start_capture();

        LOGI("camera_set_gray on success\r\n");

    } else {
        if (is_set == false) {
            LOGE("camera_set_gray already off\r\n");
            return -1;
        }

        video_camera_stop_capture();
        
        ret = lisa_camera_set_reg(camera_dev, 0XFE, 0xff, 0x00); // select page 0
        if (ret != LISA_DEVICE_OK) {
            LOGE("%s, %d, Failed to set pixformat: YUV422 ret:%d", __FUNCTION__, __LINE__, ret);
            video_camera_start_capture();
            return -1;
        }

        ret = lisa_camera_set_reg(camera_dev, 0x43, 0x02, 0x00); // CbCr fixed disable
        if (ret != LISA_DEVICE_OK) {
            LOGE("%s, %d, Failed to set pixformat: YUV422 ret:%d", __FUNCTION__, __LINE__, ret);
            video_camera_start_capture();
            return -1;
        }

        is_set = false;
        video_camera_start_capture();

        LOGI("camera_set_gray off success\r\n");
    }

    return 0;
}
