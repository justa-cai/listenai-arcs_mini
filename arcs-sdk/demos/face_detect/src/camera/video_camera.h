/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "lisa_camera.h"

typedef struct {
    uint16_t width;                         /* Image width */
    uint16_t height;                        /* Image height */
    lisa_camera_pixel_format_t format;      /* Pixel format (RGB565, YUV422, etc.) */
} camera_config_t;

typedef void (*camera_frame_release_t)(void *data);

typedef struct {
    uint8_t  *buffer;                       /* Frame buffer pointer */
    uint32_t  length;                       /* Frame data length */
    uint16_t  width;                        /* Frame width */
    uint16_t  height;                       /* Frame height */
    lisa_camera_pixel_format_t   format;    /* Pixel format */
    camera_frame_release_t func;            /* camera buffer release function */
    void *func_param;                       /* release function parameters */
} camera_frame_t;

typedef void (*camera_frame_callback_t)(camera_frame_t *frame, void *user_data);

int video_camera_init(camera_config_t *config);
int video_camera_deinit(void);
int video_camera_start_capture(void);
int video_camera_stop_capture(void);
int video_camera_register_callback(camera_frame_callback_t callback, void *user_data);
int video_camera_set_gray(bool enable);
