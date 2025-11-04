/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "display_ssd1683.h"
#include "FreeRTOS.h"
#include "display_uc8253c.h"
#include "semphr.h"
#include "systick.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "cache.h"
#include "display_common.h"
#include "display_trans_ctx.h"
#include "trans_ctx/display_trans_ctx.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "ssd1683"
#include "lisa_log.h"

static struct display_obj g_display_obj;

static int refresh_time = 0;    // 刷新计数
static __attribute__((
    section(".psram.data"))) uint8_t white_buffer[CONFIG_DISPLAY_SSD1683_WIDTH * CONFIG_DISPLAY_SSD1683_HEIGHT / 8];
static __attribute__((
    section(".psram.data"))) uint8_t lvgl_buffer[CONFIG_DISPLAY_SSD1683_WIDTH * CONFIG_DISPLAY_SSD1683_HEIGHT / 8];

static int display_ssd1683_refresh(epd_refresh_mode_e mode)
{
    uint8_t refresh_mode = 0xF7; // default to GC
    switch (mode) {
    case EPD_REFRESH_MODE_GC:
        refresh_mode = 0xF7;
        break;
    case EPD_REFRESH_MODE_DU:
        refresh_mode = 0xC7;
        break;
    case EPD_REFRESH_MODE_PART:
        refresh_mode = 0xFF;
        break;
    default:
        LOGE("[%s] Invalid mode:%d, use default GC", __FUNCTION__, mode);
        refresh_mode = 0xF7;
        break;
    }

    display_trans_cmd_data(0x22, (uint8_t[]){refresh_mode}, 1); // WRITE_RAM
    display_trans_cmd_data(0x20, NULL, 0);                      // DISPLAY_UPDATE_CONTROL

    return 0;
}

static int display_ssd1683_busy_wait(uint32_t timeout)
{
#if CONFIG_LISA_DISPLAY_BUSY_SYNC
    if (disp_comm_busy_wait(g_display_obj.busy_sem, timeout) != 0) {
        LOGE("[%s] Failed to take BUSY semaphore", __FUNCTION__);
        return -1;
    }
#endif
    return 0;
}

static int display_ssd1683_hw_reset(void)
{
    disp_comm_rst_set();
    vTaskDelay(pdMS_TO_TICKS(50));

    disp_comm_rst_clr();
    vTaskDelay(pdMS_TO_TICKS(50));

    disp_comm_rst_set();
    vTaskDelay(pdMS_TO_TICKS(20));

    return 0;
}

int display_ssd1683_blanking_on(void)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    // display_trans_cmd_data(0x07, (uint8_t[]){0xA5}, 1);// enter sleep mode
    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

int display_ssd1683_blanking_off(void)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

void display_ssd1683_get_capabilities(struct display_capabilities *capabilities)
{
    memset(capabilities, 0, sizeof(struct display_capabilities));
    capabilities->x_resolution = CONFIG_DISPLAY_SSD1683_WIDTH;
    capabilities->y_resolution = CONFIG_DISPLAY_SSD1683_HEIGHT;
    capabilities->current_pixel_format = PIXEL_FORMAT_MONO_1;
    capabilities->current_orientation = g_display_obj.orientation;
    capabilities->supported_pixel_formats = PIXEL_FORMAT_MONO_1;
}

int fb_convert_to_mono(const uint8_t *src, uint8_t *dst, const uint32_t width, const uint32_t height)
{
    if (src == NULL || dst == NULL || width == 0 || height == 0) {
        return -1;
    }
    const uint32_t src_pitch = width;              // 8bpp: 1个字节表示1个像素
    const uint32_t dst_pitch = (width + 7) / 8;    // 1bpp: 8个像素占1字节，向上取整
    const uint32_t dst_bytes = height * dst_pitch; // 目标缓冲区总字节数

    memset(dst, 0xFF, dst_bytes); // 初始化为全白
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            const uint8_t pixel = *(src + y * src_pitch + x);
            if (pixel) { // 黑色像素
                dst[y * dst_pitch + x / 8] &= ~(0x80 >> (x % 8));
            } else { // 白色像素
                dst[y * dst_pitch + x / 8] |= (0x80 >> (x % 8));
            }
        }
    }
    return 0;
}

