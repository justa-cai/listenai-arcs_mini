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
#define TTS_QUEUE_SIZE       128    /* Opus 数据队列深度 (增加到 128，约 7680ms 缓冲) */
#define TTS_WORKER_STACK_SIZE 32768  /* 工作线程堆栈: 32KB */

/** 动态缓冲配置 */
#define FRAME_INTERVAL_HISTORY_SIZE 8   /* 跟踪最近 8 帧的间隔 */
#define BYTES_PER_FRAME_24KHZ  2880     /* 24kHz 单声道 16bit: 60ms = 2880 字节 */
#define BYTES_PER_FRAME_16KHZ  1920     /* 16kHz 单声道 16bit: 60ms = 1920 字节 */
#define MIN_BUFFER_FRAMES     2         /* 最小缓冲帧数 */
#define MAX_BUFFER_FRAMES     12        /* 最大缓冲帧数 */
#define SAFETY_MARGIN_MULTIPLIER 1.5    /* 安全余量倍数（平均间隔 * 1.5） */

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

    /* 性能测量：TTS start 时间戳 */
    uint32_t start_time_ms;

    /* 帧计数器：用于实现"等待第二帧到达再播放"的优化策略
     * 第一帧解码但缓存，第二帧到达时才开始写入播放器
     * 这样利用第二帧的网络延迟作为自然缓冲，避免固定延迟
     */
    int frame_count;
    int16_t first_frame_buffer[MAX_PCM_SAMPLES];  /* 第一帧 PCM 缓存 */
    int first_frame_samples;  /* 第一帧采样点数 */

    /* 动态缓冲：跟踪帧间隔以自适应调整缓冲深度 */
    uint32_t last_frame_time_ms;                   /* 上一帧到达时间 */
    uint32_t frame_intervals[FRAME_INTERVAL_HISTORY_SIZE];  /* 帧间隔历史 */
    int interval_index;                            /* 当前写入位置 */
    int interval_count;                            /* 已记录的间隔数量 */
    bool dynamic_buffer_enabled;                   /* 是否启用动态缓冲 */
    int bytes_per_frame;                           /* 每帧字节数 */
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

    /* 初始化动态缓冲 */
    tts_dec->dynamic_buffer_enabled = true;
    tts_dec->bytes_per_frame = (config->sample_rate * 60 * 2) / 1000;  /* 60ms, 16bit = sample_rate * 0.06 * 2 */
    tts_dec->last_frame_time_ms = 0;
    tts_dec->interval_index = 0;
    tts_dec->interval_count = 0;
    memset(tts_dec->frame_intervals, 0, sizeof(tts_dec->frame_intervals));

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

    /* 记录 TTS start 时间，用于性能测量 */
    decoder->start_time_ms = lisa_os_get_tick_ms();

    /* 重置帧计数器和第一帧缓存 */
    decoder->frame_count = 0;
    decoder->first_frame_samples = 0;

    /* 重置动态缓冲状态 */
    decoder->last_frame_time_ms = 0;
    decoder->interval_index = 0;
    decoder->interval_count = 0;
    memset(decoder->frame_intervals, 0, sizeof(decoder->frame_intervals));

    /* 清空队列，确保没有上次残留的数据 */
    xQueueReset(decoder->opus_queue);

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

    /* 清空 worker_thread 指针，以便下次 start 时重新创建 */
    decoder->worker_thread = NULL;

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

/**
 * @brief 更新帧间隔历史并计算动态缓冲阈值
 * @param decoder 解码器实例
 * @param frame_interval_ms 当前帧间隔（毫秒）
 * @return 推荐的缓冲字节数
 *
 * 算法：
 * 1. 记录最近 N 帧的间隔
 * 2. 计算平均间隔和最大间隔
 * 3. 动态缓冲 = 平均间隔 * 1.5（安全余量）
 * 4. 限制在 MIN_BUFFER_FRAMES 到 MAX_BUFFER_FRAMES 之间
 */
