/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file panel_gc9307.c
 * @brief GC9307 SPI LCD 面板驱动
 */

#include <string.h>
#include "lisa_display_panel.h"
#include "lisa_mem.h"
#include "lisa_device.h"
#include "lisa_thread.h"
#include "mipi_dcs.h"
#include "lisa_gpio.h"

#define LOG_TAG "panel.gc9307"
#include "lisa_log.h"

#define GC9307_DEFAULT_MADCTL (MADCTL_MX | MADCTL_BGR)

struct panel_gc9307_priv {
    lisa_display_orientation_t orientation;
};

/* VIEWE UEDX24320024/28/35E-WB-A GC9307 240x320 SPI 初始化序列。 */
static const uint8_t init_sequence[] = {
    0xFE, 0,
    0xEF, 0,
    LCD_CMD_MADCTL, 1, GC9307_DEFAULT_MADCTL,
    LCD_CMD_COLMOD, 1, COLMOD_RGB_16BIT,
    0x86, 1, 0x98,
    0x89, 1, 0x13,
    0x8B, 1, 0x80,
    0x8D, 1, 0x33,
    0x8E, 1, 0x0F,
    0xE8, 2, 0x12, 0x00,
    0xEC, 3, 0x13, 0x02, 0x88,
    0xFF, 1, 0x62,
    0x99, 1, 0x3E,
    0x9D, 1, 0x4B,
    0x98, 1, 0x3E,
    0x9C, 1, 0x4B,
    0xC3, 1, 0x27,
    0xC4, 1, 0x18,
    0xC9, 1, 0x0A,
    0xF0, 6, 0x47, 0x0C, 0x0A, 0x09, 0x15, 0x33,
    0xF1, 6, 0x4B, 0x8F, 0x8F, 0x3B, 0x3F, 0x6F,
    0xF2, 6, 0x47, 0x0C, 0x0A, 0x09, 0x15, 0x33,
    0xF3, 6, 0x4B, 0x8F, 0x8F, 0x3B, 0x3F, 0x6F,
};

static int gc9307_send_init_sequence(lisa_display_panel_t *panel)
{
    const uint8_t *seq = (const uint8_t *)panel->init_params;
    size_t seq_len = panel->init_params_len;
    if (!seq || seq_len == 0) {
        seq = init_sequence;
        seq_len = sizeof(init_sequence);
    }

    const uint8_t *p = seq;
    const uint8_t *end = seq + seq_len;
    while (p + 2 <= end) {
        uint8_t cmd = *p++;
        uint8_t len = *p++;
        if (p + len > end) {
            LISA_LOGW(LOG_TAG, "Init sequence truncated for cmd 0x%02x", cmd);
            return LISA_DEVICE_ERR_INVALID;
        }
        int ret = panel_write_cmd_data(panel, cmd, 8, p, len);
        if (ret != LISA_DEVICE_OK) {
            return ret;
        }
        p += len;
    }

    return LISA_DEVICE_OK;
}

static int gc9307_init(lisa_display_panel_t *panel)
{
    struct panel_gc9307_priv *priv = lisa_mem_alloc(sizeof(*priv));
    int ret;

    if (!priv) {
        return LISA_DEVICE_ERR_NO_MEM;
    }

    panel->priv_data = priv;
    panel->caps.width = CONFIG_PANEL_GC9307_WIDTH;
    panel->caps.height = CONFIG_PANEL_GC9307_HEIGHT;
    panel->caps.pixel_format = LISA_DISPLAY_PIXEL_FORMAT_RGB_565;
    panel->caps.orientation = LISA_DISPLAY_ORIENTATION_0;
    panel->caps.supported_pixel_formats = (1U << LISA_DISPLAY_PIXEL_FORMAT_RGB_565);
    priv->orientation = LISA_DISPLAY_ORIENTATION_0;

    if (panel->rst_gpio) {
        lisa_gpio_write_pin(panel->rst_gpio, panel->rst_pin, 1);
        lisa_thread_mdelay(10);
        lisa_gpio_write_pin(panel->rst_gpio, panel->rst_pin, 0);
        lisa_thread_mdelay(20);
        lisa_gpio_write_pin(panel->rst_gpio, panel->rst_pin, 1);
        lisa_thread_mdelay(20);
    } else {
        ret = panel_write_cmd_data(panel, MIPI_DCS_SOFT_RESET, 8, NULL, 0);
        if (ret != LISA_DEVICE_OK) {
            goto err;
        }
        lisa_thread_mdelay(20);
    }

    ret = gc9307_send_init_sequence(panel);
    if (ret != LISA_DEVICE_OK) {
        goto err;
    }

#ifdef CONFIG_LISA_DISPLAY_COLOR_INVERT
    ret = panel_write_cmd_data(panel, LCD_CMD_INVERT_ON, 8, NULL, 0);
#else
    ret = panel_write_cmd_data(panel, LCD_CMD_INVERT_OFF, 8, NULL, 0);
#endif
    if (ret != LISA_DEVICE_OK) {
        goto err;
    }
    ret = panel_write_cmd_data(panel, LCD_CMD_SLEEP_OUT, 8, NULL, 0);
    if (ret != LISA_DEVICE_OK) {
        goto err;
    }
    lisa_thread_mdelay(120);

    LISA_LOGI(LOG_TAG, "GC9307 initialized");
    return LISA_DEVICE_OK;

err:
    lisa_mem_free(priv);
    panel->priv_data = NULL;
    return ret;
}

