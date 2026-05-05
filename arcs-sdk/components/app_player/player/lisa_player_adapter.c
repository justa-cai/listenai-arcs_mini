#include <stdint.h>
#include <stddef.h>
#include "lisa_device.h"
#include "lisa_audio.h"
#include "lisa_player_adapter.h"

#define TAG "lisa_player_adapter"
#include "lisa_log.h"

#define AUDIO_DEVICE_NAME    "audio0"
#define AUDIO_BUFFER_CNT     10
#define AUDIO_BUFFER_SAMPLES 256

static app_player_pcm_output_cb_t s_pcm_output_cb = NULL;

void app_player_adapter_set_pcm_output(app_player_pcm_output_cb_t cb)
{
    s_pcm_output_cb = cb;
}

void audio_play_send_pcm(char *data, int size)
{
    if (s_pcm_output_cb) {
        s_pcm_output_cb(data, size);
        return;
    }

    static lisa_device_t *audio_dev = NULL;
    int ret;
    if (audio_dev == NULL) {
        audio_dev = lisa_device_get(AUDIO_DEVICE_NAME);
        if (!lisa_device_ready(audio_dev)) {
            LOGE("Error: %s device not ready", AUDIO_DEVICE_NAME);
            return;
        }

        // 检查当前播放状态
        lisa_audio_status_t play_status = LISA_AUDIO_STATUS_IDLE;
        ret = lisa_audio_ioctl(audio_dev, LISA_AUDIO_IOCTL_PLAY_GET_STATUS, &play_status);
        if (ret != LISA_DEVICE_OK) {
            LOGE("get play status fail: %d", ret);
            return;
        }

        // 只有在 IDLE 状态时才执行配置和启动
        if (play_status == LISA_AUDIO_STATUS_IDLE) {
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
        } else {
            LOGW("Audio is not in IDLE state (current: %d), skip config and start", play_status);
        }
    }
    ret = lisa_audio_play_write(audio_dev, (int16_t *)data, size / 2);
}
