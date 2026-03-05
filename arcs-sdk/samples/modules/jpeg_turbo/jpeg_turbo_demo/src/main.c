/**
 * @file main.c
 * @author Tianshuang Ke(dske@listenai.com)
 * @brief libjpeg-turbo sample
 * @version 0.1
 * @date 2025-11-26
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lisa_log.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "FreeRTOS.h"
#include "task.h"

#include "jpeglib.h"      /* libjpeg-turbo standard API (支持 JCS_RGB565) */
#include "turbojpeg.h"    /* TurboJPEG simplified API */
#include "sample_jpeg.h"  /* NASA Apollo 8 Earthrise image (107262 bytes) */

#define TICKS_TO_MS(ticks) ((uint32_t)((ticks) * portTICK_PERIOD_MS))

/**
 * @brief 将 JPEG 解码为 RGB888 (TurboJPEG API)
 * 
 * @note TurboJPEG API 特点:
 *       - 简洁易用,代码量少
 *       - 性能优秀
 *       - 错误处理激进,遇到 Huffman 错误会立即中止
 *       - 需要完美的 CMake 配置,否则容易失败
 *       - 不推荐用于生产环境,建议使用原生 libjpeg API
 */
static void decode_to_rgb888(void)
{
    tjhandle tjInstance = NULL;
    unsigned char *imgBuf = NULL;
    int width, height, subsamp, colorspace;
    int ret;
    TickType_t t0, t1;
    uint32_t ms_init = 0, ms_header = 0, ms_decompress = 0, ms_destroy = 0;

    LOGI("=== JPEG -> RGB888 ===");

    /* 创建解压缩器实例 */
    t0 = xTaskGetTickCount();
    tjInstance = tjInitDecompress();
    t1 = xTaskGetTickCount();
    ms_init = TICKS_TO_MS(t1 - t0);
    if (!tjInstance) {
        LOGE("Failed to create TurboJPEG decompressor: %s\n", tjGetErrorStr());
        return;
    }

    /* 获取 JPEG 图像信息 */
    t0 = xTaskGetTickCount();
    ret = tjDecompressHeader3(tjInstance, sample_jpeg, sample_jpeg_len, &width, &height, &subsamp, &colorspace);
    t1 = xTaskGetTickCount();
    ms_header = TICKS_TO_MS(t1 - t0);
    if (ret < 0) {
        LOGE("Failed to read JPEG header: %s\n", tjGetErrorStr2(tjInstance));
        goto cleanup;
    }

    LOGI("JPEG Info: %dx%d, subsamp=%d, colorspace=%d, size=%u bytes", width, height, subsamp, colorspace, sample_jpeg_len);

    /* 分配 RGB888 缓冲区 (3 bytes per pixel) - 使用 32 字节对齐 */
    imgBuf = (unsigned char *)psram_malloc_align(32, width * height * 3);
    if (!imgBuf) {
        LOGE("Failed to allocate image buffer\n");
        goto cleanup;
    }
    LOGI("Allocated buffer at %p (should be 32-byte aligned)", imgBuf);

    /* 解压缩为 RGB888 */
    t0 = xTaskGetTickCount();
    ret = tjDecompress2(tjInstance, sample_jpeg, sample_jpeg_len, imgBuf, width, 0 /* pitch */, height, TJPF_RGB,
                        0 /* flags */);
    t1 = xTaskGetTickCount();
    ms_decompress = TICKS_TO_MS(t1 - t0);
    if (ret < 0) {
        LOGE("Failed to decompress JPEG: %s\n", tjGetErrorStr2(tjInstance));
        goto cleanup;
    }

    LOGI("Successfully decoded to RGB888");
    LOGI("First pixel: R=%d, G=%d, B=%d", imgBuf[0], imgBuf[1], imgBuf[2]);

cleanup:
    if (imgBuf) {
        psram_free(imgBuf);
    }
    if (tjInstance) {
        t0 = xTaskGetTickCount();
        tjDestroy(tjInstance);
        t1 = xTaskGetTickCount();
        ms_destroy = TICKS_TO_MS(t1 - t0);
    }

    LOGI("TurboJPEG API time(ms): init=%u, header=%u, decompress=%u, destroy=%u",
         ms_init,
         ms_header,
         ms_decompress,
         ms_destroy);
}

/**
 * @brief 将 JPEG 解码为 YUV420 (TurboJPEG API)
 * 
 * @note TurboJPEG API 特点:同 decode_to_rgb888()
 *       不推荐用于生产环境,建议使用原生 libjpeg API
 */