static uint32_t update_dynamic_buffer(xz_tts_decoder_t decoder, uint32_t frame_interval_ms)
{
    if (!decoder->dynamic_buffer_enabled) {
        /* 禁用动态缓冲，返回默认 3 帧 */
        return 3 * decoder->bytes_per_frame;
    }

    /* 记录当前帧间隔 */
    decoder->frame_intervals[decoder->interval_index] = frame_interval_ms;
    decoder->interval_index = (decoder->interval_index + 1) % FRAME_INTERVAL_HISTORY_SIZE;
    if (decoder->interval_count < FRAME_INTERVAL_HISTORY_SIZE) {
        decoder->interval_count++;
    }

    /* 需要至少 3 帧历史才开始计算 */
    if (decoder->interval_count < 3) {
        return 3 * decoder->bytes_per_frame;  /* 初始使用 3 帧 */
    }

    /* 计算平均帧间隔 */
    uint32_t sum = 0;
    uint32_t max_interval = 0;
    for (int i = 0; i < decoder->interval_count; i++) {
        sum += decoder->frame_intervals[i];
        if (decoder->frame_intervals[i] > max_interval) {
            max_interval = decoder->frame_intervals[i];
        }
    }
    uint32_t avg_interval = sum / decoder->interval_count;

    /* 计算动态缓冲帧数：平均间隔 / 60ms * 安全余量 */
    float buffer_frames_f = (avg_interval / 60.0f) * SAFETY_MARGIN_MULTIPLIER;
    int buffer_frames = (int)buffer_frames_f;

    /* 限制在合理范围内 */
    if (buffer_frames < MIN_BUFFER_FRAMES) {
        buffer_frames = MIN_BUFFER_FRAMES;
    } else if (buffer_frames > MAX_BUFFER_FRAMES) {
        buffer_frames = MAX_BUFFER_FRAMES;
    }

    /* 打印动态缓冲信息（仅在变化时） */
    static int last_buffer_frames = -1;
    if (buffer_frames != last_buffer_frames) {
        LISA_LOGI(TAG, "Dynamic buffer: %d frames (avg_interval=%u ms, max=%u ms)",
                 buffer_frames, avg_interval, max_interval);
        last_buffer_frames = buffer_frames;
    }

    return buffer_frames * decoder->bytes_per_frame;
}

