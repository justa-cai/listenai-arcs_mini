/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "FreeRTOS.h"
#include "task.h"

#include "lisa_device.h"
#include "mic.h"
#include "speaker.h"
#include "uac_device.h"

#define TAG "main"
#include "lisa_log.h"

#define AUDIO_DEVICE_NAME "audio0"

int main(void)
{
    LISA_LOGI(TAG, "TinyUSB UAC2 lisa_audio sample starting");

    lisa_device_t *audio_dev = lisa_device_get(AUDIO_DEVICE_NAME);
    if (audio_dev == NULL) {
        LISA_LOGE(TAG, "failed to get %s", AUDIO_DEVICE_NAME);
        return -1;
    }

    int ret = mic_init(audio_dev);
    if (ret != 0) {
        LISA_LOGE(TAG, "mic_init failed: %d", ret);
        return ret;
    }

    ret = speaker_init();
    if (ret != 0) {
        LISA_LOGE(TAG, "speaker_init failed: %d", ret);
        return ret;
    }

    const uac_device_config_t uac_config = {
        .capture = mic_stream(),
        .playback = speaker_stream(),
    };

    ret = uac_device_start(&uac_config);
    if (ret != 0) {
        LISA_LOGE(TAG, "uac_device_start failed: %d", ret);
        return ret;
    }

    LISA_LOGI(TAG, "TinyUSB UAC2 lisa_audio sample ready");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        LISA_LOGI(TAG, "UAC sample running");
    }

    return 0;
}
