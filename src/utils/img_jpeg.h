#ifndef IMG_JPEG_H
#define IMG_JPEG_H

#include <stdint.h>

int image_rgb565_to_jpeg(const uint8_t *rgb565, uint32_t len, int width, int height,
			 uint8_t **jpeg_data, uint32_t *jpeg_len);
void image_jpeg_free(void *jpeg);

#endif
