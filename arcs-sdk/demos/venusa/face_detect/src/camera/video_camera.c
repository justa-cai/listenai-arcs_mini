/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file video_camera.c
 * @brief VenusA 人脸检测示例的摄像头采集封装。
 *
 * @details
 * 本文件负责初始化 LISA Camera 设备、配置 DVP 总线、创建采集任务，并将
 * 摄像头帧通过回调形式交给上层人脸检测流程处理。上层回调处理完成后需要
 * 调用帧内携带的释放函数归还底层帧缓存。
 *
 * 注意：本示例需要实际连接摄像头硬件才能正常运行。
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

#define CAMERA_DEVICE "camera" /* 摄像头设备名称 */
#define DVP_DEVICE    "dvp0"   /* DVP设备名称 */
#define DMA_CHANNEL   2        /* DVP 采集使用的 DMA 通道。 */

static lisa_device_t *camera_dev = NULL;              /* 摄像头设备句柄。 */
static bool is_initialized = false;                   /* 摄像头模块是否已完成初始化。 */
static volatile bool is_capturing = false;            /* 摄像头采集任务是否处于采集状态。 */
static camera_frame_callback_t frame_callback = NULL; /* 上层注册的图像帧回调函数。 */
static void *callback_user_data = NULL;               /* 透传给上层图像帧回调的用户数据。 */

/* ========================================================================
 * 摄像头板级管脚配置。
 * ======================================================================== */

#if defined(CONFIG_BOARD_CSK7S_YT_EVB)
/*------------ CSK7S_YT_EVB ------------------*/

/* 摄像头 PWDN 管脚配置。 */
#define CAM_PWDN_ENABLED       0
#define CAM_PWDN_DEV           NULL
#define CAM_PWDN_PIN           0
/* 摄像头 RESET 管脚配置。 */
#define CAM_RESET_ENABLED      1
#define CAM_RESET_DEV          lisa_device_get("gpiob")
#define CAM_RESET_PIN          2
#define CAM_RESET_ACTIVE_LEVEL 0

/* 摄像头 I2C 配置设备和外部时钟频率。 */
#define CAMERA_I2C_DEVICE   "i2c1"
#define CAMERA_XCLK_FREQ_HZ 12000000

/* 摄像头 DVP 同步信号和主时钟管脚。 */
#define CAM_HSYNC_PIN 21
#define CAM_VSYNC_PIN 22
#define CAM_PCLK_PIN  23
#define CAM_MCLK_PIN  3
#define CAM_MCLK_PAD  CSK_IOMUX_PAD_B
/* 摄像头 DVP 8bit 数据管脚。 */
#define CAM_D0_PIN    24
#define CAM_D1_PIN    25
#define CAM_D2_PIN    26
#define CAM_D3_PIN    27
#define CAM_D4_PIN    28
#define CAM_D5_PIN    29
#define CAM_D6_PIN    30
#define CAM_D7_PIN    31

/**
 * @brief 配置摄像头 GPIO 相关管脚复用。
 */
void lisa_gpiob_pinmux(void)
{
#if CAM_PWDN_ENABLED
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, CAM_PWDN_PIN, CSK_IOMUX_FUNC_DEFAULT);
#endif
#if CAM_RESET_ENABLED
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, CAM_RESET_PIN, CSK_IOMUX_FUNC_DEFAULT);
#endif
}

/**
 * @brief 配置摄像头 I2C1 的 SDA/SCL 管脚复用。
 */
void lisa_i2c1_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 1, CSK_IOMUX_FUNC_ALTER8); /* SDA */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 0, CSK_IOMUX_FUNC_ALTER8); /* SCL */
}

/**
 * @brief 配置 DVP 采集接口相关管脚复用。
 */
void lisa_dvp_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_HSYNC_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_VSYNC_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_PCLK_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D0_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D1_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D2_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D3_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D4_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D5_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D6_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D7_PIN, CSK_IOMUX_FUNC_ALTER13);
}

#elif defined(CONFIG_BOARD_VENUSA_RD_EVB)
/*------------ VENUSA_RD_EVB ------------------*/

/* 摄像头 PWDN 管脚配置。 */
#define CAM_PWDN_ENABLED       0
#define CAM_PWDN_DEV           NULL
#define CAM_PWDN_PIN           0
/* 摄像头 RESET 管脚配置。 */
#define CAM_RESET_ENABLED      0
#define CAM_RESET_DEV          NULL
#define CAM_RESET_PIN          0
#define CAM_RESET_ACTIVE_LEVEL 0

