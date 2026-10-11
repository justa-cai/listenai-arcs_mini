#ifndef IMG_JPEG_H
#define IMG_JPEG_H

#include <stdint.h>

int image_rgb565_to_jpeg(const uint8_t *rgb565, uint32_t len, int width, int height,
			 uint8_t **jpeg_data, uint32_t *jpeg_len);

/* 同上但可指定质量。注意 tiny_jpeg 只有三档：1=最小最快（约 1/20 压缩，
 * 通话档）、2=很好（约 q3 一半大小）、3=最高质量（1/3~1/20 波动）。
 * 传 1-100 的 libjpeg 语义会被拒绝并返回失败。 */
int image_rgb565_to_jpeg_q(const uint8_t *rgb565, uint32_t len, int width, int height,
			   int quality, uint8_t **jpeg_data, uint32_t *jpeg_len);
void image_jpeg_free(void *jpeg);

#endif