int fb_invert(uint8_t *buf, const uint32_t width, const uint32_t height)
{
    if (buf == NULL || width == 0 || height == 0) {
        return -1;
    }
    const uint32_t pitch = (width + 7) / 8; // 1bpp: 8个像素占1字节，向上取整
    const uint32_t total_bytes = height * pitch;

    for (uint32_t i = 0; i < total_bytes; i++) {
        buf[i] = ~buf[i];
    }
    return 0;
}

int fb_rotate_90(const uint8_t *src, uint8_t *dst, const uint32_t width, const uint32_t height)
{
    if (src == NULL || dst == NULL || width == 0 || height == 0) {
        return -1;
    }
    const uint32_t src_pitch = (width + 7) / 8;  // 1bpp: 8个像素占1字节，向上取整
    const uint32_t dst_pitch = (height + 7) / 8; // 1bpp: 8个像素占1字节，向上取整
    const uint32_t dst_width = height;           // 旋转后宽度
    const uint32_t dst_height = width;           // 旋转后高度
    memset(dst, 0xFF, dst_height * dst_pitch);   // 初始化为全白

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            const uint8_t pixel = (src[y * src_pitch + x / 8] >> (7 - (x % 8))) & 0x01;
            if (pixel == 0) { // 黑色像素
                dst[x * dst_pitch + (dst_width - y - 1) / 8] &= ~(0x80 >> ((dst_width - y - 1) % 8));
            } else { // 白色像素
                dst[x * dst_pitch + (dst_width - y - 1) / 8] |= (0x80 >> ((dst_width - y - 1) % 8));
            }
        }
    }
    return 0;
}

static int display_ssd1683_set_refresh_windows(const uint16_t x, const uint16_t y, const uint16_t width,
                                               const uint16_t height)
{
    if ((x + width) > CONFIG_DISPLAY_SSD1683_WIDTH || (y + height) > CONFIG_DISPLAY_SSD1683_HEIGHT) {
        LOGE("[%s] Invalid parameters", __FUNCTION__);
        return -1;
    }

    const uint16_t x_start = x;
    const uint16_t x_end = x + width - 1;
    const uint16_t y_start = y;
    const uint16_t y_end = y + height - 1;
    uint8_t x_data[2] = {(x_start >> 3) & 0xFF,
                         (x_end >> 3) & 0xFF}; // Address in the X direction by 8 times address unit
    uint8_t y_data[4] = {y_start & 0xFF, (y_start >> 8) & 0xFF, y_end & 0xFF, (y_end >> 8) & 0xFF};

    display_trans_cmd_data(0x44, x_data, 2); // SET_RAM_X_ADDRESS_START_END_POSITION
    display_trans_cmd_data(0x45, y_data, 4); // SET_RAM_Y_ADDRESS_START_END_POSITION
    return 0;
}

static int display_ssd1683_set_cursor(const uint16_t x, const uint16_t y)
{
    if (x >= CONFIG_DISPLAY_SSD1683_WIDTH || y >= CONFIG_DISPLAY_SSD1683_HEIGHT) {
        LOGE("[%s] Invalid parameters", __FUNCTION__);
        return -1;
    }

    const uint8_t x_addr = x;
    const uint8_t y_addr = y;
    display_trans_cmd_data(0x4E, &x_addr,
                           1); // SET_X_ADDRESS_COUNTER, address in the X direction by 8 times address unit
    display_trans_cmd_data(0x4F, (uint8_t[]){(y_addr & 0xFF), ((y_addr >> 8) & 0xFF)}, 2); // SET_Y_ADDRESS_COUNTER
    return 0;
}

static int display_ssd1683_write(const uint16_t x, const uint16_t y, const struct display_buffer_descriptor *desc,
                                 const void *buf)
{
    int ret = 0;
    // LOGW("[%s] x: %d, y: %d, width: %d, height: %d", __FUNCTION__, x, y, desc->width, desc->height);
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);

    void *image_buf = (void *)buf;
    // fb_convert_to_mono(buf, lvgl_buffer, desc->width, desc->height);
    // void *image_buf = lvgl_buffer;

