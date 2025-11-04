/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "display_st7789p3.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "systick.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "cache.h"
#include <stdint.h>
#include <string.h>
#include "display_common.h"
#include "display_trans_ctx.h"
#if 0
#include "dma_irq_proxy.h"
#endif

#define LOG_TAG "st7789p3"
#include "lisa_log.h"


struct init_cmd_item_t {
    uint8_t cmd;
    uint8_t data_len;
    uint8_t data[24];
};
static struct display_obj g_display_obj;

static const struct init_cmd_item_t init_items[] = {
    {.cmd = 0xB2, .data_len = 5, .data = {0x0C, 0x0C, 0x00, 0x33, 0x33}},
    {.cmd = 0x35, .data_len = 1, .data = {0x00}},
    {.cmd = 0x36, .data_len = 1, .data = {0x00}},
    {.cmd = 0x3A, .data_len = 1, .data = {0x05}},
    {.cmd = 0xB7, .data_len = 1, .data = {0x55}},
    {.cmd = 0xBB, .data_len = 1, .data = {0x16}},
    {.cmd = 0xC0, .data_len = 1, .data = {0x2C}},
    {.cmd = 0xC2, .data_len = 1, .data = {0x01}},
    {.cmd = 0xC3, .data_len = 1, .data = {0x13}},
    // {.cmd = 0xC6, .data_len = 1, .data = {0x0F}},//60hz
    {.cmd = 0xC6, .data_len = 1, .data = {0x05}},//90hz
    {.cmd = 0xD0, .data_len = 3, .data = {0xA7, 0xA4, 0xA1}},
    {.cmd = 0xD6, .data_len = 1, .data = {0xA1}},
    {.cmd = 0xE0,
     .data_len = 14,
     .data = {0xF0, 0x06, 0x0E, 0x08, 0x08, 0x04, 0x37, 0x43, 0x4C, 0x36, 0x12, 0x12, 0x2C, 0x34}},
    {.cmd = 0xE1,
     .data_len = 14,
     .data = {0xF0, 0x0D, 0x12, 0x0C, 0x0A, 0x16, 0x37, 0x43, 0x4C, 0x39, 0x14, 0x15, 0x2E, 0x36}},
};


static void _st7789p3_init(void)
{
    disp_comm_rst_set();
    SysTick_Delay_Ms(10);
    disp_comm_rst_clr();
    SysTick_Delay_Ms(10);
    disp_comm_rst_set();

    SysTick_Delay_Ms(120);

    display_trans_cmd_data(DISPLAY_COMM_CMD_SLEEP_OUT, NULL, 0);

    SysTick_Delay_Ms(120);

    for (uint32_t i = 0; i < sizeof(init_items) / sizeof(init_items[0]); i++) {
        display_trans_cmd_data(init_items[i].cmd, (uint8_t *)init_items[i].data, init_items[i].data_len);
    }

#if CONFIG_LISA_DISPLAY_COLOR_INVERT
    display_trans_cmd_data(DISPLAY_COMM_CMD_INV_ON, NULL, 0);
#else
    display_trans_cmd_data(DISPLAY_COMM_CMD_INV_OFF, NULL, 0);
#endif

}

int st7789p3_display_init(display_hw_config_t *config)
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

    _st7789p3_init();

    g_display_obj.orientation = DISPLAY_ORIENTATION_NORMAL;
    g_display_obj.initialized = true;

    return 0;
}

int st7789p3_display_blanking_on(void)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    display_trans_cmd_data(DISPLAY_COMM_CMD_DISP_OFF, NULL, 0);
    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

int st7789p3_display_blanking_off(void)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    display_trans_cmd_data(DISPLAY_COMM_CMD_DISP_ON, NULL, 0);
    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

int st7789p3_display_set_brightness(const uint8_t brightness)
{
    disp_comm_brightness_set(brightness);

    return 0;
}

void st7789p3_display_get_capabilities(struct display_capabilities *capabilities)
{
    memset(capabilities, 0, sizeof(*capabilities));

    capabilities->x_resolution = CONFIG_DISPLAY_ST7789P3_WIDTH;
    capabilities->y_resolution = CONFIG_DISPLAY_ST7789P3_HEIGHT;
    capabilities->current_pixel_format = PIXEL_FORMAT_RGB_565;
    capabilities->current_orientation = g_display_obj.orientation;
    capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
}

int st7789p3_display_write(const uint16_t x, const uint16_t y, const struct display_buffer_descriptor *desc,
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
        .panel_w = CONFIG_DISPLAY_ST7789P3_WIDTH,
        .panel_h = CONFIG_DISPLAY_ST7789P3_HEIGHT,
        .x_offset = CONFIG_DISPLAY_ST7789P3_X_OFFSET,
        .y_offset = CONFIG_DISPLAY_ST7789P3_Y_OFFSET,
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
	display_trans_cmd_data(DISPLAY_COMM_CMD_RASET, coord_y,sizeof(coord_y));

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

int st7789p3_display_set_orientation(const enum display_orientation orientation)
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

int st7789p3_display_sleep(const uint8_t onoff)
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

int st7789p3_display_color_invert(const uint8_t onoff)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);

    if (onoff) {
        display_trans_cmd_data(DISPLAY_COMM_CMD_INV_ON, NULL, 0);
    } else {
        display_trans_cmd_data(DISPLAY_COMM_CMD_INV_OFF, NULL, 0);
    }

    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

static const struct display_driver_api st7789p3_driver_api = {
    .display_blanking_on = st7789p3_display_blanking_on,
    .display_blanking_off = st7789p3_display_blanking_off,
    .display_get_capabilities = st7789p3_display_get_capabilities,
    .display_set_brightness = st7789p3_display_set_brightness,
    .display_write = st7789p3_display_write,
    .display_set_orientation = st7789p3_display_set_orientation,
    .display_sleep = st7789p3_display_sleep,
    .display_color_invert = st7789p3_display_color_invert,
};

const struct display_device display_st7789p3 = {
    .name = "st7789p3",
    .device_init = st7789p3_display_init,
    .api = &st7789p3_driver_api,
};