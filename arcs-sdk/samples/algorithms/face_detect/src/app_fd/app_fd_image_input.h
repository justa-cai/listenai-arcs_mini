#pragma once

#include <stdint.h>
#include "acomp_fd.h"

typedef struct {
    uint8_t *               image_data;                       /* Image data pointer */
    uint32_t                image_index;                      /* Image index */
    uint32_t                image_length;                     /* Image data length */
    uint16_t                image_width;                      /* Image width */
    uint16_t                image_height;                     /* Image height */
    acomp_fd_pixel_format   image_format;                           /* Image Pixel format */
} image_frame_t;

int app_fd_image_stream_init(void);
int app_fd_image_send(image_frame_t *frame);