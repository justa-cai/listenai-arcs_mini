/**
 * @file xz_tts_decoder.c
 * @brief 小智云端 TTS 解码器实现
 * @note 使用独立线程处理 Opus 解码，避免占用 WebSocket 线程堆栈
 */

#define TAG "xz_tts_decoder"

#include "xz_tts_decoder.h"
#include "xz_opus_dec.h"
#include "xz_tts_player.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_thread.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include <string.h>

/** 最大缓冲区大小 */
#define MAX_OPUS_PACKET_SIZE 4000
#define MAX_PCM_SAMPLES     5760   /* 24kHz * 60ms * 2ch */
#define TTS_QUEUE_SIZE       16      /* Opus 数据队列深度 */
#define TTS_WORKER_STACK_SIZE 32768  /* 工作线程堆栈: 32KB */

/** Opus 数据消息 */
typedef struct {
    uint8_t opus_data[MAX_OPUS_PACKET_SIZE];
    uint32_t len;
    bool is_flush;  /* 是否为 flush 消息 */
} tts_opus_msg_t;

/** TTS 解码器状态 */
typedef enum {
    TTS_DECODER_STATE_IDLE = 0,
    TTS_DECODER_STATE_DECODING,
    TTS_DECODER_STATE_STOPPING,
} tts_decoder_state_e;

/** TTS 解码器结构 */
struct xz_tts_decoder_s {
    xz_opus_dec_t decoder;
    struct xz_tts_player_s *player;

    int sample_rate;
    int channels;
    int frame_size;

    /* 线程和队列 */
    lisa_thread_t *worker_thread;
    QueueHandle_t opus_queue;
    SemaphoreHandle_t mutex;

    volatile bool worker_running;
    tts_decoder_state_e state;
};

/** 工作线程 */
static void tts_decoder_worker_task(void *arg);

xz_tts_decoder_t xz_tts_decoder_create(const xz_tts_decoder_config_t *config,
                                      struct xz_tts_player_s *player)
{
    if (!config || !player) {
        LISA_LOGE(TAG, "Invalid config or player");
        return NULL;
    }

    /* 验证配置 */
    if (config->sample_rate != 16000 && config->sample_rate != 24000) {
        LISA_LOGE(TAG, "Only 16kHz or 24kHz sample rate is supported");
        return NULL;
    }

    if (config->channels != 1) {
        LISA_LOGE(TAG, "Only mono (1 channel) is supported");
        return NULL;
    }

    /* 创建 Opus 解码器 */
    xz_opus_dec_config_t dec_config = {
        .sample_rate = config->sample_rate,
        .channels = config->channels,
        .frame_size = (config->sample_rate * 60) / 1000,  /* 60ms */
    };

    xz_opus_dec_t decoder = xz_opus_dec_create(&dec_config);
    if (!decoder) {
        LISA_LOGE(TAG, "Failed to create Opus decoder");
        return NULL;
    }

    /* 分配结构 */
    xz_tts_decoder_t tts_dec = (xz_tts_decoder_t)lisa_mem_calloc(1, sizeof(struct xz_tts_decoder_s));
    if (!tts_dec) {
        LISA_LOGE(TAG, "Failed to allocate memory");
        xz_opus_dec_destroy(decoder);
        return NULL;
    }

    tts_dec->decoder = decoder;
    tts_dec->player = player;
    tts_dec->sample_rate = config->sample_rate;
    tts_dec->channels = config->channels;
    tts_dec->frame_size = dec_config.frame_size;
    tts_dec->state = TTS_DECODER_STATE_IDLE;
    tts_dec->worker_running = false;

    /* 创建互斥锁 */
    tts_dec->mutex = xSemaphoreCreateMutex();
    if (!tts_dec->mutex) {
        LISA_LOGE(TAG, "Failed to create mutex");
        xz_opus_dec_destroy(decoder);
        lisa_mem_free(tts_dec);
        return NULL;
    }

