
/*
 * @file main.c
 * @brief 使用 libjpeg-turbo, 解码 JPEG 图像并显示到 LCD
 * @version 0.1
 * @date 2025-11-26
 *
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include <lisa_device.h>
#include <lisa_display.h>
#include <lisa_mem.h>
#include <stdint.h>
#include "IOMuxManager.h"
#include "lisa_gpio.h"
#include "board.h"

#include "FreeRTOS.h"
#include "task.h"
#include "heap_private.h"

#include "jpeglib.h"     /* libjpeg-turbo 原生 API */
#include "sample_jpeg.h" /* JPEG 图像数据 (320x240, 11605 bytes) */

#define LOG_TAG "sample_display"
#include <lisa_log.h>

/*
    为满足不同板型示例场景，重定向设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB

#define LCD_CS_PIN       5
#define LCD_SPI_CLK_PIN  3
#define LCD_SPI_DATA_PIN 1

void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_RST_PIN, CSK_IOMUX_FUNC_ALTER1);
}

void lisa_gpiob_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_TE_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CD_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CS_PIN, CSK_IOMUX_FUNC_DEFAULT);
}

void lisa_spi1_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_CLK_PIN, CSK_IOMUX_FUNC_ALTER6);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_DATA_PIN, CSK_IOMUX_FUNC_ALTER6);
}

#endif

/**
 * @brief 使用原生 libjpeg API 解码 JPEG 为 RGB565
 * @param jpeg_data JPEG 数据指针
 * @param jpeg_size JPEG 数据大小
 * @param output_buf 输出缓冲区 (必须足够大,至少 width*height*2 字节)
 * @param width 返回图像宽度
 * @param height 返回图像高度
 * @return 0:成功, -1:失败
 *
 * @note libjpeg-turbo 原生支持 JCS_RGB565,可直接解码为 RGB565
 */
static int decode_jpeg_to_rgb565(const unsigned char *jpeg_data, unsigned long jpeg_size, unsigned char *output_buf,
                                 int *width, int *height)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    int row_stride;
    TickType_t t0, t1;
    uint32_t ms_create = 0, ms_memsrc = 0, ms_read_header = 0, ms_start = 0, ms_scanlines = 0, ms_finish = 0,
             ms_destroy = 0;

    /* 初始化 JPEG 解压缩对象 */
    cinfo.err = jpeg_std_error(&jerr);
    t0 = xTaskGetTickCount();
    jpeg_create_decompress(&cinfo);
    t1 = xTaskGetTickCount();
    ms_create = (t1 - t0) * portTICK_PERIOD_MS;

    /* 设置输入源 (从内存) */
    t0 = xTaskGetTickCount();
    jpeg_mem_src(&cinfo, jpeg_data, jpeg_size);
    t1 = xTaskGetTickCount();
    ms_memsrc = (t1 - t0) * portTICK_PERIOD_MS;

    /* 读取 JPEG 头信息 */
    t0 = xTaskGetTickCount();
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        t1 = xTaskGetTickCount();
        ms_read_header = (t1 - t0) * portTICK_PERIOD_MS;
        LOGE("Failed to read JPEG header");
        jpeg_destroy_decompress(&cinfo);
        return -1;
    }
    t1 = xTaskGetTickCount();
    ms_read_header = (t1 - t0) * portTICK_PERIOD_MS;

    // LOGI("JPEG Info: %dx%d, color_space=%d, num_components=%d",
    //      cinfo.image_width, cinfo.image_height,
    //      cinfo.jpeg_color_space, cinfo.num_components);

    /* 直接解码为 RGB565 (libjpeg-turbo 原生支持) */
    cinfo.out_color_space = JCS_RGB565;
    cinfo.dither_mode = JDITHER_NONE;

    /* 禁用色彩量化 */
    cinfo.quantize_colors = FALSE;
    cinfo.do_fancy_upsampling = TRUE;
    cinfo.do_block_smoothing = FALSE;

    /* 开始解压缩 */
    t0 = xTaskGetTickCount();
    jpeg_start_decompress(&cinfo);
    t1 = xTaskGetTickCount();
    ms_start = (t1 - t0) * portTICK_PERIOD_MS;

    *width = cinfo.output_width;
    *height = cinfo.output_height;
    row_stride = cinfo.output_width * 2; /* RGB565: 2 bytes per pixel */

    /* 逐行解码直接输出到 RGB565 */
    t0 = xTaskGetTickCount();
    while (cinfo.output_scanline < cinfo.output_height) {
        row_pointer[0] = &output_buf[cinfo.output_scanline * row_stride];
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
    }
    t1 = xTaskGetTickCount();
    ms_scanlines = (t1 - t0) * portTICK_PERIOD_MS;
