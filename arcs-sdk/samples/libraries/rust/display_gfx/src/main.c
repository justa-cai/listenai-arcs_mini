/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */
#include <lisa_device.h>
#include <lisa_display.h>
#include "lisa_gpio.h"
#include "IOMuxManager.h"
#include "board.h"

#define LOG_TAG "rust_display_gfx"
#include <lisa_log.h>

extern int rust_main(void);

/*
 * arcs_evb LCD wiring (ST7789P3 over SPI1 4-wire). Pinmux + bus/panel attach are
 * board-specific C; Rust (rust_main) opens the ready "display" device and draws.
 * Mirrors samples/drivers/devices/lisa_display/display_flush.
 */
#ifdef CONFIG_BOARD_ARCS_EVB
#define LCD_CS_PIN 5
#define LCD_SPI_CLK_PIN 3
#define LCD_SPI_DATA_PIN 1

void lisa_gpioa_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_RST_PIN, CSK_IOMUX_FUNC_ALTER1);
}

void lisa_gpiob_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_TE_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CD_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CS_PIN, CSK_IOMUX_FUNC_DEFAULT);
}

void lisa_spi1_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_CLK_PIN, CSK_IOMUX_FUNC_ALTER6);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_DATA_PIN, CSK_IOMUX_FUNC_ALTER6);
}
#endif

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    LOGI("=== Rust display (embedded-graphics) demo ===");

    lisa_device_t *gpioa_dev = lisa_device_get("gpioa");
    lisa_device_t *gpiob_dev = lisa_device_get("gpiob");

    lisa_display_config_t display_config = {
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .panel_name = "st7789p3",
        .bus_config = {.spi_4wire = {
                           .spi_dev = lisa_device_get("spi1"),
                           .cs_gpio = gpiob_dev,
                           .cs_pin = LCD_CS_PIN,
                           .dc_gpio = gpiob_dev,
                           .dc_pin = LCD_CD_PIN,
                           .spi_freq = 50 * 1000 * 1000,
                       }},
        .backlight = {.type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
                      .blacklight_polarity = LISA_DISPLAY_BLACKLIGHT_POLARITY_LOW,
                      .config.pwm = {.channel = 0, .dev = lisa_device_get("pwm0"), .freq = 2000}},
        .rst_gpio = gpioa_dev,
        .rst_pin = LCD_RST_PIN,
    };

    lisa_device_t *display_device = lisa_device_get("display");
    if (!display_device) {
        LOGE("Failed to get display device");
        return -1;
    }
    int ret = lisa_display_attach_bus(display_device, &display_config);
    if (ret != 0) {
        LOGE("attach_bus failed: %d", ret);
        return -1;
    }

    ret = rust_main();
    if (ret != 0) {
        LOGE("rust_main returned %d", ret);
    }
    return 0;
}
