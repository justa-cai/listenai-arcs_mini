/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __UVC_STREAM_H__
#define __UVC_STREAM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef void (*video_release_t)(void *data);

/**
 * @brief UVC stream configuration
 */
typedef struct {
    uint8_t *data;
    uint32_t len;
    uint16_t width;          /* Video width */
    uint16_t height;         /* Video height */
    uint32_t  format;         /* Video format (MJPEG, YUV, etc.) */
    video_release_t func;
    void *func_param;
} video_info_t;

int uvc_stream_init(void);

void uvc_stream_send_data(video_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* __UVC_STREAM_H__ */