#if 0
    LOGI("Decoded to RGB565: %dx%d", *width, *height);

    /* 打印前几个像素用于调试 */
    LOGI("First 5 pixels RGB565 (little-endian):");
    uint16_t *pixels = (uint16_t *)output_buf;
    for (int p = 0; p < 5 && p < (*width) * (*height); p++) {
        uint16_t rgb565 = pixels[p];
        /* 从小端序提取 RGB 分量 */
        uint8_t r = ((rgb565 >> 11) & 0x1F) << 3;  /* R5 -> R8 */
        uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;   /* G6 -> G8 */
        uint8_t b = (rgb565 & 0x1F) << 3;          /* B5 -> B8 */
        LOGI("  Pixel[%d]: RGB565=0x%04X (R=%d G=%d B=%d)", p, rgb565, r, g, b);
    }
#endif
    /* 完成解压缩 */
    t0 = xTaskGetTickCount();
    jpeg_finish_decompress(&cinfo);
    t1 = xTaskGetTickCount();
    ms_finish = (t1 - t0) * portTICK_PERIOD_MS;

    t0 = xTaskGetTickCount();
    jpeg_destroy_decompress(&cinfo);
    t1 = xTaskGetTickCount();
    ms_destroy = (t1 - t0) * portTICK_PERIOD_MS;

    LOGI("libjpeg API time(ms): create=%u, mem_src=%u, read_header=%u, start=%u, read_scanlines=%u, finish=%u, destroy=%u",
         ms_create,
         ms_memsrc,
         ms_read_header,
         ms_start,
         ms_scanlines,
         ms_finish,
         ms_destroy);

    return 0;
}

/**
 * @brief 旋转 RGB565 图像 (顺时针)
 * @param src 源图像缓冲区
 * @param src_width 源图像宽度
 * @param src_height 源图像高度
 * @param dst 目标图像缓冲区 (必须足够大: dst_width * dst_height * 2 字节)
 * @param rotate_deg 旋转角度: 0/90/180/270 (顺时针)
 * @param dst_width 返回旋转后宽度
 * @param dst_height 返回旋转后高度
 * @return 0:成功, -1:参数错误
 */
