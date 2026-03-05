/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <lisa_device.h>
#include <lisa_display.h>
#include <lisa_mem.h>
#include <stdint.h>
#include "IOMuxManager.h"
#include "lisa_gpio.h"
#include "board.h"

#define LOG_TAG "sample_display"
#include <lisa_log.h>

/*
    为满足不同板型示例场景，重定向设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB

#define LCD_CS_PIN 5
#define LCD_SPI_CLK_PIN 3
#define LCD_SPI_DATA_PIN 1

void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_RST_PIN, CSK_IOMUX_FUNC_ALTER1);
}

void lisa_gpiob_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_TE_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CD_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CS_PIN, CSK_IOMUX_FUNC_DEFAULT);
}

void lisa_spi1_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_CLK_PIN, CSK_IOMUX_FUNC_ALTER6);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_DATA_PIN, CSK_IOMUX_FUNC_ALTER6);
}

#endif

enum color {
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
};

static void fill_buffer_with_color(uint16_t color, uint16_t *buf, size_t pixel_count)
{
    for (size_t i = 0; i < pixel_count; i++) {
        buf[i] = color;
    }
}

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "Display sample started");

    lisa_device_t *gpioa_dev = lisa_device_get("gpioa");
    lisa_device_t *gpiob_dev = lisa_device_get("gpiob");

    lisa_display_config_t display_config = {
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .bus_config = {.spi_4wire =
                           {
                               .spi_dev = lisa_device_get("spi1"),
                               .cs_gpio = gpiob_dev,
                               .cs_pin = LCD_CS_PIN,
                               .dc_gpio = gpiob_dev,
                               .dc_pin = LCD_CD_PIN,
                               .spi_freq = 50 * 1000 * 1000,

                           }},
        .backlight = {.type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM, .blacklight_polarity = LISA_DISPLAY_BLACKLIGHT_POLARITY_LOW,
                      .config.pwm = {.channel = 0, .dev = lisa_device_get("pwm0"), .freq = 2000}},
        .rst_gpio = gpioa_dev,
        .rst_pin = LCD_RST_PIN,
        // .te_gpio = gpiob_dev,
        // .te_pin  = LCD_TE_PIN
    };

    lisa_device_t *display_device = lisa_device_get("display");
    if (!display_device) {
        LISA_LOGE(LOG_TAG, "Failed to get display device");
        return -1;
    }

    lisa_display_attach_bus(display_device, &display_config);

    lisa_display_capabilities_t caps;
    lisa_display_get_capabilities(display_device, &caps);
    LISA_LOGI(LOG_TAG, "Display capabilities: %d x %d", caps.width, caps.height);

    size_t buffer_pixels = caps.width * caps.height;
    size_t buffer_size = buffer_pixels * sizeof(uint16_t);
    uint16_t *buffer = lisa_mem_alloc(buffer_size);
    if (!buffer) {
        LISA_LOGE(LOG_TAG, "Failed to allocate buffer");
        return -1;
    }

    lisa_display_buffer_desc_t desc = {
        .width = caps.width,
        .height = caps.height,
        .buf_size = buffer_size,
    };

    lisa_display_blanking_off(display_device);

    int color_idx = 0;
    uint8_t brightness = 0;

    lisa_display_set_brightness(display_device, brightness);
    while (1) {
        uint16_t current_color = LISA_DISPLAY_COLOR_RED;
        switch (color_idx) {
        case COLOR_RED:
            current_color = LISA_DISPLAY_COLOR_RED;
            break;
        case COLOR_GREEN:
            current_color = LISA_DISPLAY_COLOR_GREEN;
            break;
        case COLOR_BLUE:
            current_color = LISA_DISPLAY_COLOR_BLUE;
            break;
        default:
            break;
        }

        fill_buffer_with_color(current_color, buffer, buffer_pixels);
        lisa_display_write(display_device, 0, 0, &desc, buffer);

        // Update color index for next loop
        color_idx = (color_idx + 1) % 3;
        brightness = (brightness + 1) % 100;
        LOGI("brightness: %d", brightness);
        lisa_display_set_brightness(display_device, brightness);

        lisa_thread_mdelay(1000);
    }
}
