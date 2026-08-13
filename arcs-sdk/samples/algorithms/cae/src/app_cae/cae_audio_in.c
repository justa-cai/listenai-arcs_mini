#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "workqueue.h"

#include "lisa_device.h"
#include "lisa_audio.h"

#include "acomp_cae.h"

#define TAG "cae_in"
#include "lisa_log.h"

#define AUDIO_DEVICE_NAME    "audio0"

/* 音频参数配置 - 单麦 + 硬回采参考通道 */
#define SAMPLE_RATE         LISA_AUDIO_RATE_16K
#define SAMPLE_BITS         LISA_AUDIO_BIT_16
#define RECORD_CHANNELS        LISA_AUDIO_CH_STEREO
#define RECORD_FRAME_CHANNELS  2
#define PLAY_CHANNELS          LISA_AUDIO_CH_LEFT

#define BUFFER_COUNT        12
#define BUFFER_SAMPLES      ACOMP_CAE_AUDIO_INPUT_SAMPLE_CNT

/* Record 增益配置 */
#define RECORD_ANALOG_GAIN     30
#define RECORD_DIGITAL_GAIN    0

/* Play 增益配置 */
#define PLAY_ANALOG_GAIN       0
#define PLAY_DIGITAL_GAIN      -18

#define TONE_TASK_STACK_SIZE   2048
#define TONE_TASK_PRIORITY     7
#define TONE_TABLE_SAMPLES     16

/* CAE audio in stream */
#define CAE_AUDIO_IN_STREAM_CH_INDEX (0)
#define CAE_AUDIO_IN_STREAM_CH_CNAME "stream.mix2ch"
#define CAE_AUDIO_IN_STREAM_BUF_SIZE (ACOMP_CAE_AUDIO_INPUT_LEN_ONCE_FRAME)

typedef struct {
    short mic0;
    short ref0;
} cae_input_sample_t;

typedef struct {
    cae_input_sample_t samples[ACOMP_CAE_AUDIO_INPUT_SAMPLE_CNT];
    uint32_t sample_cnt;
} cae_in_msg_t;

static workqueue_t *cae_in_wq = NULL;
static const cae_input_sample_t record_zero[ACOMP_CAE_AUDIO_INPUT_SAMPLE_CNT] = {0};
static TaskHandle_t tone_task_handle = NULL;
static volatile bool tone_task_running = false;

static const int16_t tone_1khz_table[TONE_TABLE_SAMPLES] = {
    0, 1567, 2896, 3784, 4096, 3784, 2896, 1567,
    0, -1567, -2896, -3784, -4096, -3784, -2896, -1567,
};
static int16_t tone_1khz_buffer[BUFFER_SAMPLES];

static void fill_tone_1khz_buffer(void)
{
    for (uint32_t i = 0; i < BUFFER_SAMPLES; i++) {
        tone_1khz_buffer[i] = tone_1khz_table[i % TONE_TABLE_SAMPLES];
    }
}

