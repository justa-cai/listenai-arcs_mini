#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cache.h"

#include "lisa_device.h"
#include "lisa_display_panel.h"
#include "lisa_gpio.h"
#include "lisa_mem.h"
#include "lisa_thread.h"
#include "mipi_dcs.h"

#if CONFIG_LISA_DISPLAY_PANEL_GC9A01
#define LOG_TAG "panel.gc9a01"
#elif CONFIG_LISA_DISPLAY_PANEL_GC9D01N
#define LOG_TAG "panel.gc9d01n"
#else
#error "No panel type defined"
#endif
#include "lisa_log.h"

struct panel_gc9a01_priv {
    lisa_display_orientation_t orientation;
};

#define DISP_INIT_ITEM(cmd, ...) cmd, sizeof((uint8_t[]){__VA_ARGS__}), __VA_ARGS__

static const uint8_t init_sequence[] = {
#if CONFIG_LISA_DISPLAY_PANEL_GC9A01
    DISP_INIT_ITEM(0xEF, 0),
    DISP_INIT_ITEM(0xEB, 0, 0x14),

    DISP_INIT_ITEM(0xFE, 0),
    DISP_INIT_ITEM(0xEF, 0),

    DISP_INIT_ITEM(0xEB, 0, 0x14),
    DISP_INIT_ITEM(0x84, 0, 0x40),

    DISP_INIT_ITEM(0x85, 0, 0xFF),
    DISP_INIT_ITEM(0x86, 0, 0xFF),
    DISP_INIT_ITEM(0x87, 0, 0xFF),
    DISP_INIT_ITEM(0x88, 0, 0x0A),
    DISP_INIT_ITEM(0x89, 0, 0x21),
    DISP_INIT_ITEM(0x8A, 0, 0x00),
    DISP_INIT_ITEM(0x8B, 0, 0x80),
    DISP_INIT_ITEM(0x8C, 0, 0x01),
    DISP_INIT_ITEM(0x8D, 0, 0x01),
    DISP_INIT_ITEM(0x8E, 0, 0xFF),
    DISP_INIT_ITEM(0x8F, 0, 0xFF),

    DISP_INIT_ITEM(0xB6, 0, 0x00, 0x00),
    DISP_INIT_ITEM(0x36, 0, 0x88),
    DISP_INIT_ITEM(0x3A, 0, 0x05),

    DISP_INIT_ITEM(0x90, 0, 0x08, 0x08, 0x08, 0x08),
    DISP_INIT_ITEM(0xBD, 0, 0x06),
    DISP_INIT_ITEM(0xBC, 0, 0x00),

    DISP_INIT_ITEM(0xFF, 0, 0x00, 0x60, 0x01, 0x04),

    DISP_INIT_ITEM(0xC3, 0, 0x13),
    DISP_INIT_ITEM(0xC4, 0, 0x13),

    DISP_INIT_ITEM(0xC9, 0, 0x22),
    DISP_INIT_ITEM(0xBE, 0, 0x11),

    DISP_INIT_ITEM(0xE1, 0, 0x10, 0x0E),
    DISP_INIT_ITEM(0xDF, 0, 0x21, 0x0c, 0x02),

    DISP_INIT_ITEM(0xF0, 0, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A),
    DISP_INIT_ITEM(0xF1, 0, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F),
    DISP_INIT_ITEM(0xF2, 0, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A),
    DISP_INIT_ITEM(0xF3, 0, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F),

    DISP_INIT_ITEM(0xED, 0, 0x1B, 0x0B),
    DISP_INIT_ITEM(0xAE, 0, 0x77),
    DISP_INIT_ITEM(0xCD, 0, 0x63),

    DISP_INIT_ITEM(0x70, 0, 0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03),

    DISP_INIT_ITEM(0xE8, 0, 0x34),

    DISP_INIT_ITEM(0x62, 0, 0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70),
    DISP_INIT_ITEM(0x63, 0, 0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70),
    DISP_INIT_ITEM(0x64, 0, 0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07),

    DISP_INIT_ITEM(0x66, 0, 0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00),
    DISP_INIT_ITEM(0x67, 0, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98),

    DISP_INIT_ITEM(0x74, 0, 0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00),

    DISP_INIT_ITEM(0x98, 0, 0x3e, 0x07),

    DISP_INIT_ITEM(0x35, 0, 0x00),
    DISP_INIT_ITEM(0x21, 0),
    DISP_INIT_ITEM(0x11, 20),
    DISP_INIT_ITEM(0x29, 120),
#elif CONFIG_LISA_DISPLAY_PANEL_GC9D01N
    DISP_INIT_ITEM(0xFE, 0),
    DISP_INIT_ITEM(0xEF, 0),
    DISP_INIT_ITEM(0x80, 0, 0xFF),
    DISP_INIT_ITEM(0x81, 0, 0xFF),
    DISP_INIT_ITEM(0x82, 0, 0xFF),
    DISP_INIT_ITEM(0x84, 0, 0xFF),
    DISP_INIT_ITEM(0x85, 0, 0xFF),
    DISP_INIT_ITEM(0x86, 0, 0xFF),
    DISP_INIT_ITEM(0x87, 0, 0xFF),
    DISP_INIT_ITEM(0x88, 0, 0xFF),
    DISP_INIT_ITEM(0x89, 0, 0xFF),
    DISP_INIT_ITEM(0x8A, 0, 0xFF),
    DISP_INIT_ITEM(0x8B, 0, 0xFF),
    DISP_INIT_ITEM(0x8C, 0, 0xFF),
    DISP_INIT_ITEM(0x8D, 0, 0xFF),
    DISP_INIT_ITEM(0x8E, 0, 0xFF),
    DISP_INIT_ITEM(0x8F, 0, 0xFF),
    DISP_INIT_ITEM(0x3A, 0, 0x05),
    DISP_INIT_ITEM(0xEC, 0, 0x01),
    DISP_INIT_ITEM(0x74, 0, 0x02, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00),
    DISP_INIT_ITEM(0x98, 0, 0x3E),
    DISP_INIT_ITEM(0x99, 0, 0x3E),
    DISP_INIT_ITEM(0xB5, 0, 0x0D, 0x0D),
    DISP_INIT_ITEM(0x60, 0, 0x38, 0x0F, 0x79, 0x67),
    DISP_INIT_ITEM(0x61, 0, 0x38, 0x11, 0x79, 0x67),
    DISP_INIT_ITEM(0x64, 0, 0x38, 0x17, 0x71, 0x5F, 0x79, 0x67),
    DISP_INIT_ITEM(0x65, 0, 0x38, 0x13, 0x71, 0x5B, 0x79, 0x67),
    DISP_INIT_ITEM(0x6A, 0, 0x00, 0x00),
    DISP_INIT_ITEM(0x6C, 0, 0x22, 0x02, 0x22, 0x02, 0x22, 0x22, 0x50),
    DISP_INIT_ITEM(0x6E, 0, 0x03, 0x03, 0x01, 0x01, 0x00, 0x00, 0x0F, 0x0F, 0x0D, 0x0D, 0x0B, 0x0B, 0x09, 0x09, 0x00,
                   0x00, 0x00, 0x00, 0x0A, 0x0A, 0x0C, 0x0C, 0x0E, 0x0E, 0x10, 0x10, 0x00, 0x00, 0x02, 0x02, 0x04,
                   0x04),
    DISP_INIT_ITEM(0xBF, 0, 0x01),
    DISP_INIT_ITEM(0xF9, 0, 0x40),
    DISP_INIT_ITEM(0x9B, 0, 0x3B, 0x93, 0x33, 0x7F, 0x00),
    DISP_INIT_ITEM(0x7E, 0, 0x30),
    DISP_INIT_ITEM(0x70, 0, 0x0D, 0x02, 0x08, 0x0D, 0x02, 0x08),
    DISP_INIT_ITEM(0x71, 0, 0x0D, 0x02, 0x08),
    DISP_INIT_ITEM(0x91, 0, 0x0E, 0x09),
    DISP_INIT_ITEM(0xC3, 0, 0x19, 0xC4, 0x19, 0xC9, 0x3C),
    DISP_INIT_ITEM(0xF0, 0, 0x53, 0x15, 0x0A, 0x04, 0x00, 0x3E),
    DISP_INIT_ITEM(0xF1, 0, 0x56, 0xA8, 0x7F, 0x33, 0x34, 0x5F),
    DISP_INIT_ITEM(0xF2, 0, 0x53, 0x15, 0x0A, 0x04, 0x00, 0x3A),
    DISP_INIT_ITEM(0xF3, 0, 0x52, 0xA4, 0x7F, 0x33, 0x34, 0xDF),

    DISP_INIT_ITEM(0x36, 0, 0x00),
    DISP_INIT_ITEM(0x11, 200),
    DISP_INIT_ITEM(0x29, 0),
    DISP_INIT_ITEM(0x2C, 20),
#endif
};

