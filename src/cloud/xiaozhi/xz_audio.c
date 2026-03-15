/**
 * @file xz_audio.c
 * @brief 小智云端音频发送器实现
 * @note 使用独立线程处理 Opus 编码，避免占用调用者堆栈
 */

#define TAG "xz_audio"

#include "xz_audio.h"
#include "xz_opus.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_thread.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include <string.h>

/** 最大缓冲区大小 */
#define MAX_PCM_BUFFER_SIZE  1920   /* 960 采样 * 2 字节 */
#define MAX_OPUS_PACKET_SIZE 4000
#define AUDIO_QUEUE_SIZE     32     /* 音频队列深度 (增加到 32，约 512ms 缓冲) */
#define AUDIO_WORKER_STACK_SIZE 32768  /* 工作线程堆栈: 32KB */

/** 音频数据消息 */
typedef struct {
    int16_t pcm_data[MAX_PCM_BUFFER_SIZE];
    int samples;
    bool is_flush;  /* 是否为 flush 消息 */
} audio_msg_t;

/** 音频发送器状态 */
typedef enum {
    AUDIO_STATE_IDLE = 0,     /**< 空闲状态 */
    AUDIO_STATE_SENDING,      /**< 发送中 */
    AUDIO_STATE_STOPPING,     /**< 停止中 */
} audio_state_e;

/** 音频发送器结构 */
struct xz_audio_s {
    xz_opus_t opus;
    audio_state_e state;

    int sample_rate;
    int channels;
    int frame_size;           /* 帧大小 (采样数) */
    int bitrate;

    /* PCM 缓冲区 */
    int16_t pcm_buffer[MAX_PCM_BUFFER_SIZE];
    int pcm_buffer_len;       /* 当前缓冲区中的采样数 */

    /* Opus 输出缓冲区 (堆分配，节省栈空间) */
    uint8_t *opus_buffer;
    int opus_buffer_size;

    /* 线程和队列 */
    lisa_thread_t *worker_thread;
    QueueHandle_t audio_queue;
    SemaphoreHandle_t mutex;

    /* 发送回调 */
    xz_audio_send_cb_t send_cb;
    void *send_cb_user_data;

    volatile bool worker_running;
};

/** 工作线程 */
static void audio_worker_task(void *arg);

xz_audio_t xz_audio_create(const xz_audio_config_t *config)
{
    if (!config) {
        LISA_LOGE(TAG, "Invalid config");
        return NULL;
    }

    /* 验证配置 */
    if (config->sample_rate != 16000) {
        LISA_LOGE(TAG, "Only 16kHz sample rate is supported");
        return NULL;
    }

    if (config->channels != 1) {
        LISA_LOGE(TAG, "Only mono (1 channel) is supported");
        return NULL;
    }

    /* 计算帧大小 */
    int frame_duration_ms = config->frame_duration_ms > 0 ? config->frame_duration_ms : 60;
    int frame_size = (config->sample_rate * frame_duration_ms) / 1000;  /* 60ms @ 16kHz = 960 */
    int bitrate = config->bitrate > 0 ? config->bitrate : 24000;

    /* 创建 Opus 编码器 */
    xz_opus_config_t opus_config = {
        .sample_rate = config->sample_rate,
        .channels = config->channels,
        .frame_size = frame_size,
        .bitrate = bitrate,
        .complexity = 0,       /* 最低 CPU 占用 */
    };

    xz_opus_t opus = xz_opus_create(&opus_config);
    if (!opus) {
        LISA_LOGE(TAG, "Failed to create Opus encoder");
        return NULL;
    }

    /* 分配结构 */
    xz_audio_t audio = (xz_audio_t)lisa_mem_calloc(1, sizeof(struct xz_audio_s));
    if (!audio) {
        LISA_LOGE(TAG, "Failed to allocate memory");
        xz_opus_destroy(opus);
        return NULL;
    }

    audio->opus = opus;
    audio->state = AUDIO_STATE_IDLE;
    audio->sample_rate = config->sample_rate;
    audio->channels = config->channels;
    audio->frame_size = frame_size;
    audio->bitrate = bitrate;
    audio->pcm_buffer_len = 0;
    audio->worker_running = false;

    /* 分配 Opus 输出缓冲区 (堆分配，节省栈空间) */
    audio->opus_buffer_size = MAX_OPUS_PACKET_SIZE;
    audio->opus_buffer = (uint8_t *)lisa_mem_alloc(audio->opus_buffer_size);
    if (!audio->opus_buffer) {
        LISA_LOGE(TAG, "Failed to allocate Opus buffer");
        xz_opus_destroy(opus);
        lisa_mem_free(audio);
        return NULL;
    }

    /* 创建互斥锁 */
    audio->mutex = xSemaphoreCreateMutex();
    if (!audio->mutex) {
        LISA_LOGE(TAG, "Failed to create mutex");
        xz_opus_destroy(opus);
        lisa_mem_free(audio);
        return NULL;
    }

    /* 创建音频队列 */
    audio->audio_queue = xQueueCreate(AUDIO_QUEUE_SIZE, sizeof(audio_msg_t));
    if (!audio->audio_queue) {
        LISA_LOGE(TAG, "Failed to create queue");
        vSemaphoreDelete(audio->mutex);
        xz_opus_destroy(opus);
        lisa_mem_free(audio);
        return NULL;
    }

    LISA_LOGI(TAG, "Audio sender created: %dHz, %dch, %dms frame",
              audio->sample_rate, audio->channels, frame_duration_ms);

    return audio;
}

