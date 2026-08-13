#ifndef __LS_DMA2D_H__
#define __LS_DMA2D_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _image_format {
    image_format_rgb888 = 0x0,           // RGB888 (24-bit)
    image_format_bgr888,                 // BGR888 (24-bit)
    image_format_rgb565,                 // RGB565 (16-bit)
    image_format_bgr565,                 // BGR565 (16-bit)
    image_format_yuv444_packed,          // Packed YUV444
    image_format_yuv422_yuyv_packed,     // y0cby1cr
    image_format_yuv422_uyvy_packed,     // cby0cry1
    image_format_yuv422_yvyu_packed,     // y0cry1cb
    image_format_yuv422_vyuy_packed,     // cry0cby1
    image_format_y8                      // Grayscale Y8 format
} image_format_t;

void ls_resize_bilinear_with_roi(unsigned char *src, int srcw, int srch, image_format_t srcf, unsigned char *dst, int dstw, int dsth, image_format_t dstf, int roix, int roiy, int roiw, int roih);
void test_performance_copy_calc_cpu(unsigned char *data, unsigned int width, unsigned int height, unsigned char *pBufBBB, unsigned char *pBufGGG, unsigned char *pBufRRR);

#ifdef __cplusplus
}
#endif
#endif /* __LS_DMA2D_H__ */
