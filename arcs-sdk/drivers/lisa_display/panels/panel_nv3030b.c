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

#define LOG_TAG "panel.nv3030b"
#include "lisa_log.h"

/* 私有数据结构 */
struct panel_nv3030b_priv {
    lisa_display_orientation_t orientation;
};

/* NV3030B 初始化序列（从旧版 SDK 移植） */
static const uint8_t init_sequence[] = {
    // cmd, len, data...
    0xFD, 2, 0x06, 0x08,
    0x61, 2, 0x07, 0x07,
    0x73, 1, 0x70,
    0x73, 1, 0x00,
    0x62, 4, 0x00, 0x44, 0x40, 0x01,
    0x63, 4, 0x41, 0x07, 0x12, 0x12,
    0x65, 3, 0x09, 0x17, 0x21,
    0x66, 3, 0x09, 0x17, 0x21,
    0x67, 2, 0x20, 0x40,
    0x68, 4, 0x90, 0x30, 0x32, 0x26,
    0xB1, 3, 0x0F, 0x02, 0x01,
    0xB4, 1, 0x01,
    0xB5, 4, 0x02, 0x02, 0x0A, 0x14,
    0xB6, 5, 0x04, 0x01, 0x9F, 0x00, 0x02,
    0xDF, 1, 0x11,
    0xE2, 6, 0x00, 0x05, 0x07, 0x24, 0x33, 0x3F,
    0xE5, 6, 0x3F, 0x33, 0x26, 0x09, 0x07, 0x00,
    0xE1, 2, 0x16, 0x5B,
    0xE4, 2, 0x5B, 0x18,
    0xE0, 8, 0x06, 0x07, 0x0D, 0x0F, 0x0F, 0x10, 0x12, 0x17,
    0xE3, 8, 0x19, 0x14, 0x11, 0x0F, 0x12, 0x0F, 0x07, 0x06,
    0xE6, 2, 0x00, 0xFF,
    0xE7, 6, 0x01, 0x04, 0x03, 0x03, 0x00, 0x12,
    0xE8, 3, 0x00, 0x70, 0x00,
    0xEC, 1, 0x50,
    0xF1, 1, 0x00,
    0xFD, 2, 0xFA, 0xFC,
    0x3A, 1, 0x55,   // COLMOD: RGB565
    0x35, 1, 0x00,   // TE ON
    0x36, 1, 0xC0,   // MADCTL
};

/* ========================================================================
 *  lisa_display_panel_driver_t 实现
 * ======================================================================== */

static int nv3030b_init(lisa_display_panel_t *panel)
{
    struct panel_nv3030b_priv *priv = lisa_mem_alloc(sizeof(*priv));
    if (!priv) {
        return LISA_DEVICE_ERR_NO_MEM;
    }

    panel->priv_data         = priv;
    panel->caps.width        = CONFIG_PANEL_NV3030B_WIDTH;
    panel->caps.height       = CONFIG_PANEL_NV3030B_HEIGHT;
    panel->caps.pixel_format = LISA_DISPLAY_PIXEL_FORMAT_RGB_565;
    panel->caps.orientation  = LISA_DISPLAY_ORIENTATION_0;
    panel->caps.supported_pixel_formats = (1U << LISA_DISPLAY_PIXEL_FORMAT_RGB_565);
    priv->orientation = LISA_DISPLAY_ORIENTATION_0;

    /* 硬件复位（NV3030B 需要较长的低电平时间） */
    if (panel->rst_gpio) {
        lisa_gpio_write_pin(panel->rst_gpio, panel->rst_pin, 1);
        lisa_thread_mdelay(10);
        lisa_gpio_write_pin(panel->rst_gpio, panel->rst_pin, 0);
        lisa_thread_mdelay(200);
        lisa_gpio_write_pin(panel->rst_gpio, panel->rst_pin, 1);
        lisa_thread_mdelay(120);
    }

    /* 退出睡眠模式 */
    panel_write_cmd_data(panel, LCD_CMD_SLEEP_OUT, 8, NULL, 0);
    lisa_thread_mdelay(120);

    /* 发送初始化序列：优先使用 attach 时传入的 init_params，否则使用内置序列 */
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
            break;
        }
        panel_write_cmd_data(panel, cmd, 8, p, len);
        p += len;
    }

