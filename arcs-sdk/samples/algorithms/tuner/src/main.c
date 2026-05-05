#define LOG_TAG "tuner_sample"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "Driver_GPDMA.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "workqueue.h"

#include "acomp.h"
#include "tuner/acomp_tuner.h"
#include "ic_message.h"
#include "lisa_device.h"
#include "lisa_audio.h"
#include "sysheap.h"

#define TUNER_CHN_INPUT  0
#define TUNER_CHN_OUTPUT 1

#define SAMPLE_RATE       48000
#define AUDIO_CHANNELS    LISA_AUDIO_CH_LEFT
#define AUDIO_BITS        LISA_AUDIO_BIT_16

#define TUNER_STREAM_BUFFER_SIZE 5120
#define TUNER_STREAM_NUM_DESCS 16

#define AUDIO_DEVICE_NAME "audio0"
#define PLAY_BUFFER_COUNT    12
#define PLAY_BUFFER_SAMPLES  256
#define TUNER_OUTPUT_TASK_STACK_SIZE 4096
#define TUNER_OUTPUT_TASK_PRIORITY   8

#define RECORD_ANALOG_GAIN   16
#define RECORD_DIGITAL_GAIN  8
#define PLAY_ANALOG_GAIN     0
#define PLAY_DIGITAL_GAIN    -12

typedef struct {
    const int16_t *data;
    uint32_t samples;
} tuner_audio_msg_t;

static lisa_device_t *audio_dev;
static workqueue_t *tuner_wq;
static SemaphoreHandle_t tuner_rx_sem;
static TaskHandle_t tuner_out_task_handle;
static volatile bool tuner_running;
static volatile uint32_t frame_count;

static void tuner_workqueue_destroy(workqueue_t **queue)
{
    if (queue == NULL || *queue == NULL) {
        return;
    }

    if ((*queue)->taskHandle != NULL) {
        vTaskDelete((*queue)->taskHandle);
    }

    if ((*queue)->queue != NULL) {
        vQueueDelete((*queue)->queue);
    }

    vPortFree(*queue);
    *queue = NULL;
}

static void tuner_playback_drain(void)
{
    for (;;) {
        void *rx_buf;
        uint32_t rx_len;
        uint16_t rx_desc_idx;

        rx_buf = acomp_tuner_stream_rx_buffer_get(TUNER_CHN_OUTPUT, &rx_len, &rx_desc_idx);
        if (rx_buf == NULL || rx_len == 0) {
            break;
        }

        if (audio_dev != NULL) {
            uint32_t samples = rx_len / sizeof(int16_t);
            int written = lisa_audio_play_write(audio_dev, rx_buf, samples);
            if (written < 0) {
                LOGW("Play write failed: %d", written);
            }
        }

        acomp_tuner_stream_rx_buffer_release(TUNER_CHN_OUTPUT, rx_desc_idx, rx_len, rx_buf);
    }
}