    /* 创建 Opus 数据队列 */
    tts_dec->opus_queue = xQueueCreate(TTS_QUEUE_SIZE, sizeof(tts_opus_msg_t));
    if (!tts_dec->opus_queue) {
        LISA_LOGE(TAG, "Failed to create queue");
        vSemaphoreDelete(tts_dec->mutex);
        xz_opus_dec_destroy(decoder);
        lisa_mem_free(tts_dec);
        return NULL;
    }

    LISA_LOGI(TAG, "TTS decoder created: %dHz, %dch, %d samples/frame",
              tts_dec->sample_rate, tts_dec->channels, tts_dec->frame_size);

    return tts_dec;
}

void xz_tts_decoder_destroy(xz_tts_decoder_t decoder)
{
    if (!decoder) {
        return;
    }

    if (decoder->worker_running) {
        xz_tts_decoder_stop(decoder);
    }

    if (decoder->decoder) {
        xz_opus_dec_destroy(decoder->decoder);
    }

    if (decoder->opus_queue) {
        vQueueDelete(decoder->opus_queue);
    }

    if (decoder->mutex) {
        vSemaphoreDelete(decoder->mutex);
    }

    lisa_mem_free(decoder);
    LISA_LOGI(TAG, "TTS decoder destroyed");
}

int xz_tts_decoder_start(xz_tts_decoder_t decoder)
{
    if (!decoder) {
        return -1;
    }

    xSemaphoreTake(decoder->mutex, portMAX_DELAY);

    if (decoder->state == TTS_DECODER_STATE_DECODING) {
        xSemaphoreGive(decoder->mutex);
        LISA_LOGW(TAG, "TTS decoder already decoding");
        return 0;
    }

    /* 创建工作线程 */
    if (!decoder->worker_thread) {
        decoder->worker_running = true;

        lisa_thread_attr_t attr = {
            .name = "xz_tts_dec",
            .stack_size = TTS_WORKER_STACK_SIZE,
            .priority = LISA_OS_PRIORITY_ABOVE_NORMAL,  /* 高优先级，确保音频流畅 */
        };

        decoder->worker_thread = lisa_thread_create(&attr, tts_decoder_worker_task, decoder);
        if (!decoder->worker_thread) {
            decoder->worker_running = false;
            xSemaphoreGive(decoder->mutex);
            LISA_LOGE(TAG, "Failed to create worker thread");
            return -1;
        }

        LISA_LOGI(TAG, "TTS decoder worker thread created (stack: %d bytes)", TTS_WORKER_STACK_SIZE);
    }

    decoder->state = TTS_DECODER_STATE_DECODING;
    xSemaphoreGive(decoder->mutex);

    LISA_LOGI(TAG, "TTS decoder started");
    return 0;
}