#ifdef CONFIG_LISA_DISPLAY_COLOR_INVERT
    panel_write_cmd_data(panel, LCD_CMD_INVERT_ON, 8, NULL, 0);
#else
    panel_write_cmd_data(panel, LCD_CMD_INVERT_OFF, 8, NULL, 0);
#endif

    LISA_LOGI(LOG_TAG, "NV3030B initialized");
    return LISA_DEVICE_OK;
}

static void nv3030b_set_window(lisa_display_panel_t *panel, uint16_t x, uint16_t y,
                               uint16_t w, uint16_t h)
{
    lisa_mem_coord_t x_coord, y_coord;

    lisa_display_panel_mem_area_t area = {
        .panel_w = panel->caps.width,
        .panel_h = panel->caps.height,
        .x_offset = CONFIG_PANEL_NV3030B_X_OFFSET,
        .y_offset = CONFIG_PANEL_NV3030B_Y_OFFSET,
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };
    panel_set_mem_area(panel, &area, x_coord, y_coord);

    panel_write_cmd_data(panel, LCD_CMD_CASET, 8, x_coord, sizeof(x_coord));
    panel_write_cmd_data(panel, LCD_CMD_RASET, 8, y_coord, sizeof(y_coord));
}

static int nv3030b_get_capabilities(lisa_display_panel_t *panel, lisa_display_capabilities_t *caps)
{
    if (!caps) {
        return LISA_DEVICE_ERR_INVALID;
    }

    memcpy(caps, &panel->caps, sizeof(*caps));
    return LISA_DEVICE_OK;
}

static int nv3030b_write(lisa_display_panel_t *panel, uint16_t x, uint16_t y,
                         const lisa_display_buffer_desc_t *desc, const void *buf)
{
    if (!desc || !buf) {
        return LISA_DEVICE_ERR_INVALID;
    }

    nv3030b_set_window(panel, x, y, desc->width, desc->height);

    return panel_draw_pixels(panel, LCD_CMD_RAMWR, 8, x, y, desc->width, desc->height, buf);
}

static int nv3030b_blanking_on(lisa_display_panel_t *panel)
{
    return panel_write_cmd_data(panel, LCD_CMD_DISPLAY_OFF, 8, NULL, 0);
}

static int nv3030b_blanking_off(lisa_display_panel_t *panel)
{
    return panel_write_cmd_data(panel, LCD_CMD_DISPLAY_ON, 8, NULL, 0);
}

static int nv3030b_set_brightness(lisa_display_panel_t *panel, uint8_t brightness)
{
    return panel_set_backlight_brightness(&panel->backlight, brightness);
}

static int nv3030b_set_orientation(lisa_display_panel_t *panel, lisa_display_orientation_t orientation)
{
    struct panel_nv3030b_priv *priv = (struct panel_nv3030b_priv *)panel->priv_data;

    panel->caps.orientation = orientation;
    priv->orientation = orientation;

    return LISA_DEVICE_OK;
}

const lisa_display_panel_driver_t lisa_display_nv3030b_driver = {
    .init             = nv3030b_init,
    .write            = nv3030b_write,
    .blanking_on      = nv3030b_blanking_on,
    .blanking_off     = nv3030b_blanking_off,
    .set_brightness   = nv3030b_set_brightness,
    .set_orientation  = nv3030b_set_orientation,
    .get_capabilities = nv3030b_get_capabilities,
};

int panel_nv3030b_device_init(void)
{
    LISA_LOGD(LOG_TAG, "NV3030B device registered");
    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(nv3030b, &lisa_display_nv3030b_driver, NULL, NULL, &panel_nv3030b_device_init, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_HIGH);
