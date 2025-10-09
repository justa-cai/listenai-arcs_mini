#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cache.h"

#include "lisa_log.h"
#include "display_common.h"
#include "lisa_display.h"
#include "display_trans_ctx.h"
#include "lisa_log.h"

static struct display_obj g_display_obj;

static const struct disp_init_cmd disp_init_items[] = {
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
};

static void _gc9a01_init(void)
{
    disp_comm_rst_set();
    SysTick_Delay_Ms(10);
    disp_comm_rst_clr();
    SysTick_Delay_Ms(10);
    disp_comm_rst_set();

    SysTick_Delay_Ms(120);

    display_trans_cmd_data(DISPLAY_COMM_CMD_SLEEP_OUT, NULL, 0);

    SysTick_Delay_Ms(120);

    for (uint32_t i = 0; i < sizeof(disp_init_items) / sizeof(disp_init_items[0]); i++) {
        display_trans_cmd_data(disp_init_items[i].cmd, (uint8_t *)disp_init_items[i].data,
                               disp_init_items[i].data_bytes);
    }

    display_trans_cmd_data(DISPLAY_COMM_CMD_INV_ON, NULL, 0);
}

int gc9a01_display_init(display_hw_config_t *config)
{
    if (g_display_obj.initialized) {
        return 0;
    }

    g_display_obj.mutex = xSemaphoreCreateMutex();
    if (g_display_obj.mutex == NULL) {
        LOGE("[%s] Failed to create mutex", __FUNCTION__);
        return -1;
    }

    // init rst
    disp_comm_rst_init(&config->reset);

    // init pwm
    disp_comm_brightness_init(&config->blacklight);

#if CONFIG_LISA_DISPLAY_TE_SYNC
    g_display_obj.te_sem = xSemaphoreCreateBinary();
    if (g_display_obj.te_sem == NULL) {
        LOGE("[%s] Failed to create TE semaphore", __FUNCTION__);
        return -1;
    }

    disp_comm_te_init(g_display_obj.te_sem, &config->te);
#endif

    display_trans_ctx_init(16, 8, 0, config);

    _gc9a01_init();

    g_display_obj.orientation = DISPLAY_ORIENTATION_NORMAL;
    g_display_obj.initialized = true;

    return 0;
}

int gc9a01_display_blanking_on(void)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    display_trans_cmd_data(DISPLAY_COMM_CMD_DISP_OFF, NULL, 0);
    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

int gc9a01_display_blanking_off(void)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    display_trans_cmd_data(DISPLAY_COMM_CMD_DISP_ON, NULL, 0);
    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

int gc9a01_display_set_brightness(const uint8_t brightness)
{
    disp_comm_brightness_set(brightness);

    return 0;
}

void gc9a01_display_get_capabilities(struct display_capabilities *capabilities)
{
    memset(capabilities, 0, sizeof(*capabilities));

    capabilities->x_resolution = CONFIG_DISPLAY_GC9A01_WIDTH;
    capabilities->y_resolution = CONFIG_DISPLAY_GC9A01_HEIGHT;
    capabilities->current_pixel_format = PIXEL_FORMAT_RGB_565;
    capabilities->current_orientation = g_display_obj.orientation;
    capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
}

int gc9a01_display_write(const uint16_t x, const uint16_t y, const struct display_buffer_descriptor *desc,
                         const void *buf)
{
    int ret = 0;
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);

    LOGD("[%s] x: %d, y: %d, width: %d, height: %d", __FUNCTION__, x, y, desc->width, desc->height);
#if CONFIG_LISA_DISPLAY_TE_SYNC
    if (disp_comm_te_wait(g_display_obj.te_sem, 100) != 0) {
        CLOGE("[%s] Failed to take TE semaphore", __FUNCTION__);
        xSemaphoreGive(g_display_obj.mutex);
        return -1;
    }
#endif

#if CONFIG_DCACHE_ENABLE
    dcache_clean_range((uint32_t)buf, (uint32_t)buf + desc->height * desc->pitch);
#endif

    struct disp_mem_area_input area_input = {
        .panel_w = CONFIG_DISPLAY_GC9A01_WIDTH,
        .panel_h = CONFIG_DISPLAY_GC9A01_HEIGHT,
        .x_offset = CONFIG_DISPLAY_GC9A01_X_OFFSET,
        .y_offset = CONFIG_DISPLAY_GC9A01_Y_OFFSET,
        .x = x,
        .y = y,
        .w = desc->width,
        .h = desc->height,
        .orient = g_display_obj.orientation,
    };
    disp_mem_coord coord_x;
    disp_mem_coord coord_y;
    disp_comm_set_mem_area(&area_input, coord_x, coord_y);

    display_trans_cmd_data(DISPLAY_COMM_CMD_CASET, coord_x, sizeof(coord_x));
    display_trans_cmd_data(DISPLAY_COMM_CMD_RASET, coord_y, sizeof(coord_y));

    enum display_trans_orient rotated = DISPLAY_TRANS_ORIENT_NORMAL;
    if (g_display_obj.orientation == DISPLAY_ORIENTATION_ROTATED_90) {
        rotated = DISPLAY_TRANS_ORIENT_ROTATED_90;
    } else if (g_display_obj.orientation == DISPLAY_ORIENTATION_ROTATED_270) {
        rotated = DISPLAY_TRANS_ORIENT_ROTATED_270;
    }

    ret = display_trans_image(DISPLAY_COMM_CMD_RAMWR, (void *)buf, desc->width, desc->height, rotated);

    xSemaphoreGive(g_display_obj.mutex);

    return ret;
}

int gc9a01_display_set_orientation(const enum display_orientation orientation)
{
    if (orientation == DISPLAY_ORIENTATION_ROTATED_180) {
        LOGE("[%s] not supported orientation", __func__);
        return -1;
    }

    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    g_display_obj.orientation = orientation;
    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

int gc9a01_display_sleep(const uint8_t onoff)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);

    if (onoff) {
        display_trans_cmd_data(DISPLAY_COMM_CMD_SLEEP_IN, NULL, 0);
    } else {
        display_trans_cmd_data(DISPLAY_COMM_CMD_SLEEP_OUT, NULL, 0);
    }

    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

static const struct display_driver_api gc9a01_driver_api = {
    .display_blanking_on = gc9a01_display_blanking_on,
    .display_blanking_off = gc9a01_display_blanking_off,
    .display_get_capabilities = gc9a01_display_get_capabilities,
    .display_set_brightness = gc9a01_display_set_brightness,
    .display_write = gc9a01_display_write,
    .display_set_orientation = gc9a01_display_set_orientation,
    .display_sleep = gc9a01_display_sleep,
};

const struct display_device display_gc9a01 = {
    .name = "gc9a01",
    .device_init = gc9a01_display_init,
    .api = &gc9a01_driver_api,
};