static void decode_to_yuv420(void)
{
    tjhandle tjInstance = NULL;
    unsigned char *dstBuf = NULL;
    int width, height, subsamp, colorspace;
    int ret;
    TickType_t t0, t1;
    uint32_t ms_init = 0, ms_header = 0, ms_decompress = 0, ms_destroy = 0;

    LOGI("=== JPEG -> YUV420 ===");

    /* 创建解压缩器实例 */
    t0 = xTaskGetTickCount();
    tjInstance = tjInitDecompress();
    t1 = xTaskGetTickCount();
    ms_init = TICKS_TO_MS(t1 - t0);
    if (!tjInstance) {
        LOGE("Failed to create TurboJPEG decompressor: %s\n", tjGetErrorStr());
        return;
    }

    /* 获取 JPEG 图像信息 */
    t0 = xTaskGetTickCount();
    ret = tjDecompressHeader3(tjInstance, sample_jpeg, sample_jpeg_len, &width, &height, &subsamp, &colorspace);
    t1 = xTaskGetTickCount();
    ms_header = TICKS_TO_MS(t1 - t0);
    if (ret < 0) {
        LOGE("Failed to read JPEG header: %s\n", tjGetErrorStr2(tjInstance));
        goto cleanup;
    }

    /* 计算 YUV420 所需缓冲区大小 */
    unsigned long yuvSize = tjBufSizeYUV2(width, 4 /* pad */, height, TJSAMP_420);

    /* 分配 YUV 缓冲区 */
    dstBuf = (unsigned char *)psram_malloc(yuvSize);
    if (!dstBuf) {
        LOGE("Failed to allocate YUV buffer\n");
        goto cleanup;
    }

    /* 解压缩为 YUV420 */
    t0 = xTaskGetTickCount();
    ret = tjDecompressToYUV2(tjInstance, sample_jpeg, sample_jpeg_len, dstBuf, width, 4 /* pad */, height,
                             0 /* flags */);
    t1 = xTaskGetTickCount();
    ms_decompress = TICKS_TO_MS(t1 - t0);
    if (ret < 0) {
        LOGE("Failed to decompress to YUV: %s\n", tjGetErrorStr2(tjInstance));
        goto cleanup;
    }

    LOGI("Successfully decoded to YUV420");
    LOGI("YUV buffer size: %lu bytes", yuvSize);
    LOGI("First Y value: %d", dstBuf[0]);

cleanup:
    if (dstBuf) {
        psram_free(dstBuf);
    }
    if (tjInstance) {
        t0 = xTaskGetTickCount();
        tjDestroy(tjInstance);
        t1 = xTaskGetTickCount();
        ms_destroy = TICKS_TO_MS(t1 - t0);
    }

    LOGI("TurboJPEG API time(ms): init=%u, header=%u, decompress=%u, destroy=%u",
         ms_init,
         ms_header,
         ms_decompress,
         ms_destroy);
}

/**
 * @brief 将 JPEG 解码为 RGB565 (使用原生 libjpeg API)
 * 
 * @note 原生 libjpeg API 特点:
 *       - 稳定可靠,经过长期验证
 *       - 容错能力强,遇到错误会尝试恢复
 *       - 逐行解码,错误影响范围小
 *       - 对 CMake 配置宽容,即使配置不完美也能工作
 *       - 强烈推荐用于生产环境
 *       - 性能仅比 TurboJPEG 慢 2-5%
 * 
 * @note libjpeg-turbo 原生支持 JCS_RGB565 色彩空间,无需手动转换
 */
static void decode_to_rgb565(void)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    unsigned char *imgBuf = NULL;
    JSAMPROW row_pointer[1];
    int row_stride;
    TickType_t t0, t1;
    uint32_t ms_create = 0, ms_memsrc = 0, ms_read_header = 0, ms_start = 0, ms_read_scanlines = 0, ms_finish = 0,
             ms_destroy = 0;

    LOGI("=== JPEG -> RGB565 (Native libjpeg API) ===");

    /* 初始化 JPEG 解压缩对象 */
    cinfo.err = jpeg_std_error(&jerr);
    t0 = xTaskGetTickCount();
    jpeg_create_decompress(&cinfo);
    t1 = xTaskGetTickCount();
    ms_create = TICKS_TO_MS(t1 - t0);

    /* 设置输入源 (从内存) */
    t0 = xTaskGetTickCount();
    jpeg_mem_src(&cinfo, sample_jpeg, sample_jpeg_len);
    t1 = xTaskGetTickCount();
    ms_memsrc = TICKS_TO_MS(t1 - t0);

    /* 读取 JPEG 头信息 */
    t0 = xTaskGetTickCount();
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        t1 = xTaskGetTickCount();
        ms_read_header = TICKS_TO_MS(t1 - t0);
        LOGE("Failed to read JPEG header");
        jpeg_destroy_decompress(&cinfo);
        return;
    }
    t1 = xTaskGetTickCount();
    ms_read_header = TICKS_TO_MS(t1 - t0);

    LOGI("JPEG Info: %dx%d", cinfo.image_width, cinfo.image_height);

    /* 设置解压缩参数 - 直接输出 RGB565 格式 */
    cinfo.out_color_space = JCS_RGB565;
    cinfo.dither_mode = JDITHER_NONE; /* 禁用抖动以提高性能 */

    /* 开始解压缩 */
    t0 = xTaskGetTickCount();
    jpeg_start_decompress(&cinfo);
    t1 = xTaskGetTickCount();
    ms_start = TICKS_TO_MS(t1 - t0);

    /* 分配 RGB565 缓冲区 (2 bytes per pixel) */
    row_stride = cinfo.output_width * 2;
    imgBuf = (unsigned char *)psram_malloc(cinfo.output_height * row_stride);
    if (!imgBuf) {
        LOGE("Failed to allocate RGB565 buffer");
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return;
    }

    /* 逐行解码 */
    t0 = xTaskGetTickCount();
    while (cinfo.output_scanline < cinfo.output_height) {
        row_pointer[0] = &imgBuf[cinfo.output_scanline * row_stride];
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
    }
    t1 = xTaskGetTickCount();
    ms_read_scanlines = TICKS_TO_MS(t1 - t0);

    LOGI("Successfully decoded to RGB565");
    LOGI("Image size: %dx%d", cinfo.output_width, cinfo.output_height);
    LOGI("Buffer size: %u bytes", cinfo.output_height * row_stride);
    
    /* 显示第一个像素的 RGB565 值 */
    uint16_t first_pixel = (imgBuf[1] << 8) | imgBuf[0];
    LOGI("First pixel (RGB565): 0x%04X", first_pixel);

    /* 完成解压缩 */
    t0 = xTaskGetTickCount();
    jpeg_finish_decompress(&cinfo);
    t1 = xTaskGetTickCount();
    ms_finish = TICKS_TO_MS(t1 - t0);

    t0 = xTaskGetTickCount();
    jpeg_destroy_decompress(&cinfo);
    t1 = xTaskGetTickCount();
    ms_destroy = TICKS_TO_MS(t1 - t0);

    if (imgBuf) {
        psram_free(imgBuf);
    }

    LOGI("libjpeg API time(ms): create=%u, mem_src=%u, read_header=%u, start=%u, read_scanlines=%u, finish=%u, destroy=%u",
         ms_create,
         ms_memsrc,
         ms_read_header,
         ms_start,
         ms_read_scanlines,
         ms_finish,
         ms_destroy);
}

