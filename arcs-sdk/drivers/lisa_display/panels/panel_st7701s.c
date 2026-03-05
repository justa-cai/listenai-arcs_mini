
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

#define LOG_TAG "panel.st7701s"
#include "lisa_log.h"

/* ST7701S 特定命令（非标准 MIPI DCS）*/
// 可在此添加芯片特定命令宏

/* 私有数据结构 */
struct panel_st7701s_priv {
    lisa_display_orientation_t orientation;
};


/* 初始化序列 */
static const uint8_t init_sequence[] = {
    0xFF, 5,  0x77, 0x01, 0x00, 0x00, 0x13,
    0xEF, 1,  0x08,
    0xFF, 5,  0x77, 0x01, 0x00, 0x00, 0x10,
    0xC0, 2,  0x3B, 0x00,
    0xC1, 2,  0x0B, 0x02,
    0xC2, 2,  0x37, 0x02,
    0xCC, 1,  0x10,
    0xB0, 16, 0x00, 0x0F, 0x16, 0x0E, 0x11, 0x07, 0x09, 0x09, 0x08, 0x23, 0x05, 0x11, 0x0F, 0x28, 0x2D, 0x18,
    0xB1, 16, 0x00, 0x0F, 0x16, 0x0E, 0x11, 0x07, 0x09, 0x08, 0x09, 0x23, 0x05, 0x11, 0x0F, 0x28, 0x2D, 0x18,
    0xFF, 5,  0x77, 0x01, 0x00, 0x00, 0x11,
    0xB0, 1,  0x4D,
    0xB1, 1,  0x33,
    0xB2, 1,  0x87,
    0xB5, 1,  0x4B,
    0xB7, 1,  0x8C,
    0xB8, 1,  0x20,
    0xC1, 1,  0x78,
    0xC2, 1,  0x78,
    0xD0, 1,  0x88,
    0xE0, 3,  0x00, 0x00, 0x02,
    0xE1, 11, 0x02, 0xF0, 0x00, 0x00, 0x03, 0xF0, 0x00, 0x00, 0x00, 0x44, 0x44,
    0xE2, 12, 0x10, 0x10, 0x40, 0x40, 0xF2, 0xF0, 0x00, 0x00, 0xF2, 0xF0, 0x00, 0x00,
    0xE3, 4,  0x00, 0x00, 0x11, 0x11,
    0xE4, 2,  0x44, 0x44,
    0xE5, 16, 0x07, 0xEF, 0xF0, 0xF0, 0x09, 0xF1, 0xF0, 0xF0, 0x03, 0xF3, 0xF0, 0xF0, 0x05, 0xED, 0xF0, 0xF0,
    0xE6, 4,  0x00, 0x00, 0x11, 0x11,
    0xE7, 2,  0x44, 0x44,
    0xE8, 16, 0x08, 0xF0, 0xF0, 0xF0, 0x0A, 0xF2, 0xF0, 0xF0, 0x04, 0xF4, 0xF0, 0xF0, 0x06, 0xEE, 0xF0, 0xF0,
    0xEB, 7,  0x00, 0x00, 0xE4, 0xE4, 0x44, 0x88, 0x40,
    0xEC, 2,  0x78, 0x00,
    0xED, 16, 0x20, 0xF9, 0x87, 0x76, 0x65, 0x54, 0x4F, 0xFF, 0xFF, 0xF4, 0x45, 0x56, 0x67, 0x78, 0x9F, 0x02,
    0xEF, 6,  0x10, 0x0D, 0x04, 0x08, 0x3F, 0x1F
};

/* ========================================================================
 *  lisa_display_api_t 实现
 * ======================================================================== */

static int st7701s_init(lisa_display_panel_t *panel)
{
    // 分配私有数据
    struct panel_st7701s_priv *priv = NULL;
    priv = lisa_mem_alloc(sizeof(*priv));
    if (!priv) {
        return LISA_DEVICE_ERR_NO_MEM;
    }

    panel->priv_data         = priv;
    panel->caps.width        = CONFIG_PANEL_ST7701S_WIDTH;
    panel->caps.height       = CONFIG_PANEL_ST7701S_HEIGHT;
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

    LISA_LOGI(LOG_TAG, "st7701s initialized");
    return LISA_DEVICE_OK;
}

static int st7701s_get_capabilities(lisa_display_panel_t *panel, lisa_display_capabilities_t *caps)
{
    if (!caps) {
        return LISA_DEVICE_ERR_INVALID;
    }

    memcpy(caps, &panel->caps, sizeof(*caps));
    return LISA_DEVICE_OK;
}

static int st7701s_write(lisa_display_panel_t *panel, uint16_t x, uint16_t y,
                         const lisa_display_buffer_desc_t *desc, const void *buf)
{
    if (!desc || !buf) {
        return LISA_DEVICE_ERR_INVALID;
    }

    return panel_draw_pixels(panel, LCD_CMD_RAMWR, 8, x, y, desc->width, desc->height, buf);
}

static int st7701s_blanking_on(lisa_display_panel_t *panel)
{
    return panel_write_cmd_data(panel, LCD_CMD_DISPLAY_OFF, 8, NULL, 0);
}

static int st7701s_blanking_off(lisa_display_panel_t *panel)
{
    return panel_write_cmd_data(panel, LCD_CMD_DISPLAY_ON, 8, NULL, 0);
}

static int st7701s_set_brightness(lisa_display_panel_t *panel, uint8_t brightness)
{
    return panel_set_backlight_brightness(&panel->backlight, brightness);
}

static int st7701s_set_orientation(lisa_display_panel_t *panel, lisa_display_orientation_t orientation)
{
    struct panel_st7701s_priv *priv = (struct panel_st7701s_priv *)panel->priv_data;

    panel->caps.orientation = orientation;
    priv->orientation = orientation;

    return LISA_DEVICE_OK;
}

const lisa_display_panel_driver_t lisa_display_st7701s_driver = {
    .init             = st7701s_init,
    .write            = st7701s_write,
    .blanking_on      = st7701s_blanking_on,
    .blanking_off     = st7701s_blanking_off,
    .set_brightness   = st7701s_set_brightness,
    .set_orientation  = st7701s_set_orientation,
    .get_capabilities = st7701s_get_capabilities,
};

int panel_st7701s_device_init(void)
{
    LISA_LOGI(LOG_TAG, "st7701s device registered");
    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(lcd_panel, &lisa_display_st7701s_driver, NULL, NULL, &panel_st7701s_device_init, LISA_DEVICE_PRIORITY_HIGH);
