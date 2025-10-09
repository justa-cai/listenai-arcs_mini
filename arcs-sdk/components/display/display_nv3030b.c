/*
 * Copyright (c) 2024, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "display_nv3030b.h"
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

#define LOG_TAG "nv3030b"
#include "lisa_log.h"


struct init_cmd_item_t {
    uint8_t cmd;
    uint8_t data_len;
    uint8_t data[24];
};

static struct display_obj g_display_obj;

// Forward declarations for API functions
static int nv3030b_display_init(display_hw_config_t *config);
static int nv3030b_display_blanking_on(void);
static int nv3030b_display_blanking_off(void);
static int nv3030b_display_set_brightness(const uint8_t brightness);
static void nv3030b_display_get_capabilities(struct display_capabilities *capabilities);
static int nv3030b_display_write(const uint16_t x, const uint16_t y, const struct display_buffer_descriptor *desc, const void *buf);
static int nv3030b_display_set_orientation(const enum display_orientation orientation);
static int nv3030b_display_sleep(const uint8_t onoff);

// Initialization commands for NV3030B
static const struct init_cmd_item_t init_items[] = {
	{0xfd, 2, {0x06, 0x08}},
	{0x61, 2, {0x07, 0x07}},
	{0x73, 1, {0x70}},
	{0x73, 1, {0x00}},
	{0x62, 4, {0x00, 0x44, 0x40, 0x01}},
	{0x63, 4, {0x41, 0x07, 0x12, 0x12}},
	{0x65, 3, {0x09, 0x17, 0x21}},
	{0x66, 3, {0x09, 0x17, 0x21}},
	{0x67, 2, {0x20, 0x40}},
	{0x68, 4, {0x90, 0x30, 0x32, 0x26}},
	{0xb1, 3, {0x0f, 0x02, 0x01}},
	{0xb4, 1, {0x01}},
	{0xb5, 4, {0x02, 0x02, 0x0a, 0x14}},
	{0xb6, 5, {0x04, 0x01, 0x9f, 0x00, 0x02}},
	{0xdf, 1, {0x11}},
	{0xe2, 6, {0x00, 0x05, 0x07, 0x24, 0x33, 0x3f}},
	{0xe5, 6, {0x3f, 0x33, 0x26, 0x09, 0x07, 0x00}},
	{0xe1, 2, {0x16, 0x5b}},
	{0xe4, 2, {0x5b, 0x18}},
	{0xe0, 8, {0x06, 0x07, 0x0d, 0x0f, 0x0f, 0x10, 0x12, 0x17}},
	{0xe3, 8, {0x19, 0x14, 0x11, 0x0f, 0x12, 0x0f, 0x07, 0x06}},
	{0xe6, 2, {0x00, 0xff}},
	{0xe7, 6, {0x01, 0x04, 0x03, 0x03, 0x00, 0x12}},
	{0xe8, 3, {0x00, 0x70, 0x00}},
	{0xec, 1, {0x50}},
	{0xf1, 1, {0x00}},
	{0xfd, 2, {0xfa, 0xfc}},
	{0x3a, 1, {0x55}},
	{0x35, 1, {0x00}},
	{0x36, 1, {0xC0}},
	{0x21, 0, {0x00}},
	{0x11, 0, {0x00}},
};

static void _nv3030b_panel_init(void)
{
    // Hardware Reset
    disp_comm_rst_set();
    SysTick_Delay_Ms(10);
    disp_comm_rst_clr();
    SysTick_Delay_Ms(200);
    disp_comm_rst_set();
    SysTick_Delay_Ms(120);

    for (uint32_t i = 0; i < sizeof(init_items) / sizeof(init_items[0]); i++) {
        display_trans_cmd_data(init_items[i].cmd, (uint8_t *)init_items[i].data, init_items[i].data_len);
    }
}

static int nv3030b_display_init(display_hw_config_t *config)
{
    if (g_display_obj.initialized) {
        return 0;
    }

    g_display_obj.mutex = xSemaphoreCreateMutex();
    if (g_display_obj.mutex == NULL) {
        LOGE("Failed to create mutex");
        return -1;
    }

    disp_comm_rst_init(&config->reset); // Initializes reset pin from display_common
    disp_comm_brightness_init(&config->blacklight); // Initializes PWM for backlight from display_common

#if CONFIG_LISA_DISPLAY_TE_SYNC
    g_display_obj.te_sem = xSemaphoreCreateBinary();
    if (g_display_obj.te_sem == NULL) {
        LOGE("Failed to create TE semaphore");
        // Consider cleanup for mutex if created
        if (g_display_obj.mutex != NULL) {
            vSemaphoreDelete(g_display_obj.mutex);
            g_display_obj.mutex = NULL;
        }
        return -1;
    }
    disp_comm_te_init(g_display_obj.te_sem); // Initializes TE pin and ISR from display_common
#endif

    // Initialize QSPI/SPI 4-line from display_trans_ctx. Bits per pixel, command bits, D/C command level.
    // For SPI 4-line, D/C level for command is typically 0.
    display_trans_ctx_init(16 /*bpp*/, 8 /*cmd_bits*/, 0 /*dc_cmd_level*/, &config->trans_config);

    _nv3030b_panel_init();

    g_display_obj.orientation = DISPLAY_ORIENTATION_NORMAL;
    g_display_obj.initialized = true;

    LOGI("NV3030B display initialized");
    return 0;
}

