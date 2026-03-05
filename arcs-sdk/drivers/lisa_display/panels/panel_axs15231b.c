/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file panel_axs15231b.c
 * @brief AXS15231B QSPI LCD 面板驱动
 */

#include <string.h>

#include "lisa_display_panel.h"
#include "mipi_dcs.h"
#include "lisa_mem.h"
#include "lisa_device.h"

#define LOG_TAG "panel.axs15231b"
#include "lisa_log.h"

#define AXS15231B_CMD_BITS          32

#ifndef CONFIG_PANEL_AXS15231B_WIDTH
#define CONFIG_PANEL_AXS15231B_WIDTH 176
#endif

#ifndef CONFIG_PANEL_AXS15231B_HEIGHT
#define CONFIG_PANEL_AXS15231B_HEIGHT 560
#endif

#ifndef CONFIG_PANEL_AXS15231B_X_OFFSET
#define CONFIG_PANEL_AXS15231B_X_OFFSET 0
#endif

#ifndef CONFIG_PANEL_AXS15231B_Y_OFFSET
#define CONFIG_PANEL_AXS15231B_Y_OFFSET 0
#endif

#define AXS15231B_CMD_CONCAT_CMD(cmd) (((cmd) & 0xFF) << 8 | (LCD_OPCODE_WRITE_CMD << 24))
#define AXS15231B_CMD_CONCAT_IMG(cmd) (((cmd) & 0xFF) << 8 | (LCD_OPCODE_WRITE_IMG << 24))

#define AXS_DEFAULT_BACKLIGHT_FREQ 20000U

struct panel_axs15231b_priv {
    lisa_display_orientation_t orientation;
};

static int panel_axs15231b_init(lisa_display_panel_t *panel)
{
    // reset panel
    panel_reset_pin_set(panel, 1);
    lisa_thread_mdelay(10);
    panel_reset_pin_set(panel, 0);
    lisa_thread_mdelay(10);
    panel_reset_pin_set(panel, 1);
    lisa_thread_mdelay(10);

    panel_write_cmd_data(panel, AXS15231B_CMD_CONCAT_CMD(LCD_CMD_SLEEP_OUT), AXS15231B_CMD_BITS, NULL, 0);
    lisa_thread_mdelay(10);

    struct panel_axs15231b_priv *priv = lisa_mem_alloc(sizeof(*priv));
    if (!priv) {
        return LISA_DEVICE_ERR_NO_MEM;
    }

    panel->priv_data = priv;
    panel->caps.width = CONFIG_PANEL_AXS15231B_WIDTH;
    panel->caps.height = CONFIG_PANEL_AXS15231B_HEIGHT;
    panel->caps.pixel_format = LISA_DISPLAY_PIXEL_FORMAT_RGB_565;
    panel->caps.orientation = LISA_DISPLAY_ORIENTATION_0;
    panel->caps.supported_pixel_formats = (1U << LISA_DISPLAY_PIXEL_FORMAT_RGB_565);
    priv->orientation = LISA_DISPLAY_ORIENTATION_0;

    LISA_LOGI(LOG_TAG, "AXS15231B initialized");
    return LISA_DEVICE_OK;
}

static void panel_axs15231b_set_window(lisa_display_panel_t *panel, uint16_t x, uint16_t y,
                                 uint16_t w, uint16_t h)
{
    lisa_mem_coord_t x_coord, y_coord;

    lisa_display_panel_mem_area_t area = {
        .panel_w  = panel->caps.width,
        .panel_h  = panel->caps.height,
        .x_offset = CONFIG_PANEL_AXS15231B_X_OFFSET,
        .y_offset = CONFIG_PANEL_AXS15231B_Y_OFFSET,
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };
    panel_set_mem_area(panel, &area, x_coord, y_coord);

    panel_write_cmd_data(panel, AXS15231B_CMD_CONCAT_CMD(LCD_CMD_CASET), AXS15231B_CMD_BITS, x_coord, sizeof(x_coord));
    panel_write_cmd_data(panel, AXS15231B_CMD_CONCAT_CMD(LCD_CMD_RASET), AXS15231B_CMD_BITS, y_coord, sizeof(y_coord));
}

static int panel_axs15231b_get_capabilities(lisa_display_panel_t *panel, lisa_display_capabilities_t *caps)
{
    if (!caps) {
        return LISA_DEVICE_ERR_INVALID;
    }
    memcpy(caps, &panel->caps, sizeof(*caps));
    return LISA_DEVICE_OK;
}

static int panel_axs15231b_write(lisa_display_panel_t *panel, uint16_t x, uint16_t y,
                      const lisa_display_buffer_desc_t *desc, const void *buf)
{
    if (!desc || !buf) {
        return LISA_DEVICE_ERR_INVALID;
    }

    panel_axs15231b_set_window(panel, x, y, desc->width, desc->height);

    return panel_draw_pixels(panel, AXS15231B_CMD_CONCAT_IMG(LCD_CMD_RAMWR), AXS15231B_CMD_BITS, x, y, desc->width, desc->height, buf);
}

static int panel_axs15231b_blanking_on(lisa_display_panel_t *panel)
{
    return panel_write_cmd_data(panel, AXS15231B_CMD_CONCAT_CMD(LCD_CMD_DISPLAY_OFF), AXS15231B_CMD_BITS, NULL, 0);
}

static int panel_axs15231b_blanking_off(lisa_display_panel_t *panel)
{
    return panel_write_cmd_data(panel, AXS15231B_CMD_CONCAT_CMD(LCD_CMD_DISPLAY_ON), AXS15231B_CMD_BITS, NULL, 0);
}

static int panel_axs15231b_set_brightness(lisa_display_panel_t *panel, uint8_t brightness)
{
    return panel_set_backlight_brightness(&panel->backlight, brightness);
}

static int panel_axs15231b_set_orientation(lisa_display_panel_t *panel, lisa_display_orientation_t orientation)
{
    struct panel_axs15231b_priv *priv = (struct panel_axs15231b_priv *)panel->priv_data;

    panel->caps.orientation = orientation;
    priv->orientation = orientation;

    return LISA_DEVICE_OK;
}

const lisa_display_panel_driver_t lisa_display_axs15231b_driver = {
    .init             = panel_axs15231b_init,
    .write            = panel_axs15231b_write,
    .blanking_on      = panel_axs15231b_blanking_on,
    .blanking_off     = panel_axs15231b_blanking_off,
    .set_brightness   = panel_axs15231b_set_brightness,
    .set_orientation  = panel_axs15231b_set_orientation,
    .get_capabilities = panel_axs15231b_get_capabilities,
};

int panel_axs15231b_device_init(void)
{
    LISA_LOGD(LOG_TAG, "AXS15231B device registered");
    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(lcd_panel, &lisa_display_axs15231b_driver, NULL, NULL, &panel_axs15231b_device_init, LISA_DEVICE_PRIORITY_HIGH);