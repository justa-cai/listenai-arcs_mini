#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "workqueue.h"

#include "lisa_device.h"
#include "lisa_audio.h"

#include "acomp_wakeup.h"

#define TAG "wakeup_in"
#include "lisa_log.h"

#define AUDIO_DEVICE_NAME    "audio0"

/* 音频参数配置 - 单声道(左声道) */
#define SAMPLE_RATE         LISA_AUDIO_RATE_16K
#define SAMPLE_BITS         LISA_AUDIO_BIT_16
#define RECORD_CHANNELS     LISA_AUDIO_CH_STEREO
#define PLAY_CHANNELS       LISA_AUDIO_CH_LEFT

#define BUFFER_COUNT        12
#define BUFFER_SAMPLES      ACOMP_WAKEUP_AUDIO_INPUT_SAMPLE_CNT

/* Record 增益配置 */
#define RECORD_ANALOG_GAIN     30      /* 16 dB */
#define RECORD_DIGITAL_GAIN    0       /* 8 dB */

/* Play 增益配置 */
#define PLAY_ANALOG_GAIN     0       /* 6 dB */
#define PLAY_DIGITAL_GAIN    -18

/* wakeup audio in stream */
#define WAKEUP_AUDIO_MIX2CH_STREAM_CH_INDEX (0)
#define WAKEUP_AUDIO_MIX2CH_STREAM_CH_CNAME "stream.mix2ch"
#define WAKEUP_AUDIO_MIX2CH_STREAM_BUF_SIZE (ACOMP_WAKEUP_AUDIO_INPUT_LEN_ONCE_FRAME)

typedef struct {
    short mic0;
    short mic1;
}mic_in_t;

typedef struct {
    short ref;
}ref_in_t;

typedef struct {
    mic_in_t *mic;
    ref_in_t *ref;
    uint32_t sample_cnt;
}wakeup_in_msg_t;

static workqueue_t *wakeup_in_wq = NULL;
static short zero_echo[BUFFER_SAMPLES] = {0};
static mic_in_t record_zero_echo[BUFFER_SAMPLES] = {0};

static void stream_data_fusion(acomp_wakeup_audio_in_t *algo_buf_in, mic_in_t *adc_data, ref_in_t *ref_data, int sample_cnt)
{
    for (int i = 0; i < sample_cnt; i++) {
        // 双麦软回采   
        algo_buf_in[i].mic0 = adc_data[i].mic0;
        algo_buf_in[i].mic1 = adc_data[i].mic1;
        algo_buf_in[i].ref0 = ref_data[i].ref;
        algo_buf_in[i].ref1 = ref_data[i].ref;
    }

}

static void wakeup_in_wq_handler(void *para)
{
    uint8_t *buffer;
    uint32_t buf_size;
    uint16_t desc_idx;

    wakeup_in_msg_t *msg = (wakeup_in_msg_t*)para;

    LISA_LOGD(LOG_TAG, "wakeup_in_wq_handler, sample_cnt: %u", msg->sample_cnt);

    buffer = acomp_wakeup_stream_tx_buffer_alloc(WAKEUP_AUDIO_MIX2CH_STREAM_CH_INDEX, &buf_size, &desc_idx);

    if(buffer && buf_size > 0){
        stream_data_fusion((acomp_wakeup_audio_in_t*)buffer, msg->mic, msg->ref, msg->sample_cnt);
        acomp_wakeup_stream_tx_buffer_submit(WAKEUP_AUDIO_MIX2CH_STREAM_CH_INDEX, buffer, buf_size, desc_idx);
    }

    psram_free(msg);
}

/**
 * @brief Unified audio callback handler - 仅收集数据
 */
static void unified_audio_callback(const lisa_audio_event_t *event, void *user_data)
{
    bool has_record = (event->record_buffer != NULL);
    bool has_echo = (event->echo_buffer != NULL);

    if (has_record) {
        LISA_LOGD(LOG_TAG, "unified_audio_callback, record_samples: %d", event->record_samples);
    }

    if (has_echo) {
        LISA_LOGD(LOG_TAG, "unified_audio_callback, echo_samples: %d", event->echo_samples);
    }

    wakeup_in_msg_t *msg = psram_malloc(sizeof(wakeup_in_msg_t));
#if 1
    msg->mic = (mic_in_t*)event->record_buffer;
    msg->ref = (ref_in_t*)(event->echo_buffer ? event->echo_buffer : zero_echo);
    msg->sample_cnt = ACOMP_WAKEUP_AUDIO_INPUT_SAMPLE_CNT;
#else
    msg->mic = (mic_in_t*)record_zero_echo;
    msg->ref = (ref_in_t*)zero_echo;
    msg->sample_cnt = 256;
#endif

    workqueue_submit(wakeup_in_wq, wakeup_in_wq_handler, msg);
}

