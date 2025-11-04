/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * @note: uc8253c, 芯片支持黑白/红三色，Up to 240 source x 480 gate resolution
 *        + 1 border + 1 VCOM
 *
 *       模组规格: 3.7寸 240x416, 黑白
 *
 */
#include "display_uc8253c.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "systick.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "cache.h"
#include "display_common.h"
#include "display_trans_ctx.h"
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "uc8253c"
#include "lisa_log.h"

static struct display_obj g_display_obj;

static uint8_t lut_flag = 0;    // 调用波形标志位
static uint8_t gc_lut_flag = 0; // 调用波形标志位
static int refresh_time = 0;    // 刷新计数
static __attribute__((
    section(".psram.data"))) uint8_t white_buffer[CONFIG_DISPLAY_UC8253C_WIDTH * CONFIG_DISPLAY_UC8253C_HEIGHT / 8];
static __attribute__((
    section(".psram.data"))) uint8_t rotate_buffer[CONFIG_DISPLAY_UC8253C_WIDTH * CONFIG_DISPLAY_UC8253C_HEIGHT / 8];

// LUT 波形数据 - GC全刷
static const unsigned char lut_R20_GC[] = {
    0x01, 0x0f, 0x0f, 0x0f, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char lut_R21_GC[] = {
    0x01, 0x4f, 0x8f, 0x0f, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char lut_R22_GC[] = {
    0x01, 0x0f, 0x8f, 0x0f, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char lut_R23_GC[] = {
    0x01, 0x4f, 0x8f, 0x4f, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char lut_R24_GC[] = {
    0x01, 0x0f, 0x8f, 0x4f, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// LUT 波形数据 - DU局刷
static const unsigned char lut_R20_DU[] = {
    0x01, 0x0f, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char lut_R21_DU[] = {
    0x01, 0x0f, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char lut_R22_DU[] = {
    0x01, 0x8f, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char lut_R23_DU[] = {
    0x01, 0x4f, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char lut_R24_DU[] = {
    0x01, 0x0f, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static int display_uc8253c_refresh(void)
{
    display_trans_cmd_data(0x17, (uint8_t[]){0xA5}, 1);
#if CONFIG_LISA_DISPLAY_BUSY_SYNC
    if (disp_comm_busy_wait(g_display_obj.busy_sem, 1000) != 0) {
        LOGE("[%s] Failed to take BUSY semaphore", __FUNCTION__);
        return -1;
    }
#endif
}

static int display_uc8253c_hw_reset(void)
{
    disp_comm_rst_clr();
    delay_ms(20);
    disp_comm_rst_set();
    delay_ms(20);
    return 0;
}

static void display_uc8253c_lut_refresh(epd_refresh_mode_e mode)
{
    if (mode == EPD_REFRESH_MODE_GC) {
        if (gc_lut_flag == 0) {
            display_trans_cmd_data(0x50, (uint8_t[]){0xD7}, 1);
        } else {
            display_trans_cmd_data(0x50, (uint8_t[]){0xC7}, 1);
        }
        gc_lut_flag = !gc_lut_flag;

        display_trans_cmd_data(0x20, (uint8_t *)lut_R20_GC, sizeof(lut_R20_GC)); // VCOM
        display_trans_cmd_data(0x21, (uint8_t *)lut_R21_GC, sizeof(lut_R21_GC)); // WW red not use
        display_trans_cmd_data(0x22, (uint8_t *)lut_R22_GC, sizeof(lut_R22_GC)); // BW r
        display_trans_cmd_data(0x23, (uint8_t *)lut_R23_GC, sizeof(lut_R23_GC)); // WB w
        display_trans_cmd_data(0x24, (uint8_t *)lut_R24_GC, sizeof(lut_R24_GC)); // BB b
    } else if (mode == EPD_REFRESH_MODE_DU) {
        if (lut_flag == 0) {
            display_trans_cmd_data(0x50, (uint8_t[]){0xD7}, 1);
        } else {
            display_trans_cmd_data(0x50, (uint8_t[]){0xC7}, 1);
        }
        lut_flag = !lut_flag;

        display_trans_cmd_data(0x20, (uint8_t *)lut_R20_DU, sizeof(lut_R20_DU)); // VCOM
        display_trans_cmd_data(0x21, (uint8_t *)lut_R21_DU, sizeof(lut_R21_DU)); // WW red not use
        display_trans_cmd_data(0x22, (uint8_t *)lut_R22_DU, sizeof(lut_R22_DU)); // BW r
        display_trans_cmd_data(0x23, (uint8_t *)lut_R23_DU, sizeof(lut_R23_DU)); // WB w
        display_trans_cmd_data(0x24, (uint8_t *)lut_R24_DU, sizeof(lut_R24_DU)); // BB b
    }
}

int display_uc8253c_blanking_on(void)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    // display_trans_cmd_data(0x07, (uint8_t[]){0xA5}, 1);// enter sleep mode
    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

int display_uc8253c_blanking_off(void)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

void display_uc8253c_get_capabilities(struct display_capabilities *capabilities)
{
    memset(capabilities, 0, sizeof(struct display_capabilities));
    capabilities->x_resolution = CONFIG_DISPLAY_UC8253C_WIDTH;
    capabilities->y_resolution = CONFIG_DISPLAY_UC8253C_HEIGHT;
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

int display_uc8253c_write(const uint16_t x, const uint16_t y, const struct display_buffer_descriptor *desc,
                          const void *buf)
{
    int ret = 0;
    // LOGW("[%s] x: %d, y: %d, width: %d, height: %d", __FUNCTION__, x, y, desc->width, desc->height);
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);

    void *image_buf = (void *)buf;
    if (g_display_obj.orientation == DISPLAY_ORIENTATION_ROTATED_90) {
        fb_rotate_90(image_buf, rotate_buffer, desc->width, desc->height);
        image_buf = rotate_buffer;
    }

#if CONFIG_DCACHE_ENABLE
    dcache_clean_range((uint32_t)buf, (uint32_t)buf + desc->height * desc->pitch);
#endif

    const size_t image_size = (CONFIG_DISPLAY_UC8253C_WIDTH * CONFIG_DISPLAY_UC8253C_HEIGHT / 8);

    refresh_time++;
    if (refresh_time > 10) {
        refresh_time = 0; // 设置 DU mode 快刷 10 次后改用 GC mode 刷屏一次
        memset(white_buffer, 0xFF, sizeof(white_buffer));
        display_trans_cmd_data(0x10, white_buffer, sizeof(white_buffer));
        display_trans_cmd_data(0x13, white_buffer, sizeof(white_buffer));
        display_uc8253c_lut_refresh(EPD_REFRESH_MODE_GC);
        ret = display_uc8253c_refresh();

        memset(white_buffer, 0xFF, sizeof(white_buffer));
        display_trans_cmd_data(0x10, white_buffer, sizeof(white_buffer));
        display_trans_cmd_data(0x13, white_buffer, sizeof(white_buffer));
        display_uc8253c_lut_refresh(EPD_REFRESH_MODE_GC);
        ret = display_uc8253c_refresh();
    }

    // 使用 DU mode 进行刷新
    display_trans_cmd_data(0x50, (uint8_t[]){0xD7}, 1); // Border
    display_trans_cmd_data(0x13, image_buf, image_size);
    memcpy(white_buffer, image_buf, sizeof(white_buffer)); // 拷贝当前图像数据作为下一次刷新的旧数据

    display_uc8253c_lut_refresh(EPD_REFRESH_MODE_DU);
    ret = display_uc8253c_refresh();

    xSemaphoreGive(g_display_obj.mutex);
    return ret;
}

int display_uc8253c_set_orientation(const enum display_orientation orientation)
{
    LOGI("[%s] orientation=%d", __FUNCTION__, orientation);
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    g_display_obj.orientation = orientation;
    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

static const struct disp_init_cmd disp_init_items[] = {
    DISP_INIT_ITEM(0x00, 0, 0xF3, 0x08), // panel setting, RES1 RES0 REG KW/R UD SHL SHD_N  RST_N
    DISP_INIT_ITEM(0x01, 0, 0x03, 0x10, 0x3F, 0x3F, 0x03),
    DISP_INIT_ITEM(0x06, 0, 0x17, 0x37, 0x3D),
    DISP_INIT_ITEM(0x60, 0, 0x22),
    DISP_INIT_ITEM(0x82, 0, 0x06), // VCOM_DC setting
    DISP_INIT_ITEM(0x30, 0, 0x10), // 10=85Hz 0D=70Hz 0B=60Hz 0A=55Hz 09=50Hz  05=30Hz
    DISP_INIT_ITEM(0xe3, 0, 0x88),
    DISP_INIT_ITEM(0x61, 0, 0xf0, 0x01, 0xA0),
    DISP_INIT_ITEM(0x41, 0, 0x00),
    DISP_INIT_ITEM(0xE0, 0, 0x00),
    DISP_INIT_ITEM(0X50, 0, 0xD7),
};

int display_uc8253c_init(display_hw_config_t *config)
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

#if CONFIG_LISA_DISPLAY_BUSY_SYNC
    g_display_obj.busy_sem = xSemaphoreCreateBinary();
    if (g_display_obj.busy_sem == NULL) {
        CLOGE("[%s] Failed to create BUSY semaphore", __FUNCTION__);
        return -1;
    }

    disp_comm_busy_init(g_display_obj.busy_sem, &config->busy, 1); // busy: low active
#endif

    display_trans_ctx_init(1, 8, 0, config); // 1bpp
    display_uc8253c_hw_reset();

    for (uint32_t i = 0; i < sizeof(disp_init_items) / sizeof(disp_init_items[0]); i++) {
        display_trans_cmd_data(disp_init_items[i].cmd, (uint8_t *)disp_init_items[i].data,
                               disp_init_items[i].data_bytes);
    }
    delay_ms(20);

    lut_flag = 1;
    gc_lut_flag = 0;
    // 第一次刷新 0xFF 填充全白
    memset(white_buffer, 0xFF, sizeof(white_buffer));
    display_trans_cmd_data(0x10, white_buffer, sizeof(white_buffer));
    display_trans_cmd_data(0x13, white_buffer, sizeof(white_buffer));
    display_uc8253c_lut_refresh(EPD_REFRESH_MODE_GC);
    display_uc8253c_refresh();

    memset(white_buffer, 0xFF, sizeof(white_buffer));
    display_trans_cmd_data(0x10, white_buffer, sizeof(white_buffer));
    display_trans_cmd_data(0x13, white_buffer, sizeof(white_buffer));
    display_uc8253c_lut_refresh(EPD_REFRESH_MODE_GC);
    display_uc8253c_refresh();

    refresh_time = 1;
    g_display_obj.orientation = DISPLAY_ORIENTATION_NORMAL;
    g_display_obj.initialized = true;

    return 0;
}

static const struct display_driver_api display_uc8253c_driver_api = {
    .display_blanking_on = display_uc8253c_blanking_on,
    .display_blanking_off = display_uc8253c_blanking_off,
    .display_get_capabilities = display_uc8253c_get_capabilities,
    // .display_set_brightness = display_uc8253c_set_brightness,
    .display_write = display_uc8253c_write,
    .display_set_orientation = display_uc8253c_set_orientation,
};

const struct display_device display_uc8253c = {
    .name = "uc8253c",
    .device_init = display_uc8253c_init,
    .api = &display_uc8253c_driver_api,
};