/* 摄像头 I2C 配置设备和外部时钟频率。 */
#define CAMERA_I2C_DEVICE   "i2c1"
#define CAMERA_XCLK_FREQ_HZ 12000000

/* 摄像头 DVP 同步信号和主时钟管脚。 */
#define CAM_HSYNC_PIN 21
#define CAM_VSYNC_PIN 22
#define CAM_PCLK_PIN  23
#define CAM_MCLK_PIN  3
#define CAM_MCLK_PAD  CSK_IOMUX_PAD_B
/* 摄像头 DVP 8bit 数据管脚。 */
#define CAM_D0_PIN    24
#define CAM_D1_PIN    25
#define CAM_D2_PIN    26
#define CAM_D3_PIN    27
#define CAM_D4_PIN    28
#define CAM_D5_PIN    29
#define CAM_D6_PIN    30
#define CAM_D7_PIN    31

/**
 * @brief 配置摄像头 GPIO 相关管脚复用。
 */
void lisa_gpiob_pinmux(void)
{
#if CAM_PWDN_ENABLED
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, CAM_PWDN_PIN, CSK_IOMUX_FUNC_DEFAULT);
#endif
#if CAM_RESET_ENABLED
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, CAM_RESET_PIN, CSK_IOMUX_FUNC_DEFAULT);
#endif
}

/**
 * @brief 配置摄像头 I2C1 的 SDA/SCL 管脚复用。
 */
void lisa_i2c1_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 1, CSK_IOMUX_FUNC_ALTER8); /* SDA */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, CSK_IOMUX_FUNC_ALTER8); /* SCL */
}

/**
 * @brief 配置结果输出UART1管脚复用。
 *
 */
void lisa_uart1_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 7, CSK_IOMUX_FUNC_ALTER3);
}

/**
 * @brief 配置 DVP 采集接口相关管脚复用。
 */
void lisa_dvp_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_HSYNC_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_VSYNC_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_PCLK_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D0_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D1_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D2_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D3_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D4_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D5_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D6_PIN, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D7_PIN, CSK_IOMUX_FUNC_ALTER13);
}

#endif

/**
 * @brief 释放摄像头底层帧缓存。
 *
 * @param data 待释放的 @ref lisa_camera_fb_t 指针。
 */
static void video_camera_release_fb(void *data)
{

    lisa_camera_fb_t *fb = (lisa_camera_fb_t *)data;

    if (camera_dev) {
        /* 归还底层摄像头帧缓存。 */
        lisa_camera_release_fb(camera_dev, fb);
    }
}

/**
 * @brief 摄像头采集任务。
 *
 * @details
 * 任务循环等待采集使能标志，启动后持续从摄像头设备获取帧缓存，并封装为
 * @ref camera_frame_t 交给上层注册的回调处理。帧缓存不会在本任务中立即释放，
 * 而是通过 @ref camera_frame_t::func 交由上层处理完成后释放。
 *
 * @param arg FreeRTOS 任务参数，当前未使用。
 */
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

        /* 从摄像头设备获取一帧图像。 */
        ret = lisa_camera_capture(camera_dev, &fb);
        if (ret != LISA_DEVICE_OK || fb == NULL) {
            LOGE("Capture failed: %d", ret);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* 填充对上层可见的图像帧结构。 */
        frame.buffer = fb->buf;
        frame.length = fb->len;
        frame.width = fb->width;
        frame.height = fb->height;
        frame.format = fb->format;
        frame.func = video_camera_release_fb;
        frame.func_param = (void *)fb;

        /* 上层已注册回调时，将当前帧交给上层处理。 */
        if (frame_callback) {
            frame_callback(&frame, callback_user_data);
        }
    }

    LOGI("Capture task stopped");
}

/**
 * @brief 初始化摄像头设备、DVP 总线和采集任务。
 *
 * @param config 上层指定的摄像头配置，包含分辨率和图像格式。
 *
 * @retval 0  初始化成功，或摄像头已经初始化。
 * @retval -1 参数无效、设备未就绪、摄像头配置失败或总线绑定失败。
 */
