/**
 * @file image_convert.c
 * @brief 图像格式转换和编码实现
 */

#include "image_convert.h"
#include "lisa_log.h"
#include "sysheap.h"
#include <string.h>

#define TAG "image_convert"

// Tiny JPEG编码器
#define NDEBUG
#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"

// Base64编码表
static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * @brief YUV422转RGB888
 * 实际输入格式：UYVY (U0 Y0 V0 Y1)
 */
int image_yuv422_to_rgb888(const uint8_t *yuv422, uint8_t *rgb888, uint32_t width, uint32_t height)
{
    if (!yuv422 || !rgb888 || width == 0 || height == 0) {
        LISA_LOGE(LOG_TAG, "Invalid parameters");
        return -1;
    }

    uint32_t pixel_count = width * height;
    
    for (uint32_t i = 0; i < pixel_count; i += 2) {
        // UYVY: U0 Y0 V0 Y1 (4 bytes for 2 pixels)
        uint32_t yuv_idx = i * 2;
        int u  = yuv422[yuv_idx + 0];
        int y0 = yuv422[yuv_idx + 1];
        int v  = yuv422[yuv_idx + 2];
        int y1 = yuv422[yuv_idx + 3];

        // YUV to RGB conversion (ITU-R BT.601)
        int c0 = y0 - 16;
        int c1 = y1 - 16;
        int d = u - 128;
        int e = v - 128;

        // Pixel 0
        int r0 = (298 * c0 + 409 * e + 128) >> 8;
        int g0 = (298 * c0 - 100 * d - 208 * e + 128) >> 8;
        int b0 = (298 * c0 + 516 * d + 128) >> 8;

        // Pixel 1
        int r1 = (298 * c1 + 409 * e + 128) >> 8;
        int g1 = (298 * c1 - 100 * d - 208 * e + 128) >> 8;
        int b1 = (298 * c1 + 516 * d + 128) >> 8;

        // Clamp to [0, 255]
        #define CLAMP(x) ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))
        
        rgb888[i * 3 + 0] = CLAMP(r0);
        rgb888[i * 3 + 1] = CLAMP(g0);
        rgb888[i * 3 + 2] = CLAMP(b0);
        
        rgb888[i * 3 + 3] = CLAMP(r1);
        rgb888[i * 3 + 4] = CLAMP(g1);
        rgb888[i * 3 + 5] = CLAMP(b1);
        
        #undef CLAMP
    }

    return 0;
}

/**
 * @brief YUV422转RGB565
 * 实际输入格式：UYVY (U0 Y0 V0 Y1)
 */
int image_yuv422_to_rgb565(const uint8_t *yuv422, uint16_t *rgb565, uint32_t width, uint32_t height)
{
    if (!yuv422 || !rgb565 || width == 0 || height == 0) {
        LISA_LOGE(LOG_TAG, "Invalid parameters");
        return -1;
    }

    uint32_t pixel_count = width * height;
    
    for (uint32_t i = 0; i < pixel_count; i += 2) {
        // UYVY: U0 Y0 V0 Y1 (4 bytes for 2 pixels)
        uint32_t yuv_idx = i * 2;
        int u  = yuv422[yuv_idx + 0];
        int y0 = yuv422[yuv_idx + 1];
        int v  = yuv422[yuv_idx + 2];
        int y1 = yuv422[yuv_idx + 3];

        // YUV to RGB conversion (ITU-R BT.601)
        int c0 = y0 - 16;
        int c1 = y1 - 16;
        int d = u - 128;
        int e = v - 128;

        // Pixel 0
        int r0 = (298 * c0 + 409 * e + 128) >> 8;
        int g0 = (298 * c0 - 100 * d - 208 * e + 128) >> 8;
        int b0 = (298 * c0 + 516 * d + 128) >> 8;

        // Pixel 1
        int r1 = (298 * c1 + 409 * e + 128) >> 8;
        int g1 = (298 * c1 - 100 * d - 208 * e + 128) >> 8;
        int b1 = (298 * c1 + 516 * d + 128) >> 8;

        // Clamp to [0, 255]
        #define CLAMP(x) ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))
        
        r0 = CLAMP(r0);
        g0 = CLAMP(g0);
        b0 = CLAMP(b0);
        r1 = CLAMP(r1);
        g1 = CLAMP(g1);
        b1 = CLAMP(b1);
        
        #undef CLAMP

        // Convert to RGB565: RRRRRGGGGGGBBBBB
        rgb565[i + 0] = ((r0 >> 3) << 11) | ((g0 >> 2) << 5) | (b0 >> 3);
        rgb565[i + 1] = ((r1 >> 3) << 11) | ((g1 >> 2) << 5) | (b1 >> 3);
    }

    return 0;
}