static int gc9a01_init(lisa_display_panel_t *panel)
{
    struct panel_gc9a01_priv *priv = NULL;
    priv = lisa_mem_alloc(sizeof(*priv));
    if (!priv) {
        return LISA_DEVICE_ERR_NO_MEM;
    }

    panel->priv_data = priv;
#ifdef CONFIG_LISA_DISPLAY_PANEL_GC9A01
    panel->caps.width = CONFIG_PANEL_GC9A01_WIDTH;
    panel->caps.height = CONFIG_PANEL_GC9A01_HEIGHT;
#elif CONFIG_LISA_DISPLAY_PANEL_GC9D01N
    panel->caps.width = CONFIG_PANEL_GC9D01N_WIDTH;
    panel->caps.height = CONFIG_PANEL_GC9D01N_HEIGHT;
#endif
    panel->caps.pixel_format = LISA_DISPLAY_PIXEL_FORMAT_RGB_565;
    panel->caps.orientation = LISA_DISPLAY_ORIENTATION_0;
    panel->caps.supported_pixel_formats = (1U << LISA_DISPLAY_PIXEL_FORMAT_RGB_565);
    priv->orientation = LISA_DISPLAY_ORIENTATION_0;

    if (panel->rst_gpio) {
        lisa_gpio_write_pin(panel->rst_gpio, panel->rst_pin, 1);
        lisa_thread_mdelay(10);
        lisa_gpio_write_pin(panel->rst_gpio, panel->rst_pin, 0);
        lisa_thread_mdelay(10);
        lisa_gpio_write_pin(panel->rst_gpio, panel->rst_pin, 1);
        lisa_thread_mdelay(120);
    }

    panel_write_cmd_data(panel, LCD_CMD_SLEEP_OUT, 8, NULL, 0);
    lisa_thread_mdelay(120);

    // for (uint32_t i = 0; i < sizeof(disp_init_items) / sizeof(disp_init_items[0]); i++) {
    //     display_trans_cmd_data(disp_init_items[i].cmd, (uint8_t *)disp_init_items[i].data,
    //                            disp_init_items[i].data_bytes);
    // }

    const uint8_t *p = init_sequence;
    while (p < init_sequence + sizeof(init_sequence)) {
        uint8_t cmd = *p++;
        uint8_t len = *p++ - 1;
        uint8_t delay = *p++;
        panel_write_cmd_data(panel, cmd, 8, p, len);
        p += len;
        if (delay) {
            lisa_thread_mdelay(delay);
        }
    }

#if CONFIG_LISA_DISPLAY_PANEL_GC9A01
    panel_write_cmd_data(panel, LCD_CMD_INVERT_ON, 8, NULL, 0);
#elif CONFIG_LISA_DISPLAY_PANEL_GC9D01N
    panel_write_cmd_data(panel, LCD_CMD_INVERT_OFF, 8, NULL, 0);
#endif

    LISA_LOGI(LOG_TAG, "GC9A01/GC9D01N initialization completed");
    return LISA_DEVICE_OK;
}

