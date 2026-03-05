#pragma once

#include <stdint.h>
#include "lisa_camera.h"

typedef void (*video_frame_release_t)(void *data);

typedef struct {
    uint8_t  *buffer;                       /* Frame buffer pointer */
    uint32_t  length;                       /* Frame data length */
    uint16_t  width;                        /* Frame width */
    uint16_t  height;                       /* Frame height */
    lisa_camera_pixel_format_t   format;    /* Pixel format */
    video_frame_release_t func;             /* camera buffer release function */
    void *func_param;                       /* release function parameters */
} video_frame_t;

int fd_image_stream_init(void);
int fd_image_stream_send(video_frame_t *data);