#pragma once

#include <stdint.h>
#include "acomp_palm.h"

typedef struct {
    uint8_t *image_data;
    uint32_t image_index;
    uint32_t image_length;
    uint16_t image_width;
    uint16_t image_height;
    acomp_palm_pixel_format_t image_format;
} palm_image_frame_t;

int app_palm_image_stream_init(void);
int app_palm_image_send(palm_image_frame_t *frame);