/** 工作线程主函数 */
static void tts_decoder_worker_task(void *arg)
{
    xz_tts_decoder_t decoder = (xz_tts_decoder_t)arg;
    if (!decoder) {
        return;
    }

    LISA_LOGI(TAG, "TTS decoder worker thread started");

    while (decoder->worker_running) {
        tts_opus_msg_t msg;

        /* 接收消息 (超时 20ms，加快处理速度防止队列溢出) */
        if (xQueueReceive(decoder->opus_queue, &msg, pdMS_TO_TICKS(20)) != pdTRUE) {
            /* 超时，检查是否需要退出 */
            if (decoder->state == TTS_DECODER_STATE_STOPPING) {
                break;
            }
            continue;
        }

        /* 处理 flush 消息 */
        if (msg.is_flush) {
            LISA_LOGD(TAG, "Processing flush message");

            /* 检查是否有第一帧缓存但未写入（TTS 只有 1 帧的情况）
             * 如果有，需要强制写入播放器，否则用户听不到声音
             */
            if (decoder->frame_count == 1 && decoder->first_frame_samples > 0) {
                LISA_LOGI(TAG, "TTS end with only 1 frame, writing cached frame to player");
                xz_tts_player_write(decoder->player, decoder->first_frame_buffer,
                                   decoder->first_frame_samples);
                decoder->first_frame_samples = 0;  /* 清除缓存 */
            }

            break;  /* 退出循环 */
        }

        /* 增加帧计数 */
        decoder->frame_count++;

        /* 策略：第一帧解码但缓存，第二帧到达时才开始写入播放器
         * 这样利用第二帧的网络延迟作为自然缓冲，避免固定延迟
         */
        if (decoder->frame_count == 1) {
            /* 第一帧：解码并缓存 */
            uint32_t frame1_start = lisa_os_get_tick_ms();
            uint32_t frame1_latency = frame1_start - decoder->start_time_ms;

            /* 记录第一帧到达时间（用于计算第二帧间隔） */
            decoder->last_frame_time_ms = frame1_start;

            int16_t pcm_buffer[MAX_PCM_SAMPLES];
            int out_len = MAX_PCM_SAMPLES;

            uint32_t decode_start = lisa_os_get_tick_ms();
            xz_opus_dec_result_e result = xz_opus_dec_decode(decoder->decoder,
                                                              msg.opus_data,
                                                              msg.len,
                                                              pcm_buffer,
                                                              &out_len);
            uint32_t decode_end = lisa_os_get_tick_ms();
            uint32_t decode_time = decode_end - decode_start;

            if (result == XZ_OPUS_DECODE_OK) {
                /* 缓存第一帧 */
                memcpy(decoder->first_frame_buffer, pcm_buffer, out_len * sizeof(int16_t));
                decoder->first_frame_samples = out_len;

                LISA_LOGI(TAG, "Frame 1: latency %u ms, decode %u ms, %d samples (cached, waiting for frame 2)",
                         frame1_latency, decode_time, out_len);
            } else {
                LISA_LOGE(TAG, "Frame 1: Decode failed: %d", result);
            }
        } else if (decoder->frame_count == 2) {
            /* 第二帧：先写入第一帧缓存，再解码并写入第二帧 */
            uint32_t frame2_start = lisa_os_get_tick_ms();
            uint32_t frame2_latency = frame2_start - decoder->start_time_ms;

            /* 计算第一帧到第二帧的间隔 */
            uint32_t frame_interval = frame2_start - decoder->last_frame_time_ms;

            /* 根据帧间隔计算并更新动态缓冲阈值 */
            uint32_t dynamic_threshold = update_dynamic_buffer(decoder, frame_interval);
            xz_tts_player_set_buffer_threshold(decoder->player, dynamic_threshold);

            /* 更新上一帧时间 */
            decoder->last_frame_time_ms = frame2_start;

            /* 写入第一帧缓存 */
            if (decoder->first_frame_samples > 0) {
                uint32_t write1_start = lisa_os_get_tick_ms();
                xz_tts_player_write(decoder->player, decoder->first_frame_buffer,
                                   decoder->first_frame_samples);
                uint32_t write1_end = lisa_os_get_tick_ms();
                uint32_t write1_latency = write1_end - decoder->start_time_ms;
                LISA_LOGI(TAG, "Frame 1: written to player at %u ms", write1_latency);
            }

            /* 解码并写入第二帧 */
            int16_t pcm_buffer[MAX_PCM_SAMPLES];
            int out_len = MAX_PCM_SAMPLES;

            uint32_t decode_start = lisa_os_get_tick_ms();
            xz_opus_dec_result_e result = xz_opus_dec_decode(decoder->decoder,
                                                              msg.opus_data,
                                                              msg.len,
                                                              pcm_buffer,
                                                              &out_len);
            uint32_t decode_end = lisa_os_get_tick_ms();
            uint32_t decode_time = decode_end - decode_start;

            if (result == XZ_OPUS_DECODE_OK) {
                uint32_t write2_start = lisa_os_get_tick_ms();
                xz_tts_player_write(decoder->player, pcm_buffer, out_len);
                uint32_t write2_end = lisa_os_get_tick_ms();
                uint32_t write2_latency = write2_end - decoder->start_time_ms;
                LISA_LOGI(TAG, "Frame 2: latency %u ms, decode %u ms, written at %u ms, %d samples (playback started)",
                         frame2_latency, decode_time, write2_latency, out_len);
            } else {
                LISA_LOGE(TAG, "Frame 2: Decode failed: %d", result);
            }
        } else {
            /* 第三帧及后续：正常解码并写入，并打印时间戳 */
            uint32_t frame_start = lisa_os_get_tick_ms();
            uint32_t frame_latency = frame_start - decoder->start_time_ms;

            /* 计算帧间隔并更新动态缓冲 */
            uint32_t frame_interval = frame_start - decoder->last_frame_time_ms;
            uint32_t dynamic_threshold = update_dynamic_buffer(decoder, frame_interval);
            xz_tts_player_set_buffer_threshold(decoder->player, dynamic_threshold);
            decoder->last_frame_time_ms = frame_start;

            /* 解码 */
            int16_t pcm_buffer[MAX_PCM_SAMPLES];
            int out_len = MAX_PCM_SAMPLES;

            uint32_t decode_start = lisa_os_get_tick_ms();
            xz_opus_dec_result_e result = xz_opus_dec_decode(decoder->decoder,
                                                              msg.opus_data,
                                                              msg.len,
                                                              pcm_buffer,
                                                              &out_len);
            uint32_t decode_end = lisa_os_get_tick_ms();
            uint32_t decode_time = decode_end - decode_start;

            if (result == XZ_OPUS_DECODE_OK) {
                uint32_t write_start = lisa_os_get_tick_ms();
                xz_tts_player_write(decoder->player, pcm_buffer, out_len);
                uint32_t write_end = lisa_os_get_tick_ms();
                uint32_t write_latency = write_end - decoder->start_time_ms;
                LISA_LOGI(TAG, "Frame %d: latency %u ms, decode %u ms, written at %u ms, %d samples",
                         decoder->frame_count, frame_latency, decode_time, write_latency, out_len);
            } else {
                LISA_LOGE(TAG, "Frame %d: Decode failed: %d", decoder->frame_count, result);
            }
        }
    }

    /* 重置解码器 */
    xz_opus_dec_reset(decoder->decoder);

    decoder->worker_running = false;
    LISA_LOGI(TAG, "TTS decoder worker thread exiting");
}
