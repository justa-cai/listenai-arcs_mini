#include <stdint.h>
#include <stddef.h>
#include "lisa_device.h"
#include "lisa_audio.h"
#include "lisa_player_adapter.h"

#define TAG "lisa_player_adapter"
#include "lisa_log.h"

#define AUDIO_DEVICE_NAME    "audio0"

static app_player_pcm_output_cb_t s_pcm_output_cb = NULL;
static lisa_device_t *s_audio_dev = NULL;

void app_player_adapter_set_pcm_output(app_player_pcm_output_cb_t cb)
{
    s_pcm_output_cb = cb;
}

/* 取一次 audio0 句柄并缓存 (设备句柄本身是常驻的, 不需要每次查) */
static lisa_device_t *audio0_get_dev(void)
{
    if (s_audio_dev == NULL) {
        s_audio_dev = lisa_device_get(AUDIO_DEVICE_NAME);
        if (!s_audio_dev || !lisa_device_ready(s_audio_dev)) {
            LOGE("Error: %s device not ready", AUDIO_DEVICE_NAME);
            s_audio_dev = NULL;
            return NULL;
        }
    }
    return s_audio_dev;
}

int app_audio0_ensure_play(void)
{
    lisa_device_t *audio_dev = audio0_get_dev();
    if (!audio_dev) {
        return -1;
    }

    /* audio0 是单实例, 采样率可能被共用它的功能改掉, 也可能被整个停掉,
     * 所以每次都要确认状态, 不能只在第一次调用时配置一次 (见头文件契约说明)。 */
    lisa_audio_status_t play_status = LISA_AUDIO_STATUS_IDLE;
    int ret = lisa_audio_ioctl(audio_dev, LISA_AUDIO_IOCTL_PLAY_GET_STATUS, &play_status);
    if (ret != LISA_DEVICE_OK) {
        LOGE("get play status fail: %d", ret);
        return -1;
    }

    if (play_status == LISA_AUDIO_STATUS_RUNNING) {
        return 0;   /* 共享流已在跑: 不重配、不打扰正在播放的一方 */
    }
    if (play_status != LISA_AUDIO_STATUS_IDLE) {
        /* 暂停/错误态: 此刻写入会阻塞 (DMA 不回收 buffer), 让调用方稍后重试 */
        LOGW("audio0 not in IDLE/RUNNING (status: %d)", play_status);
        return -1;
    }

    lisa_audio_play_config_t play_config = {
        .format =
            {
                .sample_rate = (lisa_audio_rate_t)APP_AUDIO0_SAMPLE_RATE,
                .channels = LISA_AUDIO_CH_LEFT,
                .sample_bits = LISA_AUDIO_BIT_16,
            },
        .gain =
            {
                /* 增益可在运行时用 lisa_audio_play_set_gain()/shell play_gain 改,
                 * 这里只给一个有裕量的初值: 数字 -12dB 留出余量, 不削顶 */
                .analog_gain = 0,
                .digital_gain = -12,
            },
        .buffer_count = APP_AUDIO0_BUF_COUNT,
        .buffer_samples = APP_AUDIO0_DMA_SAMPLES,
    };

    ret = lisa_audio_play_config(audio_dev, &play_config);
    if (ret != LISA_DEVICE_OK) {
        LOGE("play_config: %d", ret);
        return -1;
    }

    ret = lisa_audio_play_start(audio_dev);
    if (ret != LISA_DEVICE_OK) {
        LOGE("audio play start fail: %d", ret);
        return -1;
    }

    LOGI("audio0 opened at %uHz (shared stream, %ux%u)",
         (unsigned)APP_AUDIO0_SAMPLE_RATE,
         (unsigned)APP_AUDIO0_BUF_COUNT, (unsigned)APP_AUDIO0_DMA_SAMPLES);
    return 0;
}

void audio_play_send_pcm(char *data, int size)
{
    if (s_pcm_output_cb) {
        s_pcm_output_cb(data, size);
        return;
    }

    lisa_device_t *audio_dev = audio0_get_dev();
    if (!audio_dev || app_audio0_ensure_play() != 0) {
        /* 音频设备还没就绪/处于异常态: 丢掉这一块, 下次调用再试。
         * 不能硬写 —— 对已停的流写入会永久阻塞在 DMA 空闲队列上,
         * 把渲染线程连同整个播放状态一起卡死。 */
        LOGW("audio0 unavailable, drop %d bytes", size);
        return;
    }

    lisa_audio_play_write(audio_dev, (int16_t *)data, size / 2);
}