static void gc9a01_set_window(lisa_display_panel_t *panel, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    lisa_mem_coord_t x_coord, y_coord;

    lisa_display_panel_mem_area_t area = {
        .panel_w = panel->caps.width,
        .panel_h = panel->caps.height,
#ifdef CONFIG_LISA_DISPLAY_PANEL_GC9A01
        .x_offset = CONFIG_PANEL_GC9A01_X_OFFSET,
        .y_offset = CONFIG_PANEL_GC9A01_Y_OFFSET,
#elif CONFIG_LISA_DISPLAY_PANEL_GC9D01N
        .x_offset = CONFIG_PANEL_GC9D01N_X_OFFSET,
        .y_offset = CONFIG_PANEL_GC9D01N_Y_OFFSET,
#endif
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };
    panel_set_mem_area(panel, &area, x_coord, y_coord);

    panel_write_cmd_data(panel, LCD_CMD_CASET, 8, x_coord, sizeof(x_coord));
    panel_write_cmd_data(panel, LCD_CMD_RASET, 8, y_coord, sizeof(y_coord));
}

static int gc9a01_get_capabilities(lisa_display_panel_t *panel, lisa_display_capabilities_t *caps)
{
    if (!caps) {
        return LISA_DEVICE_ERR_INVALID;
    }

    memcpy(caps, &panel->caps, sizeof(*caps));

    return LISA_DEVICE_OK;
}