/**
 * @brief RGB565转RGB888
 */
int image_rgb565_to_rgb888(const uint16_t *rgb565, uint8_t *rgb888, uint32_t width, uint32_t height)
{
    if (!rgb565 || !rgb888 || width == 0 || height == 0) {
        LISA_LOGE(LOG_TAG, "Invalid parameters");
        return -1;
    }

    uint32_t pixel_count = width * height;
    
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint16_t pixel = rgb565[i];
        
        // Extract RGB565 components
        uint8_t r5 = (pixel >> 11) & 0x1F;
        uint8_t g6 = (pixel >> 5) & 0x3F;
        uint8_t b5 = pixel & 0x1F;
        
        // Convert to RGB888
        rgb888[i * 3 + 0] = (r5 * 255) / 31;
        rgb888[i * 3 + 1] = (g6 * 255) / 63;
        rgb888[i * 3 + 2] = (b5 * 255) / 31;
    }

    return 0;
}

// JPEG写入回调上下文
struct jpeg_write_context {
    uint8_t *buf;
    uint32_t total_size;
    uint32_t wrote;
};

// JPEG写入回调函数
static void jpeg_write_callback(void *context, void *data, int size)
{
    struct jpeg_write_context *ctx = (struct jpeg_write_context *)context;
    if (!ctx || !data || size <= 0) {
        return;
    }

    uint32_t can_write = ctx->total_size - ctx->wrote;
    can_write = (can_write >= (uint32_t)size) ? size : can_write;

    if (can_write > 0) {
        memcpy(ctx->buf + ctx->wrote, data, can_write);
        ctx->wrote += can_write;
    }
}

/**
 * @brief RGB888转JPEG
 */
int image_rgb888_to_jpeg(const uint8_t *rgb888, uint32_t width, uint32_t height,
                         int quality, uint8_t *jpeg_buf, uint32_t jpeg_buf_size,
                         uint32_t *jpeg_size)
{
    if (!rgb888 || !jpeg_buf || !jpeg_size || width == 0 || height == 0) {
        LISA_LOGE(LOG_TAG, "Invalid parameters");
        return -1;
    }

    if (quality < 1 || quality > 3) {
        quality = 1; // Default to best quality
    }

    struct jpeg_write_context ctx = {
        .buf = jpeg_buf,
        .total_size = jpeg_buf_size,
        .wrote = 0,
    };

    // Encode JPEG using tiny_jpeg
    int result = tje_encode_with_func(jpeg_write_callback, &ctx, quality, 
                                      width, height, 3, rgb888);
    
    if (result == 0) {
        LISA_LOGE(LOG_TAG, "JPEG encoding failed");
        return -1;
    }

    *jpeg_size = ctx.wrote;
    LISA_LOGI(LOG_TAG, "JPEG encoded: %u bytes (quality=%d)", ctx.wrote, quality);

    return 0;
}

/**
 * @brief JPEG转Base64
 */
int image_jpeg_to_base64(const uint8_t *jpeg_data, uint32_t jpeg_size,
                         char *base64_buf, uint32_t base64_buf_size,
                         uint32_t *base64_len)
{
    if (!jpeg_data || !base64_buf || !base64_len || jpeg_size == 0) {
        LISA_LOGE(LOG_TAG, "Invalid parameters");
        return -1;
    }

    // Calculate required base64 size
    uint32_t required_size = ((jpeg_size + 2) / 3) * 4 + 1; // +1 for '\0'
    if (base64_buf_size < required_size) {
        LISA_LOGE(LOG_TAG, "Base64 buffer too small: need %u, have %u", 
                  required_size, base64_buf_size);
        return -1;
    }

    uint32_t i = 0, j = 0;
    
    // Process 3 bytes at a time
    while (i + 2 < jpeg_size) {
        uint32_t triple = (jpeg_data[i] << 16) | (jpeg_data[i + 1] << 8) | jpeg_data[i + 2];
        
        base64_buf[j++] = base64_table[(triple >> 18) & 0x3F];
        base64_buf[j++] = base64_table[(triple >> 12) & 0x3F];
        base64_buf[j++] = base64_table[(triple >> 6) & 0x3F];
        base64_buf[j++] = base64_table[triple & 0x3F];
        
        i += 3;
    }

    // Handle remaining bytes
    if (i < jpeg_size) {
        uint32_t triple = jpeg_data[i] << 16;
        
        if (i + 1 < jpeg_size) {
            triple |= jpeg_data[i + 1] << 8;
        }
        
        base64_buf[j++] = base64_table[(triple >> 18) & 0x3F];
        base64_buf[j++] = base64_table[(triple >> 12) & 0x3F];
        
        if (i + 1 < jpeg_size) {
            base64_buf[j++] = base64_table[(triple >> 6) & 0x3F];
        } else {
            base64_buf[j++] = '=';
        }
        base64_buf[j++] = '=';
    }

    base64_buf[j] = '\0';
    *base64_len = j;

    LISA_LOGI(LOG_TAG, "Base64 encoded: %u bytes -> %u chars", jpeg_size, j);

    return 0;
}

