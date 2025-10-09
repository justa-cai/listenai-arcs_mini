#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cache.h"

#include "lisa_log.h"
#include "display_common.h"
#include "lisa_display.h"
#include "display_trans_ctx.h"

static struct display_obj g_display_obj;

static const struct disp_init_cmd vendor_specific_init_default[]  = {
    {0xFE, (uint8_t[]){0x0}, 0x0, 0x0},
    {0xEF, (uint8_t[]){0x0}, 0x0, 0x0},

    {0x80, (uint8_t[]){0xC0}, 0x01, 0x0},
    {0x81, (uint8_t[]){0x01}, 0x01, 0x0},
    {0x82, (uint8_t[]){0x07}, 0x01, 0x0},
    {0x83, (uint8_t[]){0x38}, 0x01, 0x0},
    {0x88, (uint8_t[]){0x64}, 0x01, 0x0},
    {0x89, (uint8_t[]){0x86}, 0x01, 0x0},
    {0x8B, (uint8_t[]){0x3C}, 0x01, 0x0},
    {0x8D, (uint8_t[]){0x51}, 0x01, 0x0},
    {0x8E, (uint8_t[]){0x70}, 0x01, 0x0},

    {0x36, (uint8_t[]){0x48}, 0x01, 0x0},
    {0x3A, (uint8_t[]){0x05}, 0x01, 0x0},

    {0xBF, (uint8_t[]){0x1F}, 0x01, 0x0},

    {0x7D, (uint8_t[]){0x45, 06}, 0x02, 0x0},
    {0xEE, (uint8_t[]){0x00, 0x06}, 0x02, 0x0},

    {0xF4, (uint8_t[]){0x53}, 0x01, 0x0},

    {0xF6, (uint8_t[]){0x17, 0x08}, 0x02, 0x0},

    {0x70, (uint8_t[]){0x4F, 0x4F}, 0x02, 0x0},

    {0x71, (uint8_t[]){0x12, 0x20}, 0x02, 0x0},

    {0x72, (uint8_t[]){0x12, 0x20}, 0x02, 0x0},

    {0xB5, (uint8_t[]){0x50}, 0x01, 0x0},
    
    {0xBA, (uint8_t[]){0x00}, 0x01, 0x0},

    {0xEC, (uint8_t[]){0x71}, 0x01, 0x0},

    {0x7B, (uint8_t[]){0x00, 0x0D}, 0x02, 0x0},
    {0x7C, (uint8_t[]){0x0D, 0x03}, 0x02, 0x0},

    {0xF5, (uint8_t[]){0x02, 0x10, 0x12}, 0x03, 0x0},
    
    {0xF0, (uint8_t[]){0x11, 0x16, 0x0B, 0x0C, 0x05, 0x39, 0x4C, 0xAE, 0x94, 0x2E, 0x30, 0x7F}, 0xC, 0x0},
    {0xF1, (uint8_t[]){0x0F, 0x16, 0x0B, 0x09, 0x07, 0x39, 0x4D, 0x5D, 0x97, 0x26, 0x2C, 0xCF}, 0xC, 0x0},

    {0x66, (uint8_t[]){0x1A}, 0x01, 0x0},

    {0x67, (uint8_t[]){0x27}, 0x01, 0x0},

    {0x68, (uint8_t[]){0x27}, 0x01, 0x0},

    {0xCA, (uint8_t[]){0x0B}, 0x01, 0x0},

    {0xE8, (uint8_t[]){0xF0}, 0x01, 0x0},

    {0xCB, (uint8_t[]){0x06}, 0x01, 0x0},

    {0xB6, (uint8_t[]){0x5C, 0x40, 0x40}, 0x03, 0x0},

    {0xCC, (uint8_t[]){0x33}, 0x01, 0x0},
    {0xCD, (uint8_t[]){0x33}, 0x01, 0x0},

    // {0x2A, (uint8_t[]){0x00, 0x23, 0x00, 0xCC}, 0x04, 0x0},
    
    // {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0x3F}, 0x04, 0x0},

    {0x35, (uint8_t[]){0x00}, 0x01, 0x0},

    {0x11, (uint8_t[]){0x00}, 0x00, 80},

    {0xE8, (uint8_t[]){0xA0}, 0x01, 0x0},
    {0xE8, (uint8_t[]){0xF0}, 0x01, 0x0},
    {0xFE, (uint8_t[]){0x00}, 0x00, 0x0},
    {0xEE, (uint8_t[]){0x00}, 0x00, 0x0},
    {0x29, (uint8_t[]){0x00}, 0x00, 0x0},
    {0x2C, (uint8_t[]){0x00}, 0x00, 10},
};