static void tuner_output_task(void *pvParameters)
{
    (void)pvParameters;

    while (tuner_running) {
        if (tuner_rx_sem != NULL) {
            xSemaphoreTake(tuner_rx_sem, pdMS_TO_TICKS(50));
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        tuner_playback_drain();
    }

    tuner_playback_drain();
    tuner_out_task_handle = NULL;
    vTaskDelete(NULL);
}

static void tuner_wq_handler(void *para)
{
    tuner_audio_msg_t *msg = (tuner_audio_msg_t *)para;
    uint8_t *tx_buf;
    uint32_t tx_len;
    uint16_t tx_desc_idx;

    tx_buf = acomp_tuner_stream_tx_buffer_alloc(TUNER_CHN_INPUT, &tx_len, &tx_desc_idx);
    if (tx_buf && tx_len > 0) {
        uint32_t bytes = msg->samples * sizeof(int16_t);
        uint32_t copy_len = (bytes < tx_len) ? bytes : tx_len;
        memcpy(tx_buf, msg->data, copy_len);
        acomp_tuner_stream_tx_buffer_submit(TUNER_CHN_INPUT, tx_buf, copy_len, tx_desc_idx);
        acomp_tuner_stream_kick(TUNER_CHN_INPUT);
    } else {
        LOGW("Tuner input buffer unavailable");
    }

    psram_free(msg);

    if (++frame_count % 200 == 0) {
        LOGI("Processed %u frames", frame_count);
    }
}

static void audio_record_callback(const lisa_audio_event_t *event, void *user_data)
{
    if (!event->record_buffer || event->record_samples == 0) {
        return;
    }

    tuner_audio_msg_t *msg = psram_malloc(sizeof(tuner_audio_msg_t));
    if (!msg) {
        return;
    }

    msg->data = (const int16_t *)event->record_buffer;
    msg->samples = event->record_samples;

    if (workqueue_submit(tuner_wq, tuner_wq_handler, msg) != pdPASS) {
        LOGW("Submit record frame failed");
        psram_free(msg);
    }
}

static void tuner_event_callback(uint32_t event, void *event_data,
                                 uint32_t event_data_len, void *priv)
{
    (void)priv;

    if ((event & TUNER_CB_EVENT_STATUS) && event_data != NULL &&
        event_data_len >= sizeof(uint32_t)) {
        uint32_t status = *(uint32_t *)event_data;
        LOGI("Tuner status: 0x%x", status);
    }

    if ((event & TUNER_CB_EVENT_STREAM_UPDATE) && tuner_rx_sem != NULL) {
        xSemaphoreGive(tuner_rx_sem);
    }
}

int main(int argc, char **argv)
{
    int ret;
    bool audio_callback_registered = false;
    bool tuner_callback_registered = false;
    bool input_enabled = false;
    bool output_enabled = false;
    bool tuner_inited = false;
    bool tuner_started = false;
    bool play_started = false;
    bool record_started = false;

    LOGI("=== Tuner Sample Started ===");
    LOGI("Audio pipeline: Mic -> CP -> AP(Tuner) -> CP -> Speaker");

    ic_message_init();
    LOGI("IPC initialized");

    vTaskDelay(pdMS_TO_TICKS(1000));

    GPDMA_Initialize();

    audio_dev = lisa_device_get(AUDIO_DEVICE_NAME);
    if (!audio_dev || !lisa_device_ready(audio_dev)) {
        LOGE("Audio device not ready");
        ret = -1;
        goto cleanup;
    }
    LOGI("Audio device ready");

    tuner_wq = workqueue_create("tuner_wq", 7, 16, 4096);
    if (!tuner_wq) {
        LOGE("Failed to create workqueue");
        ret = -1;
        goto cleanup;
    }
    LOGI("Workqueue created");

    tuner_rx_sem = xSemaphoreCreateBinary();
    if (tuner_rx_sem == NULL) {
        LOGE("Failed to create tuner_rx_sem");
        ret = -1;
        goto cleanup;
    }

    tuner_running = true;
    if (xTaskCreate(tuner_output_task,
                    "tuner_out_task",
                    TUNER_OUTPUT_TASK_STACK_SIZE,
                    NULL,
                    TUNER_OUTPUT_TASK_PRIORITY,
                    &tuner_out_task_handle) != pdPASS) {
        LOGE("Failed to create tuner_out_task");
        tuner_out_task_handle = NULL;
        ret = -1;
        goto cleanup;
    }

    ret = lisa_audio_register_callback(audio_dev, audio_record_callback, NULL);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Register audio callback failed: %d", ret);
        goto cleanup;
    }
    audio_callback_registered = true;

    acomp_init();
    LOGI("ACOMP initialized");

    ret = acomp_tuner_init();
    if (ret != 0) {
        LOGE("Tuner init failed: %d", ret);
        goto cleanup;
    }
    tuner_inited = true;
    LOGI("Tuner initialized");

    ret = acomp_tuner_add_callback(TUNER_CB_EVENT_STATUS | TUNER_CB_EVENT_STREAM_UPDATE,
                                   tuner_event_callback, NULL);
    if (ret != 0) {
        LOGE("Tuner add callback failed: %d", ret);
        goto cleanup;
    }
    tuner_callback_registered = true;

    acomp_stream_chn_create_desc_t input_desc = {
        .cname = "tuner.in",
        .direction = ACOMP_STREAM_DIRECTION_M2R,
        .index = TUNER_CHN_INPUT,
        .buffer_size = TUNER_STREAM_BUFFER_SIZE,
        .num_descs = TUNER_STREAM_NUM_DESCS,
        .kick_policy = 1,
    };
    ret = acomp_tuner_stream_ch_enable(TUNER_CHN_INPUT, &input_desc);
    if (ret != 0) {
        LOGE("Enable input stream failed: %d", ret);
        goto cleanup;
    }
    input_enabled = true;
    LOGI("Input stream channel (M2R) enabled");

    acomp_stream_chn_create_desc_t output_desc = {
        .cname = "tuner.out",
        .direction = ACOMP_STREAM_DIRECTION_R2M,
        .index = TUNER_CHN_OUTPUT,
        .buffer_size = TUNER_STREAM_BUFFER_SIZE,
        .num_descs = TUNER_STREAM_NUM_DESCS,
        .kick_policy = 1,
    };
    ret = acomp_tuner_stream_ch_enable(TUNER_CHN_OUTPUT, &output_desc);
    if (ret != 0) {
        LOGE("Enable output stream failed: %d", ret);
        goto cleanup;
    }
    output_enabled = true;
    LOGI("Output stream channel (R2M) enabled");

    ret = acomp_tuner_set_samplerate((float)SAMPLE_RATE);
    if (ret != 0) {
        LOGE("Set tuner samplerate failed: %d", ret);
        goto cleanup;
    }

    ret = acomp_tuner_set_volume(1.0f);
    if (ret != 0) {
        LOGE("Set tuner volume failed: %d", ret);
        goto cleanup;
    }

    ret = acomp_tuner_start();
    if (ret != 0) {
        LOGE("Tuner start failed: %d", ret);
        goto cleanup;
    }
    tuner_started = true;

    ret = acomp_tuner_enable(1);
    if (ret != 0) {
        LOGE("Tuner enable failed: %d", ret);
        goto cleanup;
    }
    LOGI("Tuner started and enabled");

    lisa_audio_play_config_t play_config = {
        .format = {
            .sample_rate = SAMPLE_RATE,
            .channels = AUDIO_CHANNELS,
            .sample_bits = AUDIO_BITS,
        },
        .gain = {
            .analog_gain = PLAY_ANALOG_GAIN,
            .digital_gain = PLAY_DIGITAL_GAIN,
        },
        .buffer_count = PLAY_BUFFER_COUNT,
        .buffer_samples = PLAY_BUFFER_SAMPLES,
    };
    ret = lisa_audio_play_config(audio_dev, &play_config);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Audio play config failed: %d", ret);
        goto cleanup;
    }

    ret = lisa_audio_play_start(audio_dev);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Audio play start failed: %d", ret);
        goto cleanup;
    }
    play_started = true;
    LOGI("Audio playback started");

    frame_count = 0;

    lisa_audio_record_config_t record_config = {
        .format = {
            .sample_rate = SAMPLE_RATE,
            .channels = AUDIO_CHANNELS,
            .sample_bits = AUDIO_BITS,
        },
        .gain = {
            .analog_gain = RECORD_ANALOG_GAIN,
            .digital_gain = RECORD_DIGITAL_GAIN,
        },
        .differential_input = true,
        .enable_hpf = true,
    };
    ret = lisa_audio_record_config(audio_dev, &record_config);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Audio record config failed: %d", ret);
        goto cleanup;
    }

    ret = lisa_audio_record_start(audio_dev);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Audio record start failed: %d", ret);
        goto cleanup;
    }
    record_started = true;
    LOGI("Audio recording started");
    LOGI("=== Audio pipeline running ===");

    vTaskDelay(pdMS_TO_TICKS(30000));
    ret = 0;