static void tone_play_task(void *param)
{
    lisa_device_t *audio_dev = (lisa_device_t *)param;

    while (tone_task_running) {
        int written = lisa_audio_play_write(audio_dev, tone_1khz_buffer, BUFFER_SAMPLES);
        if (written < 0) {
            LISA_LOGW(TAG, "写入1kHz测试音失败: %d", written);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    tone_task_handle = NULL;
    vTaskDelete(NULL);
}

static int start_tone_play_task(lisa_device_t *audio_dev)
{
    fill_tone_1khz_buffer();

    tone_task_running = true;
    if (xTaskCreate(tone_play_task, "cae_1khz_play", TONE_TASK_STACK_SIZE,
                    audio_dev, TONE_TASK_PRIORITY, &tone_task_handle) != pdPASS) {
        tone_task_running = false;
        tone_task_handle = NULL;
        LISA_LOGE(TAG, "创建1kHz测试音任务失败");
        return -1;
    }

    LISA_LOGI(TAG, "1kHz测试音任务已启动");
    return 0;
}

static void stop_tone_play_task(void)
{
    tone_task_running = false;
    vTaskDelay(pdMS_TO_TICKS(20));
}

static void stream_data_fusion(acomp_cae_audio_in_t *algo_buf_in,
                               const cae_input_sample_t *audio_data,
                               uint32_t sample_cnt)
{
    for (uint32_t i = 0; i < sample_cnt; i++) {
        algo_buf_in[i].mic0 = audio_data[i].mic0;
        algo_buf_in[i].ref0 = audio_data[i].ref0;
    }
}

static void cae_in_wq_handler(void *para)
{
    uint8_t *buffer;
    uint32_t buf_size;
    uint16_t desc_idx;
    cae_in_msg_t *msg = (cae_in_msg_t *)para;

    if (msg == NULL) {
        return;
    }

    buffer = acomp_cae_stream_tx_buffer_alloc(CAE_AUDIO_IN_STREAM_CH_INDEX, &buf_size, &desc_idx);
    if (buffer != NULL && buf_size >= ACOMP_CAE_AUDIO_INPUT_LEN_ONCE_FRAME) {
        stream_data_fusion((acomp_cae_audio_in_t *)buffer, msg->samples, msg->sample_cnt);
        acomp_cae_stream_tx_buffer_submit(CAE_AUDIO_IN_STREAM_CH_INDEX, buffer,
                                          ACOMP_CAE_AUDIO_INPUT_LEN_ONCE_FRAME, desc_idx);
    } else {
        LISA_LOGW(TAG, "acomp_cae_stream_tx_buffer_alloc failed, buffer:%p size:%u", buffer, buf_size);
    }

    psram_free(msg);
}

static void unified_audio_callback(const lisa_audio_event_t *event, void *user_data)
{
    (void)user_data;

    if (event == NULL || cae_in_wq == NULL) {
        return;
    }

    cae_in_msg_t *msg = psram_malloc(sizeof(cae_in_msg_t));
    if (msg == NULL) {
        LISA_LOGE(TAG, "alloc cae_in_msg failed");
        return;
    }

    /* record_samples 是交织双通道 int16 采样点数，转换为算法帧数后再拷贝。 */
    uint32_t copy_samples = event->record_samples / RECORD_FRAME_CHANNELS;
    if (copy_samples > ACOMP_CAE_AUDIO_INPUT_SAMPLE_CNT) {
        copy_samples = ACOMP_CAE_AUDIO_INPUT_SAMPLE_CNT;
    }

    memset(msg->samples, 0, sizeof(msg->samples));
    const cae_input_sample_t *record = event->record_buffer ?
                                      (const cae_input_sample_t *)event->record_buffer : record_zero;
    memcpy(msg->samples, record, copy_samples * sizeof(cae_input_sample_t));
    msg->sample_cnt = ACOMP_CAE_AUDIO_INPUT_SAMPLE_CNT;

    workqueue_submit(cae_in_wq, cae_in_wq_handler, msg);
}

static int audio_device_init(void)
{
    int ret = 0;

    /* 初始化GPDMA，lisa_audio会用到 */
    GPDMA_Initialize();

    lisa_device_t *audio_dev = lisa_device_get(AUDIO_DEVICE_NAME);
    if (!audio_dev) {
        LISA_LOGE(TAG, "获取 Audio 设备失败");
        return -1;
    }
    LISA_LOGI(TAG, "Audio 设备获取成功");

    ret = lisa_audio_register_callback(audio_dev, unified_audio_callback, NULL);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "注册统一回调失败: %d", ret);
        return -1;
    }
    LISA_LOGI(TAG, "统一回调注册成功");

    lisa_audio_record_config_t record_config = {
        .format = { .sample_rate = SAMPLE_RATE, .channels = RECORD_CHANNELS, .sample_bits = SAMPLE_BITS },
        .gain = { .analog_gain = RECORD_ANALOG_GAIN, .digital_gain = RECORD_DIGITAL_GAIN },
        .differential_input = true,
        .enable_hpf = false,
    };
    ret = lisa_audio_record_config(audio_dev, &record_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "Record 配置失败: %d", ret);
        goto cleanup;
    }

    lisa_audio_play_config_t play_config = {
        .format = { .sample_rate = SAMPLE_RATE, .channels = PLAY_CHANNELS, .sample_bits = SAMPLE_BITS },
        .gain = { .analog_gain = PLAY_ANALOG_GAIN, .digital_gain = PLAY_DIGITAL_GAIN },
        .buffer_count = BUFFER_COUNT,
        .buffer_samples = BUFFER_SAMPLES,
    };
    ret = lisa_audio_play_config(audio_dev, &play_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "Play 配置失败: %d", ret);
        goto cleanup;
    }

    ret = lisa_audio_record_start(audio_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "启动录音失败: %d", ret);
        goto cleanup;
    }
    LISA_LOGI(TAG, "录音已启动");

    ret = lisa_audio_play_start(audio_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "启动播音失败: %d", ret);
        lisa_audio_record_stop(audio_dev);
        goto cleanup;
    }
    LISA_LOGI(TAG, "播音已启动");

    ret = start_tone_play_task(audio_dev);
    if (ret != 0) {
        goto cleanup;
    }

    lisa_audio_phase_compensation_t phase_comp = {
        .record_skip_samples = 10,
        .echo_skip_samples = 0,
    };
    ret = lisa_audio_set_phase_compensation(audio_dev, &phase_comp);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "设置相位补偿失败: %d", ret);
        goto cleanup;
    }
    LISA_LOGI(TAG, "相位补偿已设置");

    return 0;
cleanup:
    stop_tone_play_task();
    lisa_audio_record_stop(audio_dev);
    lisa_audio_play_stop(audio_dev);
    vTaskDelay(pdMS_TO_TICKS(200));

    if (audio_dev) {
        lisa_audio_unregister_callback(audio_dev, unified_audio_callback);
        LISA_LOGI(TAG, "统一回调已注销");
    }
    return ret;
}

int cae_audio_in_init(void)
{
    acomp_stream_chn_create_desc_t desc = {
        .cname = CAE_AUDIO_IN_STREAM_CH_CNAME,
        .direction = ACOMP_STREAM_DIRECTION_M2R,
        .index = CAE_AUDIO_IN_STREAM_CH_INDEX,
        .buffer_size = CAE_AUDIO_IN_STREAM_BUF_SIZE,
        .num_descs = 4,
        .kick_policy = 1,
    };

    int ret = acomp_cae_stream_ch_enable(CAE_AUDIO_IN_STREAM_CH_INDEX, &desc);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp_cae_stream_ch_enable failed: %d", ret);
        return ret;
    }

    cae_in_wq = workqueue_create("cae_in_wq", 9, 10, 8096);
    if (cae_in_wq == NULL) {
        LISA_LOGE(TAG, "Failed to create cae_in_wq");
        return -1;
    }

    ret = audio_device_init();
    if (ret == 0) {
        LISA_LOGI(TAG, "CAE 音频输入已启动");
    }

    return ret;
}
