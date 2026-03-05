/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA RGB Bounce Buffer 模式示例
 *
 * 演示如何使用 RGB 驱动的 Bounce Buffer 模式显示彩色图案。
 * 展示双缓冲机制和自动帧切换功能。
 */

#include <stdio.h>
#include <string.h>
#include "IOMuxManager.h"
#include "lisa_device.h"
#include "lisa_rgb.h"
#include "lisa_mem.h"
#include "lisa_thread.h"

#define LOG_TAG "rgb_sample"
#include "lisa_log.h"

/*
    为满足不同板型示例场景，重定向rgb设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_rgb_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 0, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 1, CSK_IOMUX_FUNC_DEFAULT);

    /*pclk, hsync, vsync, de, r, g, b*/
    for (int i = 4; i < 26; i++) {
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, i, CSK_IOMUX_FUNC_ALTER31);
    }
}
#endif

/* RGB 显示分辨率 */
#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 480

/* 帧缓冲区大小（RGB565，每像素 2 字节） */
#define FRAME_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT * 2)

/* RGB565 颜色定义 */
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_WHITE   0xFFFF
#define COLOR_BLACK   0x0000
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F

/**
 * @brief 填充纯色到帧缓冲区
 * @param fb 帧缓冲区指针
 * @param color RGB565 颜色值
 */
static void fill_color(uint16_t *fb, uint16_t color)
{
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        fb[i] = color;
    }
}

/**
 * @brief 绘制彩色条纹图案
 * @param fb 帧缓冲区指针
 */
static void draw_color_bars(uint16_t *fb)
{
    const uint16_t colors[] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE,
        COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA,
        COLOR_WHITE, COLOR_BLACK
    };
    const int num_colors = sizeof(colors) / sizeof(colors[0]);
    const int bar_width = SCREEN_WIDTH / num_colors;

    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            int color_idx = x / bar_width;
            if (color_idx >= num_colors) {
                color_idx = num_colors - 1;
            }
            fb[y * SCREEN_WIDTH + x] = colors[color_idx];
        }
    }
}

/**
 * @brief 绘制渐变图案
 * @param fb 帧缓冲区指针
 */
static void draw_gradient(uint16_t *fb)
{
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            // 计算 RGB 分量（0-31 for R/B, 0-63 for G）
            uint8_t r = (x * 31) / SCREEN_WIDTH;
            uint8_t g = (y * 63) / SCREEN_HEIGHT;
            uint8_t b = ((x + y) * 31) / (SCREEN_WIDTH + SCREEN_HEIGHT);

            // 合成 RGB565
            uint16_t color = (r << 11) | (g << 5) | b;
            fb[y * SCREEN_WIDTH + x] = color;
        }
    }
}

int main(void)
{
    LOGI("=== LISA RGB Bounce Buffer 模式示例 ===\n");

    // 1. 获取 RGB 设备
    lisa_device_t *rgb = lisa_device_get("rgb0");
    if (!rgb) {
        LOGE("Failed to get RGB device");
        return -1;
    }
    LOGI("RGB device ready");

    lisa_display_bus_rgb_config_t rgb_config = {
        .rgb_dev            = rgb,
        .pclk_hz            = 6*1000*1000,
        .bounce_buffer_size = 480 * 16,
        .vsync_polarity     = LISA_RGB_POLARITY_POSITIVE,
        .hsync_polarity     = LISA_RGB_POLARITY_POSITIVE,
        .pclk_polarity      = LISA_RGB_POLARITY_NEGATIVE,
        .de_polarity        = LISA_RGB_POLARITY_POSITIVE,
        .input_format       = LISA_RGB_INPUT_FORMAT_RGB565,
        .output_format      = LISA_RGB_OUTPUT_FORMAT_RGB565,
        .output_lsb_first   = false,
        .timings = {
            .h_res = 480,
            .v_res = 480,
            .h_pulse_width = 10,
            .v_pulse_width = 10,
            .h_front_blanking = 20,
            .h_back_blanking = 10,
            .v_front_blanking = 20,
            .v_back_blanking = 1,
        }
    };

    lisa_rgb_setup(rgb, &rgb_config);

    // 2. 分配帧缓冲区（PSRAM）
    uint16_t *framebuffer = (uint16_t *)lisa_mem_alloc(FRAME_SIZE);
    if (!framebuffer) {
        LOGE("Failed to allocate framebuffer (%d bytes)", FRAME_SIZE);
        return -1;
    }
    LOGI("Framebuffer allocated: %p (size: %d bytes)", framebuffer, FRAME_SIZE);

    lisa_rgb_start(rgb);

    // 3. 循环显示不同的图案
    int pattern = 0;
    int counter = 0;

    LOGI("Starting pattern display loop...\n");

    while (1) {
        // 绘制当前图案
        switch (pattern) {
        case 0:
            LOGI("Pattern %d: Red screen", counter);
            fill_color(framebuffer, COLOR_RED);
            break;
        case 1:
            LOGI("Pattern %d: Green screen", counter);
            fill_color(framebuffer, COLOR_GREEN);
            break;
        case 2:
            LOGI("Pattern %d: Blue screen", counter);
            fill_color(framebuffer, COLOR_BLUE);
            break;
        case 3:
            LOGI("Pattern %d: White screen", counter);
            fill_color(framebuffer, COLOR_WHITE);
            break;
        case 4:
            LOGI("Pattern %d: Color bars", counter);
            draw_color_bars(framebuffer);
            break;
        case 5:
            LOGI("Pattern %d: Gradient", counter);
            draw_gradient(framebuffer);
            break;
        default:
            pattern = 0;
            continue;
        }

        // 4. 更新显示（拷贝到非显示缓冲区并标记待切换）
        int ret = lisa_rgb_update_framebuffer(rgb, framebuffer, FRAME_SIZE);
        if (ret != LISA_DEVICE_OK) {
            LOGE("Failed to update framebuffer: %d", ret);
            break;
        }

        // DMA 中断会在下一帧传输完成时自动切换显示缓冲区
        // 应用层无需手动管理缓冲区切换

        // 5. 延时后切换到下一个图案
        lisa_thread_mdelay(2000);

        pattern = (pattern + 1) % 6;
        counter++;
    }

    // 6. 清理资源（实际上不会执行到这里）
    lisa_mem_free(framebuffer);
    LOGI("Sample completed");

    return 0;
}