static int gc9a01_write(lisa_display_panel_t *panel, uint16_t x, uint16_t y, const lisa_display_buffer_desc_t *desc,
                        const void *buf)
{
    if (!desc || !buf) {
        return LISA_DEVICE_ERR_INVALID;
    }

    gc9a01_set_window(panel, x, y, desc->width, desc->height);

    return panel_draw_pixels(panel, LCD_CMD_RAMWR, 8, x, y, desc->width, desc->height, buf);
}

static int gc9a01_blanking_on(lisa_display_panel_t *panel)
{
    return panel_write_cmd_data(panel, LCD_CMD_DISPLAY_OFF, 8, NULL, 0);
}

static int gc9a01_blanking_off(lisa_display_panel_t *panel)
{
    return panel_write_cmd_data(panel, LCD_CMD_DISPLAY_ON, 8, NULL, 0);
}

static int gc9a01_set_brightness(lisa_display_panel_t *panel, const uint8_t brightness)
{
    return panel_set_backlight_brightness(&panel->backlight, brightness);
}

static int gc9a01_set_orientation(lisa_display_panel_t *panel, lisa_display_orientation_t orientation)
{
    struct panel_gc9a01_priv *priv = (struct panel_gc9a01_priv *)panel->priv_data;

    panel->caps.orientation = orientation;
    priv->orientation = orientation;

    return LISA_DEVICE_OK;
}

const lisa_display_panel_driver_t lisa_display_gc9a01_driver = {
    .init = gc9a01_init,
    .write = gc9a01_write,
    .blanking_on = gc9a01_blanking_on,
    .blanking_off = gc9a01_blanking_off,
    .set_brightness = gc9a01_set_brightness,
    .set_orientation = gc9a01_set_orientation,
    .get_capabilities = gc9a01_get_capabilities,
};

int panel_gc9a01_device_init(void)
{
    LISA_LOGD(LOG_TAG, "GC9A01/GC9D01N device registered");
    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(lcd_panel, &lisa_display_gc9a01_driver, NULL, NULL, &panel_gc9a01_device_init,
                     LISA_DEVICE_PRIORITY_HIGH);