static int _gc9309na_trans_cmd_prepare(int lcd_cmd)
{
    lcd_cmd &= 0xFF;
    lcd_cmd <<= 8;
    lcd_cmd |= (DISPLAY_COMN_OPCODE_WRITE_CMD << 24); 

    return lcd_cmd;
}

static int _gc9309na_trans_img_prepare(int lcd_cmd)
{
    lcd_cmd &= 0xFF;
    lcd_cmd <<= 8;
    lcd_cmd |= (DISPLAY_COMM_OPCODE_WRITE_IMG << 24); 

    return lcd_cmd;
}

static void _lcd_gc9309na_init(void)
{
    // reset
    disp_comm_rst_set();
    delay_ms(10);
    disp_comm_rst_clr();
    delay_ms(200);
    disp_comm_rst_set();
    delay_ms(100);

    for (int i = 0; i < sizeof(vendor_specific_init_default) / sizeof(struct disp_init_cmd); i++) {
        display_trans_cmd_data(_gc9309na_trans_cmd_prepare(vendor_specific_init_default[i].cmd), vendor_specific_init_default[i].data, vendor_specific_init_default[i].data_bytes);
        delay_ms(vendor_specific_init_default[i].delay);
    }
}

int gc9309na_display_init(display_hw_config_t *config)
{
    if (g_display_obj.initialized) {
        return 0;
    }

    g_display_obj.orientation = DISPLAY_ORIENTATION_NORMAL;

    disp_comm_rst_init(&config->reset);

    // PWM
    disp_comm_brightness_init(&config->blacklight);

#if CONFIG_LISA_DISPLAY_TE_SYNC

    g_display_obj.te_sem = xSemaphoreCreateBinary();
    if (g_display_obj.te_sem == NULL) {
        LOGE("[%s] Failed to create TE semaphore", __FUNCTION__);
        return -1;
    }
    
    disp_comm_te_init(g_display_obj.te_sem, &config->te);
#endif

    g_display_obj.mutex = xSemaphoreCreateMutex();
    if( g_display_obj.mutex == NULL )
    {
        CLOGE("[%s] Failed to create g_mutex", __FUNCTION__);
        return -1;
    }

    display_trans_ctx_init(16, 32, 0, &config->trans_config);

    _lcd_gc9309na_init();
    g_display_obj.initialized = true;
    return 0;
}

int gc9309na_display_blanking_off(void)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    display_trans_cmd_data(_gc9309na_trans_cmd_prepare(DISPLAY_COMM_CMD_DISP_ON), NULL, 0);
    xSemaphoreGive(g_display_obj.mutex);

    return 0;
}

int gc9309na_display_blanking_on(void)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    display_trans_cmd_data(_gc9309na_trans_cmd_prepare(DISPLAY_COMM_CMD_DISP_OFF), NULL, 0);
    xSemaphoreGive(g_display_obj.mutex);

    return 0;
}