int video_camera_init(camera_config_t *config)
{
    int ret = 0;

    LISA_LOGI(TAG, "camera config: w:%d, h:%d, format:%d", config->width, config->height, config->format);

    if (is_initialized) {
        LOGW("Camera already initialized");
        return 0;
    }

    if (!config) {
        LOGE("camera config is NULL");
        return -1;
    }

    /* 获取摄像头设备。 */
    camera_dev = lisa_device_get(CAMERA_DEVICE);
    if (!lisa_device_ready(camera_dev)) {
        LOGE("Camera device not ready");
        return -1;
    }
    LOGI("%s device ready", CAMERA_DEVICE);

    /* 获取用于配置摄像头寄存器的 I2C 设备。 */
    lisa_device_t *i2c_dev = lisa_device_get(CAMERA_I2C_DEVICE);
    if (!lisa_device_ready(i2c_dev)) {
        LOGE("I2C device not ready");
        return -1;
    }

    /* 配置摄像头参数 */
    lisa_camera_config_t camera_config = {
        .hw_config =
            {
                .mclk_pad = CAM_MCLK_PAD,
                .mclk_pin = CAM_MCLK_PIN,
                .pwdn_gpio_dev = CAM_PWDN_DEV,
                .pwdn_pin = CAM_PWDN_PIN,
                .reset_gpio_dev = CAM_RESET_DEV,
                .reset_pin = CAM_RESET_PIN,
                .reset_active_level = CAM_RESET_ACTIVE_LEVEL,
                .reset_delay_us = 30000, /* 30ms */
                .pwdn_delay_us = 0,
                .xclk_delay_us = 10000, /* 10ms */
                .i2c_dev = i2c_dev,
            },
        .xclk_freq_hz = CAMERA_XCLK_FREQ_HZ,
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
    LOGI("Camera capabilities: max_width=%u, max_height=%u, supported_formats=0x%08X", caps.max_width, caps.max_height,
         caps.supported_formats);

    /* 配置 DVP 总线接口 */
    lisa_camera_bus_config_t bus_config = {.dma_channel = DMA_CHANNEL,
                                           .pixel_format = LISA_CAMERA_PIXFMT_YUV422,
                                           .bus_type = LISA_CAMERA_BUS_DVP,
                                           .config.dvp = {
                                               .dvp_dev = lisa_device_get("dvp0"),
                                               .dvp_freq = camera_config.xclk_freq_hz,
                                               .data_align = 1,
                                               .line_offset = 0,
                                               .pixel_offset = 0,
                                               .pclk_polarity = 1,
                                               .vsync_polarity = 0,
                                               .hsync_polarity = 1,
                                           }};
    lisa_camera_get_framesize(camera_dev, &bus_config.width, &bus_config.height);
    bus_config.pixel_format = lisa_camera_get_pixformat(camera_dev);

    ret = lisa_camera_attach_bus(camera_dev, &bus_config);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to attach bus: %d", ret);
        return -1;
    }

    is_initialized = true;

    /* 创建内部采集任务。 */
    xTaskCreate(capture_task, "camera_task", 4096, NULL, configMAX_PRIORITIES - 1, NULL);

    LOGI("Camera capture started with internal task");

    return 0;
}

/**
 * @brief 反初始化摄像头模块并停止采集。
 *
 * @retval 0 反初始化完成；未初始化时也返回成功。
 */
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

/**
 * @brief 注册摄像头图像帧回调。
 *
 * @param callback 图像帧回调函数，不能为 NULL。
 * @param user_data 透传给 @p callback 的用户数据。
 *
 * @retval 0  回调注册成功。
 * @retval -1 回调函数为空。
 */
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

/**
 * @brief 启动摄像头硬件采集。
 *
 * @retval 0 启动成功，或摄像头已经处于采集状态。
 * @return 摄像头驱动启动失败时返回底层错误码；未初始化时返回 -1。
 */
int video_camera_start_capture(void)
{
    if (!is_initialized) {
        return -1;
    }

    if (is_capturing) {
        LOGW("Camera already capturing");
        return 0;
    }

    /* 启动摄像头硬件采集。 */
    int ret = lisa_camera_start(camera_dev);
    if (ret != 0) {
        LOGE("Camera start failed: %d", ret);
        return ret;
    }

    is_capturing = true;

    return 0;
}

/**
 * @brief 停止摄像头硬件采集。
 *
 * @retval 0 停止成功；未处于采集状态时也返回成功。
 */
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

/**
 * @brief 设置灰度模式。
 *
 * @param enable 是否使能灰度模式，当前未使用。
 *
 * @retval -1 SC030IOT 当前不支持灰度模式切换。
 */
int video_camera_set_gray(bool enable)
{
    (void)enable;
    LOGW("gray mode switch is not supported by SC030IOT");
    return -1;
}