static void gc9307_set_window(lisa_display_panel_t *panel, uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h)
{
    lisa_mem_coord_t x_coord, y_coord;
    lisa_display_panel_mem_area_t area = {
        .panel_w = panel->caps.width,
        .panel_h = panel->caps.height,
        .x_offset = CONFIG_PANEL_GC9307_X_OFFSET,
        .y_offset = CONFIG_PANEL_GC9307_Y_OFFSET,
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };

    panel_set_mem_area(panel, &area, x_coord, y_coord);
    panel_write_cmd_data(panel, LCD_CMD_CASET, 8, x_coord, sizeof(x_coord));
    panel_write_cmd_data(panel, LCD_CMD_RASET, 8, y_coord, sizeof(y_coord));
}

static int gc9307_get_capabilities(lisa_display_panel_t *panel, lisa_display_capabilities_t *caps)
{
    if (!caps) {
        return LISA_DEVICE_ERR_INVALID;
    }

    memcpy(caps, &panel->caps, sizeof(*caps));
    return LISA_DEVICE_OK;
}

static int gc9307_write(lisa_display_panel_t *panel, uint16_t x, uint16_t y,
                        const lisa_display_buffer_desc_t *desc, const void *buf)
{
    if (!desc || !buf) {
        return LISA_DEVICE_ERR_INVALID;
    }

    gc9307_set_window(panel, x, y, desc->width, desc->height);
    return panel_draw_pixels(panel, LCD_CMD_RAMWR, 8, x, y, desc->width, desc->height, buf);
}

static int gc9307_blanking_on(lisa_display_panel_t *panel)
{
    return panel_write_cmd_data(panel, LCD_CMD_DISPLAY_OFF, 8, NULL, 0);
}

static int gc9307_blanking_off(lisa_display_panel_t *panel)
{
    return panel_write_cmd_data(panel, LCD_CMD_DISPLAY_ON, 8, NULL, 0);
}

static int gc9307_set_brightness(lisa_display_panel_t *panel, uint8_t brightness)
{
    return panel_set_backlight_brightness(&panel->backlight, brightness);
}

static int gc9307_set_orientation(lisa_display_panel_t *panel, lisa_display_orientation_t orientation)
{
    struct panel_gc9307_priv *priv = (struct panel_gc9307_priv *)panel->priv_data;

    panel->caps.orientation = orientation;
    if (priv) {
        priv->orientation = orientation;
    }

    return LISA_DEVICE_OK;
}

const lisa_display_panel_driver_t lisa_display_gc9307_driver = {
    .init = gc9307_init,
    .write = gc9307_write,
    .blanking_on = gc9307_blanking_on,
    .blanking_off = gc9307_blanking_off,
    .set_brightness = gc9307_set_brightness,
    .set_orientation = gc9307_set_orientation,
    .get_capabilities = gc9307_get_capabilities,
};

int panel_gc9307_device_init(void)
{
    LISA_LOGD(LOG_TAG, "GC9307 device registered");
    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(gc9307, &lisa_display_gc9307_driver, NULL, NULL, &panel_gc9307_device_init, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_HIGH);