void xz_audio_destroy(xz_audio_t audio)
{
    if (!audio) {
        return;
    }

    if (audio->worker_running) {
        xz_audio_stop(audio);
    }

    if (audio->opus) {
        xz_opus_destroy(audio->opus);
    }

    if (audio->opus_buffer) {
        lisa_mem_free(audio->opus_buffer);
    }

    if (audio->audio_queue) {
        vQueueDelete(audio->audio_queue);
    }

    if (audio->mutex) {
        vSemaphoreDelete(audio->mutex);
    }

    lisa_mem_free(audio);
    LISA_LOGI(TAG, "Audio sender destroyed");
}

int xz_audio_start(xz_audio_t audio)
{
    if (!audio) {
        return -1;
    }

    xSemaphoreTake(audio->mutex, portMAX_DELAY);

    if (audio->state == AUDIO_STATE_SENDING) {
        xSemaphoreGive(audio->mutex);
        LISA_LOGW(TAG, "Audio already sending");
        return 0;
    }

    /* 创建工作线程 */
    if (!audio->worker_thread) {
        audio->worker_running = true;

        lisa_thread_attr_t attr = {
            .name = "xz_audio",
            .stack_size = AUDIO_WORKER_STACK_SIZE,
            .priority = LISA_OS_PRIORITY_ABOVE_NORMAL,  /* 高于普通优先级，确保实时性 */
        };

        audio->worker_thread = lisa_thread_create(&attr, audio_worker_task, audio);
        if (!audio->worker_thread) {
            audio->worker_running = false;
            xSemaphoreGive(audio->mutex);
            LISA_LOGE(TAG, "Failed to create worker thread");
            return -1;
        }

        LISA_LOGI(TAG, "Worker thread created (stack: %d bytes)", AUDIO_WORKER_STACK_SIZE);
    }

    /* 清空队列，确保没有上次残留的数据 */
    xQueueReset(audio->audio_queue);
    LISA_LOGI(TAG, "Queue cleared before starting");

    audio->state = AUDIO_STATE_SENDING;
    audio->pcm_buffer_len = 0;
    xz_opus_reset(audio->opus);

    xSemaphoreGive(audio->mutex);

    LISA_LOGI(TAG, "Audio sending started");
    return 0;
}

int xz_audio_stop(xz_audio_t audio)
{
    if (!audio) {
        return -1;
    }

    xSemaphoreTake(audio->mutex, portMAX_DELAY);

    if (audio->state != AUDIO_STATE_SENDING) {
        xSemaphoreGive(audio->mutex);
        return 0;
    }

    audio->state = AUDIO_STATE_STOPPING;

    /* 发送 flush 消息到队列前端（优先处理）*/
    audio_msg_t flush_msg = {
        .is_flush = true,
        .samples = 0,
    };

    if (xQueueSendToFront(audio->audio_queue, &flush_msg, pdMS_TO_TICKS(500)) != pdTRUE) {
        LISA_LOGW(TAG, "Failed to send flush message");
        /* 如果队列满，直接清空队列并设置停止状态 */
        xQueueReset(audio->audio_queue);
        audio->worker_running = false;
    }

    xSemaphoreGive(audio->mutex);

    /* 等待工作线程结束 */
    int wait_count = 0;
    while (audio->worker_running && wait_count < 50) {  /* 最多等待 500ms */
        vTaskDelay(pdMS_TO_TICKS(10));
        wait_count++;
    }

    if (audio->worker_running) {
        LISA_LOGW(TAG, "Worker thread still running after timeout");
    }

    /* 清空线程句柄，确保下次 start 能创建新线程 */
    if (audio->worker_thread) {
        audio->worker_thread = NULL;
        LISA_LOGI(TAG, "Worker thread handle cleared");
    }

    audio->state = AUDIO_STATE_IDLE;
    LISA_LOGI(TAG, "Audio sending stopped");

    return 0;
}

bool xz_audio_is_sending(xz_audio_t audio)
{
    if (!audio) {
        return false;
    }

    xSemaphoreTake(audio->mutex, portMAX_DELAY);
    bool sending = (audio->state == AUDIO_STATE_SENDING);
    xSemaphoreGive(audio->mutex);

    return sending;
}