cleanup:
    if (record_started) {
        lisa_audio_record_stop(audio_dev);
    }

    if (play_started) {
        lisa_audio_play_flush(audio_dev);
        lisa_audio_play_stop(audio_dev);
    }

    if (audio_callback_registered) {
        lisa_audio_unregister_callback(audio_dev, audio_record_callback);
    }

    tuner_running = false;
    if (tuner_rx_sem != NULL) {
        xSemaphoreGive(tuner_rx_sem);
    }

    for (int i = 0; i < 20 && tuner_out_task_handle != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (tuner_out_task_handle != NULL) {
        vTaskDelete(tuner_out_task_handle);
        tuner_out_task_handle = NULL;
    }

    if (tuner_started) {
        acomp_tuner_enable(0);
        acomp_tuner_stop();
    }

    if (output_enabled) {
        acomp_tuner_stream_ch_disable(TUNER_CHN_OUTPUT);
    }

    if (input_enabled) {
        acomp_tuner_stream_ch_disable(TUNER_CHN_INPUT);
    }

    if (tuner_callback_registered) {
        acomp_tuner_remove_callback(tuner_event_callback);
    }

    if (tuner_inited) {
        acomp_tuner_cleanup();
    }

    if (tuner_rx_sem != NULL) {
        vSemaphoreDelete(tuner_rx_sem);
        tuner_rx_sem = NULL;
    }

    tuner_workqueue_destroy(&tuner_wq);

    if (ret == 0) {
        LOGI("=== Tuner Sample Completed, total frames=%u ===", frame_count);
    } else {
        LOGE("Tuner sample exit with error: %d", ret);
    }

    return ret;
}
