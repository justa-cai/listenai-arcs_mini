/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "playback_mgr.h"
#include "avi_player.h"
#include "av_render.h"
#include "lisa_thread.h"
#include "lisa_mem.h"
#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "playback_mgr"
#include <lisa_log.h>

#define PLAYBACK_TASK_STACK_SIZE  (8 * 1024)
#define PLAYBACK_TASK_PRIORITY    (configMAX_PRIORITIES - 3)

typedef struct {
    playback_type_t type;
    char path_or_url[256];
    lisa_device_t *display_dev;
    lisa_device_t *audio_dev;
    const av_render_video_renderer_ops_t *video_ops;
    void *video_user;
} playback_params_t;

static volatile bool s_active;
static avi_player_ctrl_t *s_ctrl;
static TaskHandle_t s_task_handle;
static playback_done_cb_t s_done_cb;
static void *s_done_user;
static playback_params_t s_params;

static void playback_task(void *param)
{
    playback_params_t *p = (playback_params_t *)param;

    av_render_cfg_t cfg = {
        .display_dev = (void *)p->display_dev,
        .audio_dev = (void *)p->audio_dev,
        .sync_mode = AV_RENDER_SYNC_AUDIO,
        .rotate_mode = AV_RENDER_ROTATE_NONE, /* LVGL handles rotation */
        .video_fps_limit =
#if defined(CONFIG_AVI_PLAYER_FIXED_FPS)
            (uint16_t)CONFIG_AVI_PLAYER_FIXED_FPS_VALUE,
#else
            0,
#endif
        .max_video_ahead_ms = (uint32_t)CONFIG_AVI_PLAYER_MAX_VIDEO_AHEAD_MS,
        .video_queue_depth = 6,
        .audio_queue_depth = 24,
    };

    av_render_t *render = NULL;
    if (av_render_open(&cfg, &render) != 0 || !render) {
        LISA_LOGE(LOG_TAG, "av_render_open failed");
        goto done;
    }

    if (p->video_ops) {
        av_render_register_video_renderer(render, p->video_ops, p->video_user);
    }

    s_ctrl = avi_player_ctrl_create();
    if (!s_ctrl) {
        LISA_LOGE(LOG_TAG, "avi_player_ctrl_create failed");
        av_render_close(render);
        goto done;
    }

    int ret = 0;
    if (p->type == PLAYBACK_TYPE_LOCAL_FILE) {
        LISA_LOGI(LOG_TAG, "Playing file: %s", p->path_or_url);
        ret = avi_player_play_file_ex(p->path_or_url, render, 0, s_ctrl);
    }
#if defined(CONFIG_AVI_PLAYER_HTTP_RANGE)
    else if (p->type == PLAYBACK_TYPE_HTTP_URL) {
        LISA_LOGI(LOG_TAG, "Playing URL: %s", p->path_or_url);
        ret = avi_player_play_url_ex(p->path_or_url, render, 0, s_ctrl);
    }
#endif

    LISA_LOGI(LOG_TAG, "Playback finished: ret=%d", ret);

    av_render_close(render);
    avi_player_ctrl_destroy(s_ctrl);
    s_ctrl = NULL;

done:
    s_active = false;
    s_task_handle = NULL;

    /* Notify done (only if not stopped externally) */
    if (s_done_cb) {
        s_done_cb(s_done_user);
    }

    vTaskDelete(NULL);
}

int playback_start(playback_type_t type,
                   const char *path_or_url,
                   lisa_device_t *display_dev,
                   lisa_device_t *audio_dev,
                   const av_render_video_renderer_ops_t *video_ops,
                   void *video_user)
{
    if (s_active) {
        LISA_LOGW(LOG_TAG, "Already playing");
        return -1;
    }

    memset(&s_params, 0, sizeof(s_params));
    s_params.type = type;
    strncpy(s_params.path_or_url, path_or_url, sizeof(s_params.path_or_url) - 1);
    s_params.display_dev = display_dev;
    s_params.audio_dev = audio_dev;
    s_params.video_ops = video_ops;
    s_params.video_user = video_user;

    s_active = true;

    if (xTaskCreate(playback_task, "playback",
                    PLAYBACK_TASK_STACK_SIZE, &s_params,
                    PLAYBACK_TASK_PRIORITY, &s_task_handle) != pdPASS) {
        LISA_LOGE(LOG_TAG, "Failed to create playback task");
        s_active = false;
        return -1;
    }

    return 0;
}

void playback_stop(void)
{
    if (!s_active) {
        return;
    }

    if (s_ctrl) {
        LISA_LOGI(LOG_TAG, "Requesting playback stop...");
        avi_player_stop(s_ctrl);
    }

    /* Wait for task to finish */
    int timeout_ms = 5000;
    while (s_active && timeout_ms > 0) {
        lisa_thread_mdelay(50);
        timeout_ms -= 50;
    }

    if (s_active) {
        LISA_LOGW(LOG_TAG, "Playback task did not exit in time");
    }
}

bool playback_is_active(void)
{
    return s_active;
}

void playback_set_done_callback(playback_done_cb_t cb, void *user)
{
    s_done_cb = cb;
    s_done_user = user;
}

int playback_get_progress(uint32_t *position_ms, uint32_t *duration_ms)
{
    if (!s_active || !s_ctrl) {
        return -1;
    }
    return avi_player_get_progress(s_ctrl, position_ms, duration_ms);
}