/**
 * @brief 一键转换：YUV422 -> RGB888 -> JPEG -> Base64
 */
int image_yuv422_to_base64(const uint8_t *yuv422, uint32_t width, uint32_t height,
                           int quality, char **base64_out, uint32_t *base64_len)
{
    if (!yuv422 || !base64_out || !base64_len || width == 0 || height == 0) {
        LISA_LOGE(LOG_TAG, "Invalid parameters");
        return -1;
    }

    int ret = -1;
    uint8_t *rgb888 = NULL;
    uint8_t *jpeg_buf = NULL;
    char *base64_buf = NULL;
    uint32_t jpeg_size = 0;

    LISA_LOGI(LOG_TAG, "Starting conversion: %ux%u YUV422 -> Base64", width, height);

    // 1. Allocate RGB888 buffer
    uint32_t rgb888_size = width * height * 3;
    rgb888 = exram_malloc(32,rgb888_size);
    if (!rgb888) {
        LISA_LOGE(LOG_TAG, "Failed to allocate RGB888 buffer: %u bytes", rgb888_size);
        goto cleanup;
    }

    // 2. YUV422 -> RGB888
    ret = image_yuv422_to_rgb888(yuv422, rgb888, width, height);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "YUV422 to RGB888 conversion failed");
        goto cleanup;
    }
    LISA_LOGI(LOG_TAG, "YUV422 -> RGB888 done");

    // 3. Allocate JPEG buffer (estimate 1/5 of RGB888 size)
    uint32_t jpeg_buf_size = rgb888_size / 5;
    if (jpeg_buf_size < 10240) {
        jpeg_buf_size = 10240; // Minimum 10KB
    }
    jpeg_buf = exram_malloc(32,jpeg_buf_size);
    if (!jpeg_buf) {
        LISA_LOGE(LOG_TAG, "Failed to allocate JPEG buffer: %u bytes", jpeg_buf_size);
        goto cleanup;
    }

    // 4. RGB888 -> JPEG
    ret = image_rgb888_to_jpeg(rgb888, width, height, quality, 
                               jpeg_buf, jpeg_buf_size, &jpeg_size);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "RGB888 to JPEG conversion failed");
        goto cleanup;
    }
    LISA_LOGI(LOG_TAG, "RGB888 -> JPEG done: %u bytes", jpeg_size);

    // RGB888 no longer needed
    exram_free(rgb888);
    rgb888 = NULL;

    // 5. Allocate Base64 buffer
    uint32_t base64_buf_size = ((jpeg_size + 2) / 3) * 4 + 1;
    base64_buf = exram_malloc(32,base64_buf_size);
    if (!base64_buf) {
        LISA_LOGE(LOG_TAG, "Failed to allocate Base64 buffer: %u bytes", base64_buf_size);
        goto cleanup;
    }

    // 6. JPEG -> Base64
    ret = image_jpeg_to_base64(jpeg_buf, jpeg_size, base64_buf, 
                               base64_buf_size, base64_len);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "JPEG to Base64 conversion failed");
        goto cleanup;
    }
    LISA_LOGI(LOG_TAG, "JPEG -> Base64 done: %u chars", *base64_len);

    // Success
    *base64_out = base64_buf;
    ret = 0;

cleanup:
    if (rgb888) {
        exram_free(rgb888);
    }
    if (jpeg_buf) {
        exram_free(jpeg_buf);
    }
    if (ret != 0 && base64_buf) {
        exram_free(base64_buf);
    }

    return ret;
}
