/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lcd_logo.c
 * @brief LCD 显示 Logo 示例
 */

#include <lisa_device.h>
#include <lisa_display.h>
#include <lisa_mem.h>
#include <stdint.h>
#include "IOMuxManager.h"
#include "lisa_gpio.h"
#include "board.h"
#include "pinmux.h"
#include "logo_data.h"

#define LOG_TAG "lcd_logo"
#include <lisa_log.h>

int lcd_show_logo(void)
{
    LISA_LOGI(LOG_TAG, "LCD Logo display started");

    // 配置 LCD 所需的引脚复用
    lisa_spi0_pinmux();   // SPI0: LCD_CS, LCD_SPI_DATA, LCD_SPI_CLK
    lisa_gpioa_pinmux();  // GPIOA: LCD_CD
    lisa_gpiob_pinmux();  // GPIOB: LCD_RST
    lisa_pwm_pinmux();    // PWM: LCD_PWM (backlight)

    lisa_device_t *gpioa_dev = lisa_device_get("gpioa");
    lisa_device_t *gpiob_dev = lisa_device_get("gpiob");

    // Display 配置 (SPI 4-Wire, ST7789P3)
    lisa_display_config_t display_config = {
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .bus_config = {.spi_4wire =
                           {
                               .spi_dev = lisa_device_get("spi0"),
                               .cs_gpio = gpioa_dev,
                               .cs_pin = LCD_CS_PIN,
                               .dc_gpio = gpioa_dev,
                               .dc_pin = LCD_CD_PIN,
                               .spi_freq = 50 * 1000 * 1000,

                           }},
        .backlight = {.type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
                      .config.pwm = {.channel = 1, .dev = lisa_device_get("pwm0"), .freq = 2000}},
        .rst_gpio = gpiob_dev,
        .rst_pin = LCD_RST_PIN,
    };

    lisa_device_t *display_device = lisa_device_get("display");
    if (!display_device) {
        LISA_LOGE(LOG_TAG, "Failed to get display device");
        return -1;
    }

    lisa_display_attach_bus(display_device, &display_config);

    lisa_display_capabilities_t caps;
    lisa_display_get_capabilities(display_device, &caps);
    LISA_LOGI(LOG_TAG, "Display: %d x %d", caps.width, caps.height);

    // 打开显示和背光
    lisa_display_blanking_off(display_device);
    lisa_display_set_brightness(display_device, 100);

    // 设置显示方向（如果图片旋转了，可以修改这个值）
    // 0=0°, 1=90°, 2=180°, 3=270°
    lisa_display_set_orientation(display_device, LISA_DISPLAY_ORIENTATION_0);

    // 显示 Logo - 逐行渲染
    int line_bytes = LOGO_WIDTH * 2;
    uint8_t *buf = lisa_mem_alloc(line_bytes);
    if (!buf) {
        LISA_LOGE(LOG_TAG, "Failed to allocate line buffer");
        return -1;
    }

    LISA_LOGI(LOG_TAG, "Displaying logo (%d x %d)", LOGO_WIDTH, LOGO_HEIGHT);

    lisa_display_buffer_desc_t desc = {
        .width = LOGO_WIDTH,
        .height = 1,
        .buf_size = line_bytes,
        .pitch = line_bytes,
    };

    for (int y = 0; y < LOGO_HEIGHT; y++) {
        // 复制一行数据到缓冲区
        int offset = y * line_bytes;
        for (int x = 0; x < line_bytes; x++) {
            buf[x] = logo_data[offset + x];
        }
        lisa_display_write(display_device, 0, y, &desc, buf);
    }

    lisa_mem_free(buf);

    LISA_LOGI(LOG_TAG, "Logo displayed successfully");
    return 0;
}