static int rotate_image_rgb565(const uint8_t *src, int src_width, int src_height, uint8_t *dst, int rotate_deg,
                               int *dst_width, int *dst_height)
{
    TickType_t t0, t1;
    uint32_t rotate_time_ms = 0;

    if (src == NULL || dst == NULL || dst_width == NULL || dst_height == NULL) {
        return -1;
    }

    if (rotate_deg != 0 && rotate_deg != 90 && rotate_deg != 180 && rotate_deg != 270) {
        LOGE("Invalid rotate_deg=%d (expect 0/90/180/270)", rotate_deg);
        return -1;
    }

    int out_w = (rotate_deg == 90 || rotate_deg == 270) ? src_height : src_width;
    int out_h = (rotate_deg == 90 || rotate_deg == 270) ? src_width : src_height;
    *dst_width = out_w;
    *dst_height = out_h;

    LOGI("Rotating image %d° clockwise: %dx%d -> %dx%d", rotate_deg, src_width, src_height, out_w, out_h);

    /* 注意: 这里按字节操作,因为此时数据还是大端序 */
    const uint8_t *src_bytes = src;
    uint8_t *dst_bytes = dst;

    if (rotate_deg == 0) {
        t0 = xTaskGetTickCount();
        memcpy(dst_bytes, src_bytes, (size_t)src_width * (size_t)src_height * 2);
        t1 = xTaskGetTickCount();
        rotate_time_ms = (t1 - t0) * portTICK_PERIOD_MS;
        LOGI("Rotation completed, time: %u ms", rotate_time_ms);
        return 0;
    }

    t0 = xTaskGetTickCount();
    for (int y = 0; y < src_height; y++) {
        for (int x = 0; x < src_width; x++) {
            int src_idx = (y * src_width + x) * 2; /* 字节索引 */
            int dst_x = 0;
            int dst_y = 0;

            switch (rotate_deg) {
            case 90:
                /* (x, y) -> (src_height - 1 - y, x) */
                dst_x = src_height - 1 - y;
                dst_y = x;
                break;
            case 180:
                /* (x, y) -> (src_width - 1 - x, src_height - 1 - y) */
                dst_x = src_width - 1 - x;
                dst_y = src_height - 1 - y;
                break;
            case 270:
                /* (x, y) -> (y, src_width - 1 - x) */
                dst_x = y;
                dst_y = src_width - 1 - x;
                break;
            default:
                break;
            }

            int dst_idx = (dst_y * out_w + dst_x) * 2; /* 字节索引 */
            dst_bytes[dst_idx + 0] = src_bytes[src_idx + 0];
            dst_bytes[dst_idx + 1] = src_bytes[src_idx + 1];
        }
    }

    t1 = xTaskGetTickCount();
    rotate_time_ms = (t1 - t0) * portTICK_PERIOD_MS;

    LOGI("Rotation completed, time: %u ms", rotate_time_ms);
    return 0;
}

/**
 * @brief 转换RGB565图像的字节序 (大端 -> 小端)
 * @param buf 图像缓冲区
 * @param width 图像宽度
 * @param height 图像高度
 */
static void convert_byte_order(uint8_t *buf, int width, int height)
{
    LOGI("Converting byte order for LCD...");
    uint16_t *pixels = (uint16_t *)buf;
    int total_pixels = width * height;
    for (int i = 0; i < total_pixels; i++) {
        uint16_t pixel = pixels[i];
        /* 交换字节序: 0xAABB -> 0xBBAA */
        pixels[i] = ((pixel & 0xFF) << 8) | ((pixel >> 8) & 0xFF);
    }
    LOGI("Byte order conversion completed");
}

lisa_device_t *display_init(void)
{

    lisa_device_t *gpioa_dev = lisa_device_get("gpioa");
    lisa_device_t *gpiob_dev = lisa_device_get("gpiob");

    lisa_display_config_t display_config = {
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .bus_config = {.spi_4wire =
                           {
                               .spi_dev = lisa_device_get("spi1"),
                               .cs_gpio = gpiob_dev,
                               .cs_pin = LCD_CS_PIN,
                               .dc_gpio = gpiob_dev,
                               .dc_pin = LCD_CD_PIN,
                               .spi_freq = 50 * 1000 * 1000,

                           }},
        .backlight = {.type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
                      .config.pwm = {.channel = 0, .dev = lisa_device_get("pwm0"), .freq = 2000}},
        .rst_gpio = gpioa_dev,
        .rst_pin = LCD_RST_PIN,
        // .te_gpio = gpiob_dev,
        // .te_pin  = LCD_TE_PIN
    };

    lisa_device_t *display_device = lisa_device_get("display");
    if (!display_device) {
        LISA_LOGE(LOG_TAG, "Failed to get display device");
        return NULL;
    }

    lisa_display_attach_bus(display_device, &display_config);

    return display_device;
}

