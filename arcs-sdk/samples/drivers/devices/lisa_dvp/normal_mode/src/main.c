/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA DVP 驱动示例 - 普通模式
 *
 * 演示内容：
 * 1. 初始化 DVP 设备并配置参数
 * 2. 使用普通模式捕获图像帧
 * 3. 在回调函数中处理接收到的图像数据
 * 4. 连续捕获多帧图像
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "lisa_device.h"
#include "lisa_dvp.h"
#include "lisa_thread.h"

#define TAG "sample_dvp_normal"
#include "lisa_log.h"

/* DVP 设备名称 */
#define DVP_DEVICE_NAME    LISA_DVP0_NAME

/* DMA 通道 */
#define DMA_CHANNEL        2

/* 图像参数 */
#define FRAME_WIDTH        640
#define FRAME_HEIGHT       480
#define FRAME_FORMAT       LISA_DVP_INPUT_FORM_YUV422_Y0CBY1CR  // YUV422 格式，每像素 2 字节

/* 捕获帧数限制 */
#define MAX_CAPTURE_FRAMES 10

/* DVP 配置参数 */
static const lisa_dvp_config_t dvp_config = {
    .dvp_hal_config = {
        .frame_width    = FRAME_WIDTH,
        .frame_height   = FRAME_HEIGHT,
        .pixel_offset   = 0,
        .line_offset    = 0,
        .input_format   = FRAME_FORMAT,
        .data_align     = LISA_DVP_DATA_ALIGN_LEFT,
        .vsync_polarity = LISA_DVP_POL_RISING,
        .hsync_polarity = LISA_DVP_POL_RISING,
        .pclk_polarity  = LISA_DVP_POL_RISING,
    },
    .gpdma_ch = DMA_CHANNEL,
};

/* 帧缓冲区（放在 PSRAM，需要 4 字节对齐） */
static uint8_t frame_buffer[FRAME_WIDTH * FRAME_HEIGHT * 2]
    __attribute__((aligned(4))) __attribute__((section(".psram.data")));

/* 全局变量 */
static lisa_device_t *g_dvp_dev = NULL;
static volatile uint32_t g_frame_count = 0;
static volatile bool g_capture_done = false;

/**
 * @brief DVP 事件回调函数
 *
 * @param event DVP 事件类型
 * @param user_data 用户数据
 */
static void dvp_event_callback(lisa_dvp_event_t event, void *user_data)
{
    if (event == LISA_DVP_EVENT_DONE) {
        g_frame_count++;

        LOGI("Frame %u captured, size=%u bytes", g_frame_count, sizeof(frame_buffer));

        // 打印前几个字节用于验证
        LOGI("Frame data: %02X %02X %02X %02X %02X %02X %02X %02X",
             frame_buffer[0], frame_buffer[1], frame_buffer[2], frame_buffer[3],
             frame_buffer[4], frame_buffer[5], frame_buffer[6], frame_buffer[7]);

        // 检查是否达到捕获帧数限制
        if (g_frame_count >= MAX_CAPTURE_FRAMES) {
            LOGI("Reached maximum capture frames, stopping...");
            g_capture_done = true;

            // 停止 DVP 捕获
            int ret = lisa_dvp_stop(g_dvp_dev);
            if (ret != LISA_DEVICE_OK) {
                LOGE("Failed to stop DVP: %d", ret);
            } else {
                LOGI("DVP stopped successfully");
            }
        } else {
            // 重载缓冲区，继续捕获下一帧
            int ret = lisa_dvp_reload(g_dvp_dev, frame_buffer, sizeof(frame_buffer));
            if (ret != LISA_DEVICE_OK) {
                LOGE("Failed to reload buffer: %d", ret);
                g_capture_done = true;
            }
        }
    }
}

/**
 * @brief 初始化 DVP 设备
 *
 * @return 0 成功，其他值失败
 */
static int dvp_init(void)
{
    int ret;

    // 1. 获取 DVP 设备
    g_dvp_dev = lisa_device_get(DVP_DEVICE_NAME);
    if (!g_dvp_dev) {
        LOGE("Failed to get %s device", DVP_DEVICE_NAME);
        return -1;
    }
    LOGI("DVP device obtained");

    // 2. 配置 DVP 设备
    ret = lisa_dvp_setup(g_dvp_dev, &dvp_config, dvp_event_callback, NULL);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to setup DVP: %d", ret);
        return -1;
    }
    LOGI("DVP setup completed (DMA channel: %u)", DMA_CHANNEL);

    // 3. 启用 DVP 时钟输出（为摄像头提供 25MHz 时钟）
    ret = lisa_dvp_enable_clockout(g_dvp_dev, 25000000);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to enable DVP clockout: %d", ret);
        return -1;
    }
    LOGI("DVP clockout enabled (25MHz)");

    return 0;
}

/**
 * @brief 启动 DVP 捕获
 *
 * @return 0 成功，其他值失败
 */
static int dvp_start_capture(void)
{
    int ret;

    // 清空帧缓冲区
    memset(frame_buffer, 0, sizeof(frame_buffer));

    // 启动 DVP 捕获
    ret = lisa_dvp_start(g_dvp_dev, frame_buffer, sizeof(frame_buffer));
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to start DVP capture: %d", ret);
        return -1;
    }

    LOGI("DVP capture started (normal mode)");
    LOGI("Frame size: %ux%u, buffer size: %u bytes",
         FRAME_WIDTH, FRAME_HEIGHT, sizeof(frame_buffer));

    return 0;
}

int main(int argc, char **argv)
{
    int ret;

    LOGI("========================================");
    LOGI("  LISA DVP Driver Sample - Normal Mode");
    LOGI("========================================");
    LOGI("Frame format: YUV422");
    LOGI("Resolution: %ux%u", FRAME_WIDTH, FRAME_HEIGHT);
    LOGI("Max frames: %u", MAX_CAPTURE_FRAMES);
    LOGI("");

    // 初始化 DVP 设备
    ret = dvp_init();
    if (ret != 0) {
        LOGE("DVP initialization failed");
        return -1;
    }

    // 启动捕获
    ret = dvp_start_capture();
    if (ret != 0) {
        LOGE("Failed to start capture");
        return -1;
    }

    // 等待捕获完成
    LOGI("Waiting for frames...");
    while (!g_capture_done) {
        lisa_thread_mdelay(100);
    }

    LOGI("");
    LOGI("========================================");
    LOGI("Capture completed, total frames: %u", g_frame_count);
    LOGI("========================================");

    return 0;
}
