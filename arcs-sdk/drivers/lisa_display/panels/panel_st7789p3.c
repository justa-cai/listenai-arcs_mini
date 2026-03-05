/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "lisa_display_panel.h"
#include "lisa_mem.h"
#include "lisa_device.h"
#include "mipi_dcs.h"
#include "lisa_gpio.h"

#define LOG_TAG "panel.st7789p3"
#include "lisa_log.h"

/* ST7789P3 特定命令（非标准 MIPI DCS）*/
// 可在此添加芯片特定命令宏

/* 私有数据结构 */
struct panel_st7789p3_priv {
    lisa_display_orientation_t orientation;
};


/* 初始化序列 */
static const uint8_t init_sequence[] = {
    // cmd, len, data...
    0xB2, 5, 0x0C, 0x0C, 0x00, 0x33, 0x33,
    0x35, 1, 0x00,
    0x36, 1, 0x00,
    0x3A, 1, 0x05, // RGB565
    0xB7, 1, 0x55,
    0xBB, 1, 0x16,
    0xC0, 1, 0x2C,
    0xC2, 1, 0x01,
    0xC3, 1, 0x13,
    0xC6, 1, 0x0F, // 60Hz
    0xD0, 3, 0xA7, 0xA4, 0xA1,
    0xD6, 1, 0xA1,
    0xE0, 14, 0xF0, 0x06, 0x0E, 0x08, 0x08, 0x04, 0x37, 0x43, 0x4C, 0x36, 0x12, 0x12, 0x2C, 0x34,
    0xE1, 14, 0xF0, 0x0D, 0x12, 0x0C, 0x0A, 0x16, 0x37, 0x43, 0x4C, 0x39, 0x14, 0x15, 0x2E, 0x36,
};

/* ========================================================================
 *  lisa_display_api_t 实现
 * ======================================================================== */

static int st7789p3_init(lisa_display_panel_t *panel)
{
    // 分配私有数据
    struct panel_st7789p3_priv *priv = NULL;
    priv = lisa_mem_alloc(sizeof(*priv));
    if (!priv) {
        return LISA_DEVICE_ERR_NO_MEM;
    }

    panel->priv_data         = priv;
    panel->caps.width        = CONFIG_PANEL_ST7789P3_WIDTH;
    panel->caps.height       = CONFIG_PANEL_ST7789P3_HEIGHT;
    panel->caps.pixel_format = LISA_DISPLAY_PIXEL_FORMAT_RGB_565;
    panel->caps.orientation  = LISA_DISPLAY_ORIENTATION_0;
    panel->caps.supported_pixel_formats = (1U << LISA_DISPLAY_PIXEL_FORMAT_RGB_565);
    priv->orientation = LISA_DISPLAY_ORIENTATION_0;

    // 硬件复位
    if (panel->rst_gpio) {
        lisa_gpio_write_pin(panel->rst_gpio, panel->rst_pin, 1);
        lisa_thread_mdelay(10);
        lisa_gpio_write_pin(panel->rst_gpio, panel->rst_pin, 0);
        lisa_thread_mdelay(10);
        lisa_gpio_write_pin(panel->rst_gpio, panel->rst_pin, 1);
        lisa_thread_mdelay(120);
    }

    // 退出睡眠模式
    panel_write_cmd_data(panel, LCD_CMD_SLEEP_OUT, 8, NULL, 0);
    lisa_thread_mdelay(120);

    // 发送初始化序列
    const uint8_t *p = init_sequence;
    while (p < init_sequence + sizeof(init_sequence)) {
        uint8_t cmd = *p++;
        uint8_t len = *p++;
        panel_write_cmd_data(panel, cmd, 8, p, len);
        p += len;
    }

#ifdef CONFIG_LISA_DISPLAY_COLOR_INVERT
    panel_write_cmd_data(panel, LCD_CMD_INVERT_ON, 8, NULL, 0);
#else
    panel_write_cmd_data(panel, LCD_CMD_INVERT_OFF, 8, NULL, 0);
#endif

    LISA_LOGI(LOG_TAG, "ST7789P3 initialized");
    return LISA_DEVICE_OK;
}

static void st7789p3_set_window(lisa_display_panel_t *panel, uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h)
{
    lisa_mem_coord_t x_coord, y_coord;

    lisa_display_panel_mem_area_t area = {
        .panel_w = panel->caps.width,
        .panel_h = panel->caps.height,
        .x_offset = CONFIG_PANEL_ST7789P3_X_OFFSET,
        .y_offset = CONFIG_PANEL_ST7789P3_Y_OFFSET,
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };
    panel_set_mem_area(panel, &area, x_coord, y_coord);

    panel_write_cmd_data(panel, LCD_CMD_CASET, 8, x_coord, sizeof(x_coord));
    panel_write_cmd_data(panel, LCD_CMD_RASET, 8, y_coord, sizeof(y_coord));
}

static int st7789p3_get_capabilities(lisa_display_panel_t *panel, lisa_display_capabilities_t *caps)
{
    if (!caps) {
        return LISA_DEVICE_ERR_INVALID;
    }

    memcpy(caps, &panel->caps, sizeof(*caps));
    return LISA_DEVICE_OK;
}

static int st7789p3_write(lisa_display_panel_t *panel, uint16_t x, uint16_t y,
                         const lisa_display_buffer_desc_t *desc, const void *buf)
{
    if (!desc || !buf) {
        return LISA_DEVICE_ERR_INVALID;
    }

    st7789p3_set_window(panel, x, y, desc->width, desc->height);

    return panel_draw_pixels(panel, LCD_CMD_RAMWR, 8, x, y, desc->width, desc->height, buf);
}

static int st7789p3_blanking_on(lisa_display_panel_t *panel)
{
    return panel_write_cmd_data(panel, LCD_CMD_DISPLAY_OFF, 8, NULL, 0);
}

static int st7789p3_blanking_off(lisa_display_panel_t *panel)
{
    return panel_write_cmd_data(panel, LCD_CMD_DISPLAY_ON, 8, NULL, 0);
}

static int st7789p3_set_brightness(lisa_display_panel_t *panel, uint8_t brightness)
{
    return panel_set_backlight_brightness(&panel->backlight, brightness);
}

static int st7789p3_set_orientation(lisa_display_panel_t *panel, lisa_display_orientation_t orientation)
{
    struct panel_st7789p3_priv *priv = (struct panel_st7789p3_priv *)panel->priv_data;

    panel->caps.orientation = orientation;
    priv->orientation = orientation;

    return LISA_DEVICE_OK;
}

const lisa_display_panel_driver_t lisa_display_st7789p3_driver = {
    .init             = st7789p3_init,
    .write            = st7789p3_write,
    .blanking_on      = st7789p3_blanking_on,
    .blanking_off     = st7789p3_blanking_off,
    .set_brightness   = st7789p3_set_brightness,
    .set_orientation  = st7789p3_set_orientation,
    .get_capabilities = st7789p3_get_capabilities,
};

int panel_st7789p3_device_init(void)
{
    LISA_LOGD(LOG_TAG, "ST7789P3 device registered");
    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(lcd_panel, &lisa_display_st7789p3_driver, NULL, NULL, &panel_st7789p3_device_init, LISA_DEVICE_PRIORITY_HIGH);