int xz_tts_decoder_stop(xz_tts_decoder_t decoder)
{
    if (!decoder) {
        return -1;
    }

    xSemaphoreTake(decoder->mutex, portMAX_DELAY);

    if (decoder->state != TTS_DECODER_STATE_DECODING) {
        xSemaphoreGive(decoder->mutex);
        return 0;
    }

    decoder->state = TTS_DECODER_STATE_STOPPING;

    /* 发送 flush 消息 */
    tts_opus_msg_t flush_msg = {
        .is_flush = true,
        .len = 0,
    };

    if (xQueueSend(decoder->opus_queue, &flush_msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        LISA_LOGW(TAG, "Failed to send flush message");
    }

    xSemaphoreGive(decoder->mutex);

    /* 等待工作线程结束 */
    int wait_count = 0;
    while (decoder->worker_running && wait_count < 50) {  /* 最多等待 500ms */
        vTaskDelay(pdMS_TO_TICKS(10));
        wait_count++;
    }

    if (decoder->worker_running) {
        LISA_LOGW(TAG, "Worker thread still running after timeout");
    }

    decoder->state = TTS_DECODER_STATE_IDLE;
    LISA_LOGI(TAG, "TTS decoder stopped");

    return 0;
}

bool xz_tts_decoder_is_decoding(xz_tts_decoder_t decoder)
{
    if (!decoder) {
        return false;
    }

    xSemaphoreTake(decoder->mutex, portMAX_DELAY);
    bool decoding = (decoder->state == TTS_DECODER_STATE_DECODING);
    xSemaphoreGive(decoder->mutex);

    return decoding;
}

int xz_tts_decoder_write(xz_tts_decoder_t decoder, const uint8_t *opus_data, uint32_t len)
{
    if (!decoder || !opus_data || len == 0) {
        return -1;
    }

    if (!xz_tts_decoder_is_decoding(decoder)) {
        /* 静默丢弃数据 */
        return 0;
    }

    if (len > MAX_OPUS_PACKET_SIZE) {
        LISA_LOGW(TAG, "Opus packet too large: %u, truncating to %d", len, MAX_OPUS_PACKET_SIZE);
        len = MAX_OPUS_PACKET_SIZE;
    }

    tts_opus_msg_t msg = {
        .is_flush = false,
        .len = len,
    };

    memcpy(msg.opus_data, opus_data, len);

    if (xQueueSend(decoder->opus_queue, &msg, pdMS_TO_TICKS(10)) != pdTRUE) {
        /* 队列满，丢弃数据 */
        LISA_LOGW(TAG, "TTS decoder queue full, dropping %u bytes", len);
        return -1;
    }

    return 0;
}

int xz_tts_decoder_end_stream(xz_tts_decoder_t decoder)
{
    if (!decoder) {
        return -1;
    }

    LISA_LOGD(TAG, "TTS decoder end stream");

    /* 停止解码 */
    return xz_tts_decoder_stop(decoder);
}

/** 处理 Opus 数据解码和播放 */
static void process_opus_frame(xz_tts_decoder_t decoder, const uint8_t *opus_data, uint32_t len)
{
    /* 在独立线程中，可以使用栈缓冲区进行解码 */
    int16_t pcm_buffer[MAX_PCM_SAMPLES];
    int out_len = MAX_PCM_SAMPLES;

    xz_opus_dec_result_e result = xz_opus_dec_decode(decoder->decoder,
                                                      opus_data,
                                                      len,
                                                      pcm_buffer,
                                                      &out_len);
    if (result == XZ_OPUS_DECODE_OK) {
        /* 将解码后的 PCM 数据送入播放器 */
        xz_tts_player_write(decoder->player, pcm_buffer, out_len);
    } else {
        LISA_LOGE(TAG, "TTS decode failed: %d", result);
    }
}

/** 工作线程主函数 */
static void tts_decoder_worker_task(void *arg)
{
    xz_tts_decoder_t decoder = (xz_tts_decoder_t)arg;
    if (!decoder) {
        return;
    }

    LISA_LOGI(TAG, "TTS decoder worker thread started");

    static int decode_count = 0;

    while (decoder->worker_running) {
        tts_opus_msg_t msg;

        /* 接收消息 (超时 100ms) */
        if (xQueueReceive(decoder->opus_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
            /* 超时，检查是否需要退出 */
            if (decoder->state == TTS_DECODER_STATE_STOPPING) {
                break;
            }
            continue;
        }

        /* 处理 flush 消息 */
        if (msg.is_flush) {
            LISA_LOGD(TAG, "Processing flush message");
            break;  /* 退出循环 */
        }

        /* 处理 Opus 数据 */
        process_opus_frame(decoder, msg.opus_data, msg.len);

        /* 调试输出 */
        if (decode_count++ < 5) {
            LISA_LOGI(TAG, "TTS decoded: %u bytes -> %d samples", msg.len,
                     decoder->frame_size * decoder->channels);
        }
    }

    /* 重置解码器 */
    xz_opus_dec_reset(decoder->decoder);

    decoder->worker_running = false;
    LISA_LOGI(TAG, "TTS decoder worker thread exiting");
}
