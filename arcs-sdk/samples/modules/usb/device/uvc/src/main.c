/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief Camera to UVC streaming demo
 *
 */

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "camera/video_camera.h"
#include "uvc/uvc_stream.h"

#define TAG "main"
#include <lisa_log.h>

static void on_frame_captured(camera_frame_t *frame, void *user_data)
{
    (void)user_data;
    
    LISA_LOGD(TAG, "camera buf:0x%p, len: %d, w:%d, h:%d, f:%d", 
                    frame->buffer, 
                    frame->length,
                    frame->width,
                    frame->height,
                    frame->format);

    uvc_stream_send_data((video_info_t *)frame);
}

int main(void)
{
    int ret;

    LISA_LOGI(TAG, "=== Camera UVC Streaming Demo");
    LISA_LOGI(TAG, "Resolution: %dx%d @ %d fps", CONFIG_IMAGE_WIDTH, CONFIG_IMAGE_HEIGHT, CONFIG_IMAGE_FPS);

    /* Step 1: Initialize Camera */
    camera_config_t config = {
        .width = CONFIG_IMAGE_WIDTH,
        .height = CONFIG_IMAGE_HEIGHT,
        .format = LISA_CAMERA_PIXFMT_YUV422,
    };
    ret = video_camera_init(&config);
    if (ret != 0) {
        LISA_LOGE(TAG, "Camera init failed");
        return -1;
    }

    /* Step 2: Initialize UVC */
    ret = uvc_stream_init();
    if (ret != 0) {
        LISA_LOGE(TAG, "UVC init failed");
        return -1;
    }
    
    video_camera_register_callback(on_frame_captured, NULL);

    ret = video_camera_start_capture();
    if (ret != 0) {
        LISA_LOGE(TAG, "Camera start failed");
        return -1;
    }

    LISA_LOGI(TAG, "Camera capture started");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    return 0;
}
