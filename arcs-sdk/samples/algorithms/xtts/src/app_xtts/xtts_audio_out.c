#include <stdint.h>

#include "lisa_device.h"
#include "lisa_audio.h"

#define TAG "xtts_audio"
#include "lisa_log.h"

#define XTTS_AUDIO_DEVICE_NAME       "audio0"
#define XTTS_AUDIO_BUFFER_COUNT      8
#define XTTS_AUDIO_BUFFER_SAMPLES    256

static lisa_device_t *g_audio_dev;
static int g_audio_started;

int xtts_audio_out_init(void)
{
    int ret;

    g_audio_dev = lisa_device_get(XTTS_AUDIO_DEVICE_NAME);
    if ((g_audio_dev == NULL) || !lisa_device_ready(g_audio_dev)) {
        LISA_LOGE(TAG, "%s device not ready", XTTS_AUDIO_DEVICE_NAME);
        return -1;
    }

    lisa_audio_play_config_t play_config = {
        .format = {
            .sample_rate = LISA_AUDIO_RATE_24K,
            .channels = LISA_AUDIO_CH_LEFT,
            .sample_bits = LISA_AUDIO_BIT_16,
        },
        .gain = {
            .analog_gain = 0,
            .digital_gain = -12,
        },
        .buffer_count = XTTS_AUDIO_BUFFER_COUNT,
        .buffer_samples = XTTS_AUDIO_BUFFER_SAMPLES,
    };

    ret = lisa_audio_play_config(g_audio_dev, &play_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "play config failed: %d", ret);
        return ret;
    }

    ret = lisa_audio_play_start(g_audio_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "play start failed: %d", ret);
        return ret;
    }

    g_audio_started = 1;
    return 0;
}

int xtts_audio_out_write(const void *data, uint32_t len)
{
    if ((g_audio_dev == NULL) || !g_audio_started || (data == NULL) || (len == 0)) {
        return -1;
    }

    return lisa_audio_play_write(g_audio_dev, data, len / sizeof(int16_t));
}

int xtts_audio_out_stop(void)
{
    if ((g_audio_dev == NULL) || !g_audio_started) {
        return 0;
    }

    lisa_audio_play_flush(g_audio_dev);
    lisa_audio_play_stop(g_audio_dev);
    g_audio_started = 0;

    return 0;
}