static int audio_device_init(void)
{   
    int ret = 0;

    /* 初始化GPDMA，lisa_audio会用到 */
    GPDMA_Initialize();

    /* 获取 Audio 设备 */
    lisa_device_t *audio_dev = lisa_device_get(AUDIO_DEVICE_NAME);
    if (!audio_dev) {
        LISA_LOGE(LOG_TAG, "获取 Audio 设备失败");
        return -1;
    }
    LISA_LOGI(LOG_TAG, "Audio 设备获取成功");

    /* 注册统一回调函数 */
    ret = lisa_audio_register_callback(audio_dev, unified_audio_callback, NULL);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "注册统一回调失败: %d", ret);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "统一回调注册成功");

    /* 统一配置 Record */
    lisa_audio_record_config_t record_config = {
        .format = { .sample_rate = SAMPLE_RATE, .channels = RECORD_CHANNELS, .sample_bits = SAMPLE_BITS },
        .gain = { .analog_gain = RECORD_ANALOG_GAIN, .digital_gain = RECORD_DIGITAL_GAIN },
        .differential_input = true,
        .enable_hpf = false,
    };
    ret = lisa_audio_record_config(audio_dev, &record_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Record 配置失败: %d", ret);
        goto cleanup;
    }

    /* 统一配置 Play */
    lisa_audio_play_config_t play_config = {
        .format = { .sample_rate = SAMPLE_RATE, .channels = PLAY_CHANNELS, .sample_bits = SAMPLE_BITS },
        .gain = { .analog_gain = PLAY_ANALOG_GAIN, .digital_gain = PLAY_DIGITAL_GAIN },
        .buffer_count = BUFFER_COUNT,
        .buffer_samples = BUFFER_SAMPLES,
    };
    ret = lisa_audio_play_config(audio_dev, &play_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Play 配置失败: %d", ret);
        goto cleanup;
    }

    /* 统一启动录音和播音 */
    ret = lisa_audio_record_start(audio_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "启动录音失败: %d", ret);
        goto cleanup;
    }
    LISA_LOGI(LOG_TAG, "录音已启动");

    ret = lisa_audio_play_start(audio_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "启动播音失败: %d", ret);
        lisa_audio_record_stop(audio_dev);
        goto cleanup;
    }
    LISA_LOGI(LOG_TAG, "播音已启动");

    /* 设置相位补偿 */
    lisa_audio_phase_compensation_t phase_comp = {
        .record_skip_samples = 10,
        .echo_skip_samples = 0,
    };
    ret = lisa_audio_set_phase_compensation(audio_dev, &phase_comp);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "设置相位补偿失败: %d", ret);
        return ret;
    }
    LISA_LOGI(LOG_TAG, "相位补偿已设置");

    return 0;
cleanup:
    lisa_audio_record_stop(audio_dev);
    lisa_audio_play_stop(audio_dev);
    vTaskDelay(pdMS_TO_TICKS(200)); // 等待资源释放

    /* 注销统一回调 */
    if (audio_dev) {
        lisa_audio_unregister_callback(audio_dev, unified_audio_callback);
        LISA_LOGI(LOG_TAG, "统一回调已注销");
    }
    return ret;
}

int wakeup_audio_in_init(void)
{
    int ret = 0;

    acomp_stream_chn_create_desc_t desc = {
        .cname = WAKEUP_AUDIO_MIX2CH_STREAM_CH_CNAME,
        .direction = ACOMP_STREAM_DIRECTION_M2R,
        .index = WAKEUP_AUDIO_MIX2CH_STREAM_CH_INDEX,
        .buffer_size = WAKEUP_AUDIO_MIX2CH_STREAM_BUF_SIZE,
        .num_descs = 4,
        .kick_policy = 1,
    };

    acomp_wakeup_stream_ch_enable(WAKEUP_AUDIO_MIX2CH_STREAM_CH_INDEX,&desc);

    wakeup_in_wq = workqueue_create("wakeup_in_wq", 9, 10, 8096);
    if (wakeup_in_wq == NULL) {
        LISA_LOGE(TAG, "Failed to create wakeup_in_wq");
    }

    audio_device_init();

    return 0;
}
