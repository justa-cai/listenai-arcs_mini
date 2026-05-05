/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LOG_TAG "coex.bt.audio"
#include <lisa_log.h>

#include "FreeRTOS.h"
#include "task.h"

#include "bt_audio_interface_virtual.h"

#include "audio_pcm.h"
#include "coex_bt.h"
#include "coex_bt_audio.h"

#define COEX_USE_PCM_ENCODE_MODE 1
#define COEX_SAMPLE_RATE 44100
#define COEX_CHANNELS 2
#define COEX_BITS_PER_SAMPLE 16
#define COEX_PCM_FRAME_SIZE (COEX_SAMPLE_RATE / 100 * COEX_CHANNELS * (COEX_BITS_PER_SAMPLE / 8))

extern bt_audio_error_t bt_vintf_init(void);
extern bt_audio_error_t bt_vintf_set_encode_mode(bool encode_pcm);

static TaskHandle_t g_audio_task_handle;
static volatile bool g_audio_running;
static volatile bool g_audio_task_exit;
static coex_bt_audio_status_t g_audio_status = {
    .volume = 50,
};

static void coex_audio_source_task(void *arg)
{
    size_t offset = 0;

    (void)arg;

    LOGI("audio source task started (PCM encode mode)");
    while (g_audio_running) {
        if ((offset + COEX_PCM_FRAME_SIZE) >= audio_pcm_len) {
            offset = 0;
        }

        int ret = vintf_playback_write(&audio_pcm[offset], COEX_PCM_FRAME_SIZE);
        if (ret < 0) {
            LOGE("vintf_playback_write failed: %d", ret);
            break;
        }
        offset += COEX_PCM_FRAME_SIZE;
    }

    g_audio_task_exit = true;
    g_audio_task_handle = NULL;
    g_audio_running = false;
    g_audio_status.streaming = false;
    LOGI("audio source task stopped");
    vTaskDelete(NULL);
}

static void coex_audio_open_complete(bt_audio_format_t format)
{
    (void)format;

    if (xTaskCreate(coex_audio_source_task, "coex_audio", 4096, NULL, 8, &g_audio_task_handle) != pdPASS) {
        LOGE("failed to create coex audio task");
        (void)vintf_profile_close();
        g_audio_status.profile_open = false;
        return;
    }

    g_audio_running = true;
    g_audio_task_exit = false;
    g_audio_status.profile_open = true;
    g_audio_status.streaming = true;
    LOGI("bt audio stream started");
}

int coex_bt_audio_init(void)
{
    bt_audio_error_t ret;

    if (g_audio_status.initialized) {
        return 0;
    }

    ret = bt_vintf_init();
    if (ret != BT_AUDIO_OK) {
        LOGE("bt_vintf_init failed: %d", ret);
        return -1;
    }

    g_audio_status.initialized = true;
    LOGI("bt audio virtual interface initialized");
    return 0;
}

int coex_bt_audio_start(void)
{
    vintf_open_info_t info = {
        .type = VINTF_PROFILE_PLAYBACK,
        .open_complete_callback = coex_audio_open_complete,
        .audio_data_callback = NULL,
        .user_data = NULL,
    };
    bt_audio_error_t ret;

    if (!coex_bt_is_a2dp_connected()) {
        LOGW("a2dp is not connected, run bt_connect first");
        return -1;
    }

    if (g_audio_running || g_audio_status.profile_open) {
        LOGW("audio already running or opening");
        return 0;
    }

#if COEX_USE_PCM_ENCODE_MODE
    ret = bt_vintf_set_encode_mode(true);
#else
    ret = bt_vintf_set_encode_mode(false);
#endif
    if (ret != BT_AUDIO_OK) {
        LOGE("bt_vintf_set_encode_mode failed: %d", ret);
        return -1;
    }

    ret = vintf_profile_open(&info);
    if (ret != BT_AUDIO_OK) {
        LOGE("vintf_profile_open failed: %d", ret);
        return -1;
    }

    g_audio_status.profile_open = true;
    LOGI("bt audio open requested");
    return 0;
}

int coex_bt_audio_stop(void)
{
    if (!g_audio_running && !g_audio_status.profile_open) {
        LOGI("audio stream is not running");
        return 0;
    }

    g_audio_running = false;
    for (int i = 0; i < 50 && !g_audio_task_exit && g_audio_task_handle != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    (void)vintf_profile_close();
    g_audio_status.profile_open = false;
    g_audio_status.streaming = false;
    LOGI("bt audio stream stopped");
    return 0;
}

int coex_bt_audio_set_volume(uint8_t volume)
{
    bt_audio_error_t ret;

    if (volume > 100) {
        return -1;
    }

    ret = vintf_set_volume(volume);
    if (ret != BT_AUDIO_OK) {
        LOGE("vintf_set_volume failed: %d", ret);
        return -1;
    }

    g_audio_status.volume = volume;
    LOGI("bt audio volume set to %u", volume);
    return 0;
}

void coex_bt_audio_get_status(coex_bt_audio_status_t *status)
{
    if (status == NULL) {
        return;
    }

    *status = g_audio_status;
}