int xz_audio_write(xz_audio_t audio, const int16_t *pcm_data, int samples)
{
    if (!audio || !pcm_data || samples <= 0) {
        return -1;
    }

    if (!xz_audio_is_sending(audio)) {
        /* 静默丢弃数据 */
        return 0;
    }

    /* 分块发送，避免单次消息过大 */
    int processed = 0;
    while (processed < samples) {
        int chunk_size = (samples - processed < MAX_PCM_BUFFER_SIZE)
                        ? (samples - processed)
                        : MAX_PCM_BUFFER_SIZE;

        audio_msg_t msg = {
            .is_flush = false,
            .samples = chunk_size,
        };

        memcpy(msg.pcm_data, pcm_data + processed, chunk_size * sizeof(int16_t));

        if (xQueueSend(audio->audio_queue, &msg, pdMS_TO_TICKS(10)) != pdTRUE) {
            /* 队列满，丢弃数据但继续 */
            static uint32_t last_drop_log = 0;
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - last_drop_log > 1000) {  /* 每秒最多打印一次 */
                LISA_LOGW(TAG, "Audio queue full, dropping %d samples", chunk_size);
                last_drop_log = now;
            }
            break;
        }

        processed += chunk_size;
    }

    return 0;
}

void xz_audio_set_send_callback(xz_audio_t audio, xz_audio_send_cb_t send_cb, void *user_data)
{
    if (!audio) {
        return;
    }

    xSemaphoreTake(audio->mutex, portMAX_DELAY);
    audio->send_cb = send_cb;
    audio->send_cb_user_data = user_data;
    xSemaphoreGive(audio->mutex);
}

/** 处理音频帧编码和发送 */
static void process_audio_frame(xz_audio_t audio, const int16_t *pcm_data, int samples)
{
    /* 写入缓冲区 */
    int written = 0;
    while (written < samples) {
        int available = audio->frame_size - audio->pcm_buffer_len;
        int to_write = samples - written;
        if (to_write > available) {
            to_write = available;
        }

        memcpy(audio->pcm_buffer + audio->pcm_buffer_len,
               pcm_data + written,
               to_write * sizeof(int16_t));
        audio->pcm_buffer_len += to_write;
        written += to_write;

        /* 检查是否达到完整帧 */
        if (audio->pcm_buffer_len >= audio->frame_size) {
            /* 编码 (使用堆缓冲区，节省栈空间) */
            int opus_size = audio->opus_buffer_size;

            if (xz_opus_encode(audio->opus, audio->pcm_buffer, audio->frame_size,
                              audio->opus_buffer, &opus_size) == 0) {
                /* 发送 */
                if (audio->send_cb) {
                    int ret = audio->send_cb(audio->opus_buffer, opus_size, audio->send_cb_user_data);
                    if (ret != 0) {
                        LISA_LOGE(TAG, "Send callback failed: %d", ret);
                    }
                }
            } else {
                LISA_LOGE(TAG, "Opus encode failed");
            }

            audio->pcm_buffer_len = 0;
        }
    }
}

/** 工作线程主函数 */
static void audio_worker_task(void *arg)
{
    xz_audio_t audio = (xz_audio_t)arg;
    if (!audio) {
        return;
    }

    LISA_LOGI(TAG, "Worker thread started");

    while (audio->worker_running) {
        audio_msg_t msg;

        /* 接收消息 (超时 50ms，更快响应停止请求) */
        if (xQueueReceive(audio->audio_queue, &msg, pdMS_TO_TICKS(50)) != pdTRUE) {
            /* 超时，检查是否需要退出 */
            if (audio->state == AUDIO_STATE_STOPPING) {
                LISA_LOGD(TAG, "Stopping due to STOPPING state");
                break;
            }
            continue;
        }

        /* 处理 flush 消息 */
        if (msg.is_flush) {
            LISA_LOGD(TAG, "Processing flush message");

            /* 编码并发送剩余数据 */
            if (audio->pcm_buffer_len > 0) {
                /* 填充零到完整帧 */
                while (audio->pcm_buffer_len < audio->frame_size) {
                    audio->pcm_buffer[audio->pcm_buffer_len++] = 0;
                }

                /* 编码并发送 (使用堆缓冲区) */
                int opus_size = audio->opus_buffer_size;

                if (xz_opus_encode(audio->opus, audio->pcm_buffer, audio->frame_size,
                                  audio->opus_buffer, &opus_size) == 0) {
                    if (audio->send_cb) {
                        audio->send_cb(audio->opus_buffer, opus_size, audio->send_cb_user_data);
                    }
                }
            }

            audio->pcm_buffer_len = 0;
            break;  /* 退出循环 */
        }

        /* 处理音频数据 */
        process_audio_frame(audio, msg.pcm_data, msg.samples);

        /* 每处理一条消息后检查停止状态 */
        if (audio->state == AUDIO_STATE_STOPPING) {
            LISA_LOGD(TAG, "Stopping after processing current message");
            break;
        }
    }

    audio->worker_running = false;
    LISA_LOGI(TAG, "Worker thread exiting");
}