static int nv3030b_display_blanking_on(void)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    display_trans_cmd_data(DISPLAY_COMM_CMD_DISP_OFF, NULL, 0);
    xSemaphoreGive(g_display_obj.mutex);
    LOGD("Display blanking ON");
    return 0;
}

static int nv3030b_display_blanking_off(void)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    display_trans_cmd_data(DISPLAY_COMM_CMD_DISP_ON, NULL, 0);
    xSemaphoreGive(g_display_obj.mutex);
    LOGD("Display blanking OFF");
    return 0;
}

static int nv3030b_display_set_brightness(const uint8_t brightness)
{
    // Uses common PWM function to set backlight brightness
    disp_comm_brightness_set(brightness);
    LOGD("Set brightness to %u", brightness);
    return 0;
}

static void nv3030b_display_get_capabilities(struct display_capabilities *capabilities)
{
    memset(capabilities, 0, sizeof(*capabilities));
    capabilities->x_resolution = CONFIG_DISPLAY_NV3030B_WIDTH;  // From Kconfig
    capabilities->y_resolution = CONFIG_DISPLAY_NV3030B_HEIGHT; // From Kconfig
    capabilities->current_pixel_format = PIXEL_FORMAT_RGB_565;
    capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
    capabilities->current_orientation = g_display_obj.orientation;
    // Add other capabilities if any (e.g., other supported formats or orientations)
}

static int nv3030b_display_write(const uint16_t x, const uint16_t y, const struct display_buffer_descriptor *desc, const void *buf)
{
    int ret = 0;
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);

    LOGD("Write x:%u, y:%u, w:%u, h:%u", x, y, desc->width, desc->height);

#if CONFIG_LISA_DISPLAY_TE_SYNC
    if (disp_comm_te_wait(g_display_obj.te_sem, 100) != 0) {
        LOGE("Failed to take TE semaphore");
        xSemaphoreGive(g_display_obj.mutex);
        return -1;
    }
#endif

#if CONFIG_DCACHE_ENABLE
    // Ensure data is flushed from D-Cache to memory if buffer is cacheable
    dcache_clean_range((uint32_t)buf, (uint32_t)buf + desc->height * desc->pitch);
#endif

    struct disp_mem_area_input area_input = {
        .panel_w = CONFIG_DISPLAY_NV3030B_WIDTH,
        .panel_h = CONFIG_DISPLAY_NV3030B_HEIGHT,
        .x_offset = CONFIG_DISPLAY_NV3030B_X_OFFSET, // From Kconfig
        .y_offset = CONFIG_DISPLAY_NV3030B_Y_OFFSET, // From Kconfig
        .x = x,
        .y = y,
        .w = desc->width,
        .h = desc->height,
        .orient = g_display_obj.orientation,
    };
    disp_mem_coord coord_x; // uint8_t[4]
    disp_mem_coord coord_y; // uint8_t[4]

    disp_comm_set_mem_area(&area_input, coord_x, coord_y);

    display_trans_cmd_data(DISPLAY_COMM_CMD_CASET, coord_x, sizeof(coord_x));
    display_trans_cmd_data(DISPLAY_COMM_CMD_RASET, coord_y, sizeof(coord_y));

    enum display_trans_orient rotated = DISPLAY_TRANS_ORIENT_NORMAL;
    if (g_display_obj.orientation == DISPLAY_ORIENTATION_ROTATED_90) {
        rotated = DISPLAY_TRANS_ORIENT_ROTATED_90;
    }else if (g_display_obj.orientation == DISPLAY_ORIENTATION_ROTATED_270) {
        rotated = DISPLAY_TRANS_ORIENT_ROTATED_270;
    }

    ret = display_trans_image(DISPLAY_COMM_CMD_RAMWR, (void *)buf, desc->width, desc->height, rotated);

    xSemaphoreGive(g_display_obj.mutex);
    return ret;
}

static int nv3030b_display_set_orientation(const enum display_orientation orientation)
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

static int nv3030b_display_sleep(const uint8_t onoff)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    if (onoff) { // Enter sleep
        display_trans_cmd_data(DISPLAY_COMM_CMD_SLEEP_IN, NULL, 0);
        LOGD("Entering sleep mode");
    } else { // Exit sleep
        display_trans_cmd_data(DISPLAY_COMM_CMD_SLEEP_OUT, NULL, 0);
        LOGD("Exiting sleep mode");
    }
    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

static const struct display_driver_api nv3030b_driver_api = {
    .display_blanking_on = nv3030b_display_blanking_on,
    .display_blanking_off = nv3030b_display_blanking_off,
    .display_get_capabilities = nv3030b_display_get_capabilities,
    .display_set_brightness = nv3030b_display_set_brightness,
    .display_write = nv3030b_display_write,
    .display_set_orientation = nv3030b_display_set_orientation,
    .display_sleep = nv3030b_display_sleep,
};

const struct display_device display_nv3030b = {
    .name = "nv3030b",
    .device_init = nv3030b_display_init,
    .api = &nv3030b_driver_api,
};