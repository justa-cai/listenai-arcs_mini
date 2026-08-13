/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#include "avi_io.h"

typedef struct av_render av_render_t;

typedef struct avi_player_ctrl avi_player_ctrl_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Play a local AVI file from filesystem.
 *
 * Note:
 * - Currently supports MJPEG video + PCM (s16le) audio in AVI.
 * - Video will be decoded to RGB565 and pushed to av_render.
 */
int avi_player_play_file(const char *path, av_render_t *render, uint32_t max_frames);

/**
 * @brief Play a local AVI file with external control (pause/resume/stop).
 *
 * Notes:
 * - This API is cooperative: pause/stop will be observed between demux operations.
 * - On stop request, the function returns -ECANCELED.
 */
int avi_player_play_file_ex(const char *path, av_render_t *render, uint32_t max_frames, avi_player_ctrl_t *ctrl);

/**
 * @brief Play AVI from a generic IO source.
 *
 * The IO must support read + forward seek.
 */
int avi_player_play_io(avi_io_t *io, av_render_t *render, uint32_t max_frames);

/**
 * @brief Play AVI from a generic IO source with external control.
 *
 * The IO must support read + forward seek.
 */
int avi_player_play_io_ex(avi_io_t *io, av_render_t *render, uint32_t max_frames, avi_player_ctrl_t *ctrl);

#if defined(CONFIG_AVI_PLAYER_HTTP_RANGE)
/**
 * @brief Play AVI from an HTTP URL via Range requests.
 */
int avi_player_play_url(const char *url, av_render_t *render, uint32_t max_frames);

/**
 * @brief Play AVI from an HTTP URL via Range requests with external control.
 */
int avi_player_play_url_ex(const char *url, av_render_t *render, uint32_t max_frames, avi_player_ctrl_t *ctrl);
#endif

/**
 * @brief Create a playback control handle.
 *
 * The handle can be shared across threads:
 * - One thread runs avi_player_play_*_ex()
 * - Another thread calls avi_player_pause/resume/stop()
 */
avi_player_ctrl_t *avi_player_ctrl_create(void);

/**
 * @brief Destroy a playback control handle.
 */
void avi_player_ctrl_destroy(avi_player_ctrl_t *ctrl);

/**
 * @brief Pause playback (demux/push side).
 */
int avi_player_pause(avi_player_ctrl_t *ctrl);

/**
 * @brief Resume playback.
 */
int avi_player_resume(avi_player_ctrl_t *ctrl);

/**
 * @brief Stop playback.
 *
 * After stop is requested, avi_player_play_*_ex() will exit and return -ECANCELED.
 */
int avi_player_stop(avi_player_ctrl_t *ctrl);

/**
 * @brief Get current playback progress.
 *
 * @param ctrl         Playback control handle
 * @param position_ms  [out] Current playback position in milliseconds (can be NULL)
 * @param duration_ms  [out] Total duration in milliseconds (can be NULL)
 * @return 0 on success, negative on error
 */
int avi_player_get_progress(avi_player_ctrl_t *ctrl, uint32_t *position_ms, uint32_t *duration_ms);

#ifdef __cplusplus
}
#endif