void gc9309na_display_get_capabilities(struct display_capabilities *capabilities)
{
    memset(capabilities, 0, sizeof(*capabilities));

    capabilities->x_resolution = CONFIG_DISPLAY_GC9309NA_WIDTH;
    capabilities->y_resolution = CONFIG_DISPLAY_GC9309NA_HEIGHT;
    capabilities->current_pixel_format = PIXEL_FORMAT_RGB_565;
    capabilities->current_orientation = g_display_obj.orientation;
    capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
}

int gc9309na_display_set_brightness(const uint8_t brightness)
{
    uint8_t value = brightness;

    disp_comm_brightness_set(value);

    return 0;
}

int gc9309na_display_write(const uint16_t x, const uint16_t y, const struct display_buffer_descriptor *desc,
    const void *buf)
{
    int ret = 0;
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);

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
        .panel_w = CONFIG_DISPLAY_GC9309NA_WIDTH,
        .panel_h = CONFIG_DISPLAY_GC9309NA_HEIGHT,
        .x_offset = CONFIG_DISPLAY_GC9309NA_X_OFFSET,
        .y_offset = CONFIG_DISPLAY_GC9309NA_Y_OFFSET,
        .x = x,
        .y = y,
        .w = desc->width,
        .h = desc->height,
        .orient = g_display_obj.orientation,
    };
    disp_mem_coord coord_x;
    disp_mem_coord coord_y;
    disp_comm_set_mem_area(&area_input, coord_x, coord_y);

    display_trans_cmd_data(_gc9309na_trans_cmd_prepare(DISPLAY_COMM_CMD_CASET), coord_x, sizeof(coord_x));
    display_trans_cmd_data(_gc9309na_trans_cmd_prepare(DISPLAY_COMM_CMD_RASET), coord_y,sizeof(coord_y));

    enum display_trans_orient rotated = DISPLAY_TRANS_ORIENT_NORMAL;
    if (g_display_obj.orientation == DISPLAY_ORIENTATION_ROTATED_90) {
        rotated = DISPLAY_TRANS_ORIENT_ROTATED_90;
    } else if (g_display_obj.orientation == DISPLAY_ORIENTATION_ROTATED_270) {
        rotated = DISPLAY_TRANS_ORIENT_ROTATED_270;
    }

    ret = display_trans_image(_gc9309na_trans_img_prepare(DISPLAY_COMM_CMD_RAMWR), (void *)buf, desc->width, desc->height, rotated);
    if (ret) {
        LOGE("[%s] Failed to trans image.");
        xSemaphoreGive(g_display_obj.mutex);
        return -1;
    }

    xSemaphoreGive(g_display_obj.mutex);

    return 0;
}

int gc9309na_display_set_orientation(const enum display_orientation orientation)
{
    if(orientation == DISPLAY_ORIENTATION_ROTATED_180){
        LOGE("[%s] not supported orientation", __func__);
        return -1;
    }

    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    g_display_obj.orientation = orientation;
    xSemaphoreGive(g_display_obj.mutex);

    return 0;
}

int gc9309na_display_sleep(const uint8_t onoff)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);

    if (onoff) {
        display_trans_cmd_data(_gc9309na_trans_cmd_prepare(DISPLAY_COMM_CMD_SLEEP_IN), NULL, 0);
    } else {
        display_trans_cmd_data(_gc9309na_trans_cmd_prepare(DISPLAY_COMM_CMD_SLEEP_OUT), NULL, 0);
    }

    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

static const struct display_driver_api gc9309na_driver_api = {
    .display_blanking_on      = gc9309na_display_blanking_on,
    .display_blanking_off     = gc9309na_display_blanking_off,
    .display_get_capabilities = gc9309na_display_get_capabilities,
    .display_set_brightness   = gc9309na_display_set_brightness,
    .display_write            = gc9309na_display_write,
    .display_set_orientation  = gc9309na_display_set_orientation,
    .display_sleep            = gc9309na_display_sleep,
};

const struct display_device display_gc9309na = {
    .name        = "gc9309na",
    .device_init = gc9309na_display_init,
    .api         = &gc9309na_driver_api,
};