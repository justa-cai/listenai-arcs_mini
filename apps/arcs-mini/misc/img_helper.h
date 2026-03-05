#ifndef IMG_HELPER_H
#define IMG_HELPER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int img_helper_rgb888_to_jpeg(const uint8_t *rgb_data, uint32_t width, uint32_t height, uint8_t **jpeg_data,
                              uint32_t *jpeg_len);
void img_helper_rgb565_to_rgb888(const uint16_t *rgb565, uint8_t *rgb888, uint32_t width, uint32_t height);
int img_helper_rgb565_to_jpeg(uint8_t *rgb565, uint32_t len, int width, int height, uint8_t **jpeg_data,
                              uint32_t *jpeg_len);
void img_helper_jpeg_free(void *jpeg);

#ifdef __cplusplus
}
#endif

#endif
