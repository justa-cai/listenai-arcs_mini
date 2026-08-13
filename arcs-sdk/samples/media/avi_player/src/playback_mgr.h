/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PLAYBACK_MGR_H
#define PLAYBACK_MGR_H

#include <stdbool.h>
#include "lisa_device.h"
#include "av_render.h"

typedef enum {
    PLAYBACK_TYPE_LOCAL_FILE,
    PLAYBACK_TYPE_HTTP_URL,
} playback_type_t;

typedef void (*playback_done_cb_t)(void *user);

/**
 * Start playback in a background FreeRTOS task.
 * @param type         local file or HTTP URL
 * @param path_or_url  file path or URL string
 * @param display_dev  display device
 * @param audio_dev    audio device (can be NULL)
 * @param video_ops    LVGL video renderer ops (can be NULL for default)
 * @param video_user   user data for video_ops
 * @return 0 on success, -1 if already playing or error
 */
int playback_start(playback_type_t type,
                   const char *path_or_url,
                   lisa_device_t *display_dev,
                   lisa_device_t *audio_dev,
                   const av_render_video_renderer_ops_t *video_ops,
                   void *video_user);

/**
 * Stop current playback and wait for task to exit.
 */
void playback_stop(void);

/**
 * @return true if playback task is running
 */
bool playback_is_active(void);

/**
 * Set callback invoked when playback finishes naturally (not via stop).
 * Called from the playback task context.
 */
void playback_set_done_callback(playback_done_cb_t cb, void *user);

/**
 * Get current playback progress.
 * @param position_ms  [out] current position in ms (can be NULL)
 * @param duration_ms  [out] total duration in ms (can be NULL)
 * @return 0 on success, -1 if not playing
 */
int playback_get_progress(uint32_t *position_ms, uint32_t *duration_ms);

#endif /* PLAYBACK_MGR_H */