#if CONFIG_DCACHE_ENABLE
    dcache_clean_range((uint32_t)buf, (uint32_t)buf + desc->height * desc->pitch);
#endif

    const size_t image_size = (CONFIG_DISPLAY_SSD1683_WIDTH * CONFIG_DISPLAY_SSD1683_HEIGHT / 8);
    if (desc->width == CONFIG_DISPLAY_SSD1683_WIDTH && desc->height == CONFIG_DISPLAY_SSD1683_HEIGHT) {
        // display_ssd1683_set_refresh_windows(0, 0, CONFIG_DISPLAY_SSD1683_WIDTH, CONFIG_DISPLAY_SSD1683_HEIGHT);
        // display_ssd1683_set_cursor(0, 0);
        // fb_invert(image_buf, CONFIG_DISPLAY_SSD1683_WIDTH, CONFIG_DISPLAY_SSD1683_HEIGHT);
        display_trans_cmd_data(0x24, image_buf, image_size); // Transfer old data
        // display_ssd1683_refresh(EPD_REFRESH_MODE_GC);
        if (refresh_time < 1) {
            display_ssd1683_refresh(EPD_REFRESH_MODE_GC);
        } else {
            display_ssd1683_refresh(EPD_REFRESH_MODE_DU);
        }
        display_ssd1683_busy_wait(5000);
        if (refresh_time++ > 10) {
            refresh_time = 0;
        }
    }

    xSemaphoreGive(g_display_obj.mutex);
    return ret;
}

int display_ssd1683_set_orientation(const enum display_orientation orientation)
{
    LOGI("[%s] orientation=%d", __FUNCTION__, orientation);
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    g_display_obj.orientation = orientation;
    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

static int display_ssd1683_init(display_hw_config_t *config)
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

    g_display_obj.busy_sem = xSemaphoreCreateBinary();
    if (g_display_obj.busy_sem == NULL) {
        LOGE("[%s] Failed to create BUSY semaphore", __FUNCTION__);
        return -1;
    }

    display_trans_ctx_init(1, 8, 0, config); // 1bpp
    
    disp_comm_busy_init(g_display_obj.busy_sem, &config->busy, 0); // busy: high active
    display_ssd1683_hw_reset();
    // display_ssd1683_busy_wait(1000);

    display_trans_cmd_data(0x12, NULL, 0); // SWRESET
    display_ssd1683_busy_wait(1000);

    display_trans_cmd_data(0x21, (uint8_t[]){0x40, 0x00}, 2); // Set display update control
    display_trans_cmd_data(0x3C, (uint8_t[]){0x05}, 1);       // BorderWaveform
    display_trans_cmd_data(0x11, (uint8_t[]){0x03}, 1);       // data entry mode

    display_ssd1683_set_refresh_windows(0, 0, CONFIG_DISPLAY_SSD1683_WIDTH, CONFIG_DISPLAY_SSD1683_HEIGHT);
    display_ssd1683_set_cursor(0, 0);
    display_ssd1683_busy_wait(1000);

    // 第一次刷新 0xFF 填充全白
    memset(white_buffer, 0xFF, sizeof(white_buffer));
    display_trans_cmd_data(0x24, white_buffer, sizeof(white_buffer));
    display_ssd1683_refresh(EPD_REFRESH_MODE_GC);
    display_ssd1683_busy_wait(5000);

    refresh_time = 1;
    g_display_obj.orientation = DISPLAY_ORIENTATION_NORMAL;
    g_display_obj.initialized = true;

    return 0;
}

static const struct display_driver_api display_ssd1683_driver_api = {
    .display_blanking_on = display_ssd1683_blanking_on,
    .display_blanking_off = display_ssd1683_blanking_off,
    .display_get_capabilities = display_ssd1683_get_capabilities,
    // .display_set_brightness = display_ssd1683_set_brightness,
    .display_write = display_ssd1683_write,
    .display_set_orientation = display_ssd1683_set_orientation,
};

const struct display_device display_ssd1683 = {
    .name = "ssd1683",
    .device_init = display_ssd1683_init,
    .api = &display_ssd1683_driver_api,
};