int main(int argc, char **argv)
{
    uint8_t *image_buf = NULL;
    int jpeg_width = 0, jpeg_height = 0;
    int ret;

    LOGI("==============================================");
    LOGI("  JPEG Decode and Display Demo");
    LOGI("==============================================\n");

    lisa_device_t *display_device = display_init();
    if (display_device == NULL) {
        LOGE("Display initialization failed");
        return -1;
    }
    LOGI("Display initialized successfully");

    /* 分配图像缓冲区 (足够大以容纳解码后的图像) */
    /* 假设 JPEG 最大为 640x480 */
    image_buf = (uint8_t *)exram_malloc(32, 640 * 480 * 2);
    if (image_buf == NULL) {
        LOGE("Failed to allocate image buffer");
        return -1;
    }
    /* 解码 JPEG 为 RGB565 */
    LOGI("Decoding JPEG (%u bytes)...", sample_jpeg_len);

    TickType_t decode_start = xTaskGetTickCount();
    ret = decode_jpeg_to_rgb565(sample_jpeg, sample_jpeg_len, image_buf, &jpeg_width, &jpeg_height);
    TickType_t decode_end = xTaskGetTickCount();
    uint32_t decode_time_ms = (decode_end - decode_start) * portTICK_PERIOD_MS;

    if (ret < 0) {
        LOGE("JPEG decode failed (time: %u ms)", decode_time_ms);
        exram_free(image_buf);
        return -1;
    }

    LOGI("JPEG decoded successfully: %dx%d, time: %u ms", jpeg_width, jpeg_height, decode_time_ms);

    /* 如果JPEG是横向(320x240),旋转 270 度以适配竖屏(240x320) */
    if (jpeg_width == 320 && jpeg_height == 240) {
        int rotated_w = 0;
        int rotated_h = 0;
        uint8_t *rotated_buf = (uint8_t *)exram_malloc(32, jpeg_height * jpeg_width * 2);
        if (rotated_buf == NULL) {
            LOGE("Failed to allocate rotation buffer");
            exram_free(image_buf);
            return -1;
        }

        ret = rotate_image_rgb565(image_buf, jpeg_width, jpeg_height, rotated_buf, 270, &rotated_w, &rotated_h);
        if (ret < 0) {
            LOGE("Image rotation failed");
            exram_free(rotated_buf);
            exram_free(image_buf);
            return -1;
        }

        /* 复制旋转后的图像回原缓冲区 */
        memcpy(image_buf, rotated_buf, (size_t)rotated_w * (size_t)rotated_h * 2);
        exram_free(rotated_buf);

        /* 更新尺寸 */
        jpeg_width = rotated_w;
        jpeg_height = rotated_h;

        LOGI("Image rotated to: %dx%d", jpeg_width, jpeg_height);
    }

    lisa_display_capabilities_t caps;
    lisa_display_get_capabilities(display_device, &caps);
    LISA_LOGI(LOG_TAG, "Display capabilities: %d x %d", caps.width, caps.height);

    size_t buffer_pixels = caps.width * caps.height;
    size_t buffer_size = buffer_pixels * sizeof(uint16_t);
    uint16_t *buffer = lisa_mem_alloc(buffer_size);
    if (!buffer) {
        LISA_LOGE(LOG_TAG, "Failed to allocate buffer");
        return -1;
    }

    lisa_display_blanking_off(display_device);
    int color_idx = 0;
    uint8_t brightness = 0;

    lisa_display_buffer_desc_t desc = {
        .width = caps.width,
        .height = caps.height,
        .buf_size = buffer_size,
    };

    /* 设置显示描述符 */
    desc.width = jpeg_width;
    desc.height = jpeg_height;
    desc.pitch = jpeg_width * 2; /* RGB565: 2 bytes per pixel */
    desc.buf_size = jpeg_height * desc.pitch;

    LOGI("Final image size: %dx%d, pitch=%d, buf_size=%d", jpeg_width, jpeg_height, desc.pitch, desc.buf_size);

    /* 显示图像到屏幕 */
    LOGI("Displaying image to LCD...");
    ret = lisa_display_write(display_device, 0, 0, &desc, image_buf);
    if (ret < 0) {
        LOGE("Display write failed");
    } else {
        LOGI("Image displayed successfully!");
    }

    LOGI("==============================================");
    LOGI("  Demo completed. Image should be visible.");
    LOGI("==============================================\n");

    /* 保持显示 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
