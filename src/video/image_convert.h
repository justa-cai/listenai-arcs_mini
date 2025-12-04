/**
 * @file image_convert.h
 * @brief 图像格式转换和编码接口
 * 
 * 支持的转换流程：
 * YUV422 -> RGB888 -> JPEG -> Base64
 */

#ifndef __IMAGE_CONVERT_H__
#define __IMAGE_CONVERT_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief YUV422转RGB888
 * 
 * @param yuv422 输入YUV422数据
 * @param rgb888 输出RGB888数据缓冲区（需预先分配 width*height*3 字节）
 * @param width 图像宽度
 * @param height 图像高度
 * @return 0成功，-1失败
 */
int image_yuv422_to_rgb888(const uint8_t *yuv422, uint8_t *rgb888, uint32_t width, uint32_t height);

/**
 * @brief YUV422转RGB565
 * 
 * @param yuv422 输入YUV422数据
 * @param rgb565 输出RGB565数据缓冲区（需预先分配 width*height*2 字节）
 * @param width 图像宽度
 * @param height 图像高度
 * @return 0成功，-1失败
 */
int image_yuv422_to_rgb565(const uint8_t *yuv422, uint16_t *rgb565, uint32_t width, uint32_t height);

/**
 * @brief RGB565转RGB888
 * 
 * @param rgb565 输入RGB565数据
 * @param rgb888 输出RGB888数据缓冲区（需预先分配 width*height*3 字节）
 * @param width 图像宽度
 * @param height 图像高度
 * @return 0成功，-1失败
 */
int image_rgb565_to_rgb888(const uint16_t *rgb565, uint8_t *rgb888, uint32_t width, uint32_t height);

/**
 * @brief RGB888转JPEG
 * 
 * @param rgb888 输入RGB888数据
 * @param width 图像宽度
 * @param height 图像高度
 * @param quality JPEG质量 (1-3: 1=最高质量，3=最低质量)
 * @param jpeg_buf 输出JPEG数据缓冲区（需预先分配足够空间，建议 width*height 字节）
 * @param jpeg_buf_size JPEG缓冲区大小
 * @param jpeg_size 输出实际JPEG大小
 * @return 0成功，-1失败
 */
int image_rgb888_to_jpeg(const uint8_t *rgb888, uint32_t width, uint32_t height, 
                         int quality, uint8_t *jpeg_buf, uint32_t jpeg_buf_size, 
                         uint32_t *jpeg_size);

/**
 * @brief JPEG转Base64
 * 
 * @param jpeg_data JPEG数据
 * @param jpeg_size JPEG数据大小
 * @param base64_buf 输出Base64字符串缓冲区（需预先分配足够空间，建议 jpeg_size*2 字节）
 * @param base64_buf_size Base64缓冲区大小
 * @param base64_len 输出Base64字符串长度（不含'\0'）
 * @return 0成功，-1失败
 */
int image_jpeg_to_base64(const uint8_t *jpeg_data, uint32_t jpeg_size,
                         char *base64_buf, uint32_t base64_buf_size,
                         uint32_t *base64_len);

/**
 * @brief 一键转换：YUV422 -> RGB888 -> JPEG -> Base64
 * 
 * @param yuv422 输入YUV422数据
 * @param width 图像宽度
 * @param height 图像高度
 * @param quality JPEG质量 (1-3)
 * @param base64_out 输出Base64字符串（函数内部分配内存，调用者需释放）
 * @param base64_len 输出Base64字符串长度
 * @return 0成功，-1失败
 * 
 * @note 调用者需要使用 exram_free() 释放 base64_out
 */
int image_yuv422_to_base64(const uint8_t *yuv422, uint32_t width, uint32_t height,
                           int quality, char **base64_out, uint32_t *base64_len);

#ifdef __cplusplus
}
#endif

#endif /* __IMAGE_CONVERT_H__ */