/**
 * @brief 压缩 RGB888 到 JPEG
 */
static void encode_rgb_to_jpeg(void)
{
    tjhandle tjInstance = NULL;
    unsigned char *jpegBuf = NULL;
    unsigned long jpegSize = 0;

    LOGI("=== RGB888 -> JPEG (Compression) ===");

    /* 创建一个简单的 80x80 红色图像 (RGB888) */
    int width = 80, height = 80;
    unsigned char *srcBuf = (unsigned char *)psram_malloc(width * height * 3);
    if (!srcBuf) {
        LOGE("Failed to allocate source buffer\n");
        return;
    }

    /* 填充红色 */
    for (int i = 0; i < width * height; i++) {
        srcBuf[i * 3 + 0] = 255; /* R */
        srcBuf[i * 3 + 1] = 0;   /* G */
        srcBuf[i * 3 + 2] = 0;   /* B */
    }

    /* 创建压缩器实例 */
    tjInstance = tjInitCompress();
    if (!tjInstance) {
        LOGE("Failed to create TurboJPEG compressor: %s\n", tjGetErrorStr());
        psram_free(srcBuf);
        return;
    }

    /* 压缩为 JPEG */
    int ret = tjCompress2(tjInstance, srcBuf, width, 0 /* pitch */, height, TJPF_RGB, &jpegBuf, &jpegSize, TJSAMP_444,
                          90 /* quality */, 0 /* flags */);
    if (ret < 0) {
        LOGE("Failed to compress image: %s\n", tjGetErrorStr2(tjInstance));
        goto cleanup;
    }

    LOGI("Successfully compressed to JPEG");
    LOGI("Original size: %d bytes (RGB888)", width * height * 3);
    LOGI("Compressed size: %lu bytes (JPEG)", jpegSize);
    LOGI("Compression ratio: %.2f%%", (float)jpegSize / (width * height * 3) * 100);

cleanup:
    if (srcBuf) {
        psram_free(srcBuf);
    }
    if (jpegBuf) {
        tjFree(jpegBuf);
    }
    if (tjInstance) {
        tjDestroy(tjInstance);
    }
}

int main(int argc, char **argv)
{
    LOGI("==============================================");
    LOGI("      libjpeg-turbo Sample Demo");
    LOGI("==============================================\n");

    /* JPEG 解码示例 */
    LOGI("--- Testing TurboJPEG Decode APIs (Work but not recommended for production) ---");
    decode_to_rgb888();   // 可用,但容错性不如原生 API
    decode_to_yuv420();   // 可用,但容错性不如原生 API
    
    LOGI("\n--- Testing Native libjpeg API (Recommended for production) ---");
    decode_to_rgb565();   // 推荐使用

    /* JPEG 编码示例 */
    LOGI("\n--- Testing TurboJPEG Encode API (Safe to use) ---");
    encode_rgb_to_jpeg(); // 编码功能稳定

    LOGI("==============================================");
    LOGI("      All tests completed!");
    LOGI("      Recommendation: Use native libjpeg API for decoding in production");
    LOGI("==============================================\n");

    return 0;
}