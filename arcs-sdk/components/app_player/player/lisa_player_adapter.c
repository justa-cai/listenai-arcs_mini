#include <stdint.h>
#include "lisa_device.h"
#include "lisa_audio.h"

#define TAG "lisa_player_adapter"
#include "lisa_log.h"

#define AUDIO_DEVICE_NAME    "audio0"
#define AUDIO_BUFFER_CNT     10
#define AUDIO_BUFFER_SAMPLES 256

void audio_play_send_pcm(char *data, int size)
{
    static lisa_device_t *audio_dev = NULL;
    int ret;
    if (audio_dev == NULL) {
        audio_dev = lisa_device_get(AUDIO_DEVICE_NAME);
        if (!lisa_device_ready(audio_dev)) {
            LOGE("Error: %s device not ready", AUDIO_DEVICE_NAME);
            return;
        }

        lisa_audio_play_config_t play_config = {
            .format =
                {
                    .sample_rate = LISA_AUDIO_RATE_16K,
                    .channels = LISA_AUDIO_CH_LEFT,
                    .sample_bits = LISA_AUDIO_BIT_16,
                },
            .gain =
                {
                    .analog_gain = 0,
                    .digital_gain = -12,
                },
            .buffer_count = AUDIO_BUFFER_CNT,
            .buffer_samples = AUDIO_BUFFER_SAMPLES,
        };

        ret = lisa_audio_play_config(audio_dev, &play_config);
        if (ret != LISA_DEVICE_OK) {
            LOGE("play_config: %d", ret);
            return;
        }

        ret = lisa_audio_play_start(audio_dev);
        if (ret != LISA_DEVICE_OK) {
            LOGE("audio play start fail: %d", ret);
            return;
        }
    }
    ret = lisa_audio_play_write(audio_dev, (int16_t *)data, size / 2);
}
