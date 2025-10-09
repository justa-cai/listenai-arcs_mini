/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
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

static uint8_t lut_flag = 0; // 调用波形标志位
static int refresh_time = 0; // 刷新计数
static uint8_t white_buffer[CONFIG_DISPLAY_UC8253C_WIDTH * CONFIG_DISPLAY_UC8253C_HEIGHT / 8];

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

static void UC8253C_Reset(void)
{
    disp_comm_rst_set();
    delay_ms(100);
    disp_comm_rst_clr();
    delay_ms(100);
    disp_comm_rst_set();
    delay_ms(100);
}

// 进入睡眠模式
static void UC8253C_sleep(void)
{
    display_trans_cmd_data(0x07, (uint8_t[]){0xA5}, 1);
}

// 开始执行刷屏
static int UC8253C_refresh(void)
{
    display_trans_cmd_data(0x17, (uint8_t[]){0xA5}, 1);
#if CONFIG_LISA_DISPLAY_BUSY_SYNC
    if (disp_comm_busy_wait(g_display_obj.busy_sem, 1000) != 0) {
        CLOGE("[%s] Failed to take BUSY semaphore", __FUNCTION__);
        xSemaphoreGive(g_display_obj.mutex);
        return -1;
    }
#endif
}

static void uc8253c_lut_refresh(epd_refresh_mode_e mode)
{
    if (mode == EPD_REFRESH_MODE_GC) {
        if (lut_flag == 0) {
            display_trans_cmd_data(0x50, (uint8_t[]){0xD7}, 1);
        } else {
            display_trans_cmd_data(0x50, (uint8_t[]){0xC7}, 1);
        }
        lut_flag = !lut_flag;

        display_trans_cmd_data(0x20, (uint8_t *)lut_R20_GC, sizeof(lut_R20_GC)); // VCOM
        display_trans_cmd_data(0x21, (uint8_t *)lut_R21_GC, sizeof(lut_R21_GC)); // WW red not use
        display_trans_cmd_data(0x24, (uint8_t *)lut_R24_GC, sizeof(lut_R24_GC)); // BB b
        display_trans_cmd_data(0x22, (uint8_t *)lut_R22_GC, sizeof(lut_R22_GC)); // BW r
        display_trans_cmd_data(0x23, (uint8_t *)lut_R23_GC, sizeof(lut_R23_GC)); // WB w
    } else if (mode == EPD_REFRESH_MODE_DU) {
        if (lut_flag == 0) {
            display_trans_cmd_data(0x50, (uint8_t[]){0xD7}, 1);
        } else {
            display_trans_cmd_data(0x50, (uint8_t[]){0xC7}, 1);
        }
        lut_flag = !lut_flag;

        display_trans_cmd_data(0x20, (uint8_t *)lut_R20_DU, sizeof(lut_R20_DU)); // VCOM
        display_trans_cmd_data(0x21, (uint8_t *)lut_R21_DU, sizeof(lut_R21_DU)); // WW red not use
        display_trans_cmd_data(0x24, (uint8_t *)lut_R24_DU, sizeof(lut_R24_DU)); // BB b
        display_trans_cmd_data(0x22, (uint8_t *)lut_R22_DU, sizeof(lut_R22_DU)); // BW r
        display_trans_cmd_data(0x23, (uint8_t *)lut_R23_DU, sizeof(lut_R23_DU)); // WB w
    }
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

    disp_comm_busy_init(g_display_obj.busy_sem, &config->busy);
#endif

    display_trans_ctx_init(1, 8, 0, config); // 1bpp
    UC8253C_Reset();
    lut_flag = 0;

    for (uint32_t i = 0; i < sizeof(disp_init_items) / sizeof(disp_init_items[0]); i++) {
        display_trans_cmd_data(disp_init_items[i].cmd, (uint8_t *)disp_init_items[i].data,
                               disp_init_items[i].data_bytes);
    }
    SysTick_Delay_Ms(120);

    // 第一次刷新 0xFF 填充全白
    memset(white_buffer, 0xFF, sizeof(white_buffer));
    display_trans_cmd_data(0x10, white_buffer, sizeof(white_buffer));

    uc8253c_lut_refresh(EPD_REFRESH_MODE_GC);
    UC8253C_refresh();
    g_display_obj.orientation = DISPLAY_ORIENTATION_NORMAL;
    g_display_obj.initialized = true;

    return 0;
}

int display_uc8253c_blanking_on(void)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    UC8253C_sleep();
    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

int display_uc8253c_blanking_off(void)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    UC8253C_refresh();
    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

int display_uc8253c_set_brightness(const uint8_t brightness)
{
    disp_comm_brightness_set(brightness);
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

int display_uc8253c_write(const uint16_t x, const uint16_t y, const struct display_buffer_descriptor *desc,
                          const void *buf)
{
    int ret = 0;
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    LOGD("[%s] x: %d, y: %d, width: %d, height: %d", __FUNCTION__, x, y, desc->width, desc->height);
#if CONFIG_DCACHE_ENABLE
    dcache_clean_range((uint32_t)buf, (uint32_t)buf + desc->height * desc->pitch);
#endif
    const size_t image_size = (CONFIG_DISPLAY_UC8253C_WIDTH * CONFIG_DISPLAY_UC8253C_HEIGHT / 8);

    if (refresh_time < 1) { // 第一次要复位、初始化、用GC刷新
        // Transfer old data - 创建全白数据缓冲区
        memset(white_buffer, 0xFF, image_size);
        display_trans_cmd_data(0x10, white_buffer, image_size);

        // Transfer new data - 直接传输图像数据
        display_trans_cmd_data(0x13, buf, image_size);

        uc8253c_lut_refresh(EPD_REFRESH_MODE_GC);
        ret = UC8253C_refresh();
    } else {
        // 使用 DU mode 进行刷新
        display_trans_cmd_data(0X50, (uint8_t[]){0xD7}, 1); // Border
        display_trans_cmd_data(0x13, buf, image_size);

        uc8253c_lut_refresh(EPD_REFRESH_MODE_DU);
        ret = UC8253C_refresh();
    }
    // SysTick_Delay_Ms(100);
    refresh_time++;
    if (refresh_time > 10) {
        refresh_time = 0; // 设置DU mode快刷10次后改用GC mode刷屏一次
    }

    xSemaphoreGive(g_display_obj.mutex);
    return ret;
}

int display_uc8253c_set_orientation(const enum display_orientation orientation)
{
    xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
    g_display_obj.orientation = orientation;
    xSemaphoreGive(g_display_obj.mutex);
    return 0;
}

static const struct display_driver_api display_uc8253c_driver_api = {
    .display_blanking_on = display_uc8253c_blanking_on,
    .display_blanking_off = display_uc8253c_blanking_off,
    .display_get_capabilities = display_uc8253c_get_capabilities,
    .display_set_brightness = display_uc8253c_set_brightness,
    .display_write = display_uc8253c_write,
    .display_set_orientation = display_uc8253c_set_orientation,
};

const struct display_device display_uc8253c = {
    .name = "uc8253c",
    .device_init = display_uc8253c_init,
    .api = &display_uc8253c_driver_api,
};
