#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"
#include "sysheap.h"

#include "lisa_log.h"

#define TAG "img_helper"

struct jpeg_write_context {
    uint8_t *buf;
    uint32_t total_size;
    uint32_t wrote;
};

static void jpeg_write(void *context, void *data, int size)
{
    struct jpeg_write_context *ctx = (struct jpeg_write_context *)context;
    if (ctx == NULL) {
        return;
    }

    uint32_t can_write = ctx->total_size - ctx->wrote;
    can_write = can_write >= size ? size : can_write;

    memcpy(ctx->buf + ctx->wrote, data, can_write);
    ctx->wrote += can_write;
}

int img_helper_rgb888_to_jpeg(const uint8_t *rgb_data, uint32_t width, uint32_t height, uint8_t **jpeg_data, uint32_t *jpeg_len)
{
    if (!rgb_data || !jpeg_data || !jpeg_len || width == 0 || height == 0) {
        return -1;
    }

    uint32_t jpeg_buf_size = width * height * 3 / 5;
    if (jpeg_buf_size < 1024) {
        jpeg_buf_size = 1024;
    }

    uint8_t *jpeg_buf = psram_malloc(jpeg_buf_size);
    if (!jpeg_buf) {
        LOGE("failed to allocate JPEG buffer");
        return -1;
    }

    struct jpeg_write_context jpeg_write_ctx = {
        .buf = jpeg_buf,
        .total_size = jpeg_buf_size,
        .wrote = 0,
    };

    int result = tje_encode_with_func(jpeg_write, &jpeg_write_ctx, 1, width, height, 3, rgb_data);
    if (result == 0) {
        LOGE("JPEG encoding failed");
        psram_free(jpeg_buf);
        return -1;
    }

    *jpeg_data = jpeg_buf;
    *jpeg_len = jpeg_write_ctx.wrote;

    return 0;
}

void img_helper_rgb565_to_rgb888(const uint16_t *rgb565, uint8_t *rgb888, uint32_t width, uint32_t height)
{
    uint32_t pixel_count = width * height;

    for (uint32_t i = 0; i < pixel_count; i++) {
        uint16_t pixel = rgb565[i];

        uint8_t r5 = (pixel >> 11) & 0x1F;
        uint8_t g6 = (pixel >> 5) & 0x3F;
        uint8_t b5 = pixel & 0x1F;

        rgb888[i * 3 + 0] = (r5 * 255 + 15) / 31;
        rgb888[i * 3 + 1] = (g6 * 255 + 31) / 63;
        rgb888[i * 3 + 2] = (b5 * 255 + 15) / 31;
    }
}

int img_helper_rgb565_to_jpeg(uint8_t *rgb565, uint32_t len, int width, int height, uint8_t **jpeg_data, uint32_t *jpeg_len)
{
    if (!rgb565 || !jpeg_data || !jpeg_len || width <= 0 || height <= 0) {
        LOGE("Invalid parameters");
        return -1;
    }

    uint32_t pixel_count = len / 2;
    uint8_t *rgb888 = psram_malloc(pixel_count * 3);
    if (!rgb888) {
        LOGE("Failed to allocate RGB888 buffer");
        return -1;
    }

    img_helper_rgb565_to_rgb888((const uint16_t *)rgb565, rgb888, width, height);

    uint8_t *jpeg_buf = NULL;
    uint32_t jpeg_size = 0;
    int ret = img_helper_rgb888_to_jpeg(rgb888, width, height, &jpeg_buf, &jpeg_size);
    psram_free(rgb888);

    if (ret != 0) {
        LOGE("Failed to encode JPEG");
        return -1;
    }

    *jpeg_data = jpeg_buf;
    *jpeg_len = jpeg_size;

    return 0;
}

void img_helper_jpeg_free(void *jpeg)
{
    if (jpeg) {
        psram_free(jpeg);
    }
}
