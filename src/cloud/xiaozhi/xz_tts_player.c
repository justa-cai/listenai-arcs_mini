/**
 * @file xz_tts_player.c
 * @brief 小智云端 TTS 播放器实现
 * @note 基于 jk_pcm_player.c 的架构，支持动态采样率
 */

#define TAG "xz_tts_player"

#include "xz_tts_player.h"
#include "lisa_player.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_time.h"
#include "pa_manager.h"
#include "listen_audiomgr.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

/** 默认配置 */
#define XZ_TTS_PLAYER_DEFAULT_RATE 24000
#define XZ_TTS_PLAYER_DEFAULT_CHANNELS 1
#define XZ_TTS_PLAYER_DEFAULT_BITS 16

/** TTS 播放器延迟关闭时间 (毫秒) */
#define XZ_TTS_PLAYER_CLOSE_DELAY_MS  10000  /* 10 秒 */

/** TTS 预缓冲阈值 (字节)
 * @note 首次播放前积累足够数据后再启动，避免播放卡顿
 * 24kHz 单声道 16bit：
 * - 60ms 帧 = 1440 采样点 = 2880 字节
 * - 3 帧 = 180ms = 8640 字节
 * @note 解码器已实现"第一帧缓存，第二帧到达时才开始播放"的策略
 * 第二帧到达的时间差就是自然的缓冲时间，无需固定延迟
 * @note 3 帧缓冲优先保证响应速度，让音频和文字显示同步
 * 偶尔可能有轻微卡顿，但整体体验更自然
 */
#define XZ_TTS_PLAYER_PREBUFFER_THRESHOLD  (3 * 2880)   /* 3 帧 = 180ms */

/** 全局单例播放器 */
static xz_tts_player_t s_tts_player = NULL;

/** 播放器状态 */
typedef enum {
    TTS_PLAYER_STATE_IDLE = 0,
    TTS_PLAYER_STATE_PREPARING,
    TTS_PLAYER_STATE_PREPARED,
    TTS_PLAYER_STATE_PLAYING,
} tts_player_state_e;

/** TTS 播放器结构 */
struct xz_tts_player_s {
    PLAYER_HANDLE player;
    listen_audiomgr_t *audio_mgr;
    tts_player_state_e state;
    xz_tts_player_config_t config;
    xz_tts_player_callbacks_t cbs;
    uint32_t total_written;
    bool is_playing;
    bool is_preparing;

    /* 性能测量：play() 调用时间戳 */
    uint32_t play_call_time_ms;

    /* 动态缓冲阈值 */
    uint32_t dynamic_buffer_threshold;  /* 动态设置的缓冲阈值 */
};

/** 播放器回调 */
static int tts_player_callback(PlayerEvt evt, int arg1, int arg2, int id)
{
    (void)arg1;
    (void)arg2;
    (void)id;

    if (!s_tts_player) {
        LISA_LOGW(TAG, "Callback invoked but player is NULL");
        return 0;
    }

    /* 清除准备状态 */
    s_tts_player->is_preparing = false;

    switch (evt) {
        case PLAYER_EVT_PREPARED:
            s_tts_player->state = TTS_PLAYER_STATE_PREPARED;
            /* 不立即启动播放，等待足够数据缓冲
             * write() 函数会在数据达到阈值时启动播放
             * 这样可以避免首次播放卡顿
             */
            LISA_LOGI(TAG, "TTS player prepared (waiting for data buffer threshold)");
            break;

        case PLAYER_EVT_PLAYING:
            /* 测量从 play() 调用到音频实际开始播放的延迟 */
            if (s_tts_player->play_call_time_ms > 0) {
                uint32_t now = lisa_os_get_tick_ms();
                uint32_t audio_start_latency = now - s_tts_player->play_call_time_ms;
                LISA_LOGI(TAG, "TTS player playing event (play() -> audio start: %u ms)",
                         audio_start_latency);
                s_tts_player->play_call_time_ms = 0;  /* 重置，避免重复打印 */
            } else {
                LISA_LOGD(TAG, "TTS player playing event");
            }

            if (s_tts_player->cbs.on_play_start && s_tts_player->total_written > 0) {
                s_tts_player->cbs.on_play_start(s_tts_player);
            }
            break;

        case PLAYER_EVT_PLAYBACK_COMPLETE:
            LISA_LOGI(TAG, "TTS player playback complete (total_written=%u bytes)",
                      s_tts_player->total_written);

            /* 调用回调（在重置状态之前，此时 total_written 仍然有效） */
            if (s_tts_player->cbs.on_play_complete && s_tts_player->total_written > 0) {
                s_tts_player->cbs.on_play_complete(s_tts_player);
            }

            s_tts_player->is_playing = false;
            s_tts_player->state = TTS_PLAYER_STATE_IDLE;
            s_tts_player->total_written = 0;  /* 重置写入计数器 */
            s_tts_player->is_preparing = false;  /* 清除准备状态 */

            pa_manager_refresh(PA_MGR_OFF, LS_PA_BASE_TIME, "tts_player_end");
            listen_audiomgr_release_channel(s_tts_player->audio_mgr, TTS);
            break;

        case PLAYER_EVT_ERROR:
            LISA_LOGE(TAG, "TTS player error occurred");
            s_tts_player->is_playing = false;
            s_tts_player->state = TTS_PLAYER_STATE_IDLE;
            s_tts_player->is_preparing = false;
            s_tts_player->total_written = 0;  /* 重置写入计数器 */

            pa_manager_refresh(PA_MGR_OFF, LS_PA_BASE_TIME, "tts_player_error");
            listen_audiomgr_release_channel(s_tts_player->audio_mgr, TTS);

            if (s_tts_player->cbs.on_error) {
                s_tts_player->cbs.on_error(s_tts_player, "Playback error");
            }
            break;

        default:
            LISA_LOGD(TAG, "TTS player unhandled event: %d", evt);
            break;
    }

    return 0;
}

xz_tts_player_t xz_tts_player_create(listen_audiomgr_t *audio_mgr,
                                     const xz_tts_player_config_t *config,
                                     const xz_tts_player_callbacks_t *cbs)
{
    if (!audio_mgr) {
        LISA_LOGE(TAG, "audio_mgr is NULL");
        return NULL;
    }

    if (s_tts_player) {
        LISA_LOGI(TAG, "TTS player already exists (singleton), returning existing instance");
        return s_tts_player;
    }

    xz_tts_player_t player = (xz_tts_player_t)lisa_mem_calloc(1, sizeof(struct xz_tts_player_s));
    if (!player) {
        LISA_LOGE(TAG, "Failed to allocate player");
        return NULL;
    }

    /* 保存配置 */
    if (config) {
        player->config.sample_rate = config->sample_rate > 0 ? config->sample_rate : XZ_TTS_PLAYER_DEFAULT_RATE;
        player->config.channels = config->channels > 0 ? config->channels : XZ_TTS_PLAYER_DEFAULT_CHANNELS;
        player->config.bits = config->bits > 0 ? config->bits : XZ_TTS_PLAYER_DEFAULT_BITS;
    } else {
        player->config.sample_rate = XZ_TTS_PLAYER_DEFAULT_RATE;
        player->config.channels = XZ_TTS_PLAYER_DEFAULT_CHANNELS;
        player->config.bits = XZ_TTS_PLAYER_DEFAULT_BITS;
    }

    /* 创建播放器 */
    player->player = lisa_player_create("tts_player", 0);
    if (!player->player) {
        LISA_LOGE(TAG, "Failed to create lisa player");
        lisa_mem_free(player);
        return NULL;
    }

    lisa_player_set_callback(player->player, tts_player_callback);
    lisa_player_set_vol(player->player, 60);

    /* 保存 audio_mgr 和回调 */
    player->audio_mgr = audio_mgr;
    if (cbs) {
        player->cbs = *cbs;
    }

    player->state = TTS_PLAYER_STATE_IDLE;
    player->is_playing = false;
    player->is_preparing = false;
    player->total_written = 0;
    player->dynamic_buffer_threshold = XZ_TTS_PLAYER_PREBUFFER_THRESHOLD;  /* 默认使用定义的阈值 */

    s_tts_player = player;

    LISA_LOGI(TAG, "TTS player created: %dHz, %dch, %dbits",
              player->config.sample_rate, player->config.channels, player->config.bits);

    return player;
}

void xz_tts_player_destroy(xz_tts_player_t player)
{
    if (!player) {
        LISA_LOGW(TAG, "Cannot destroy NULL player");
        return;
    }

    /* 释放 TTS 通道 */
    if (player->state != TTS_PLAYER_STATE_IDLE) {
        listen_audiomgr_release_channel(player->audio_mgr, TTS);
        pa_manager_refresh(PA_MGR_OFF, LS_PA_BASE_TIME, "tts_player_destroy");
    }

    /* 销毁播放器 */
    if (player->player) {
        lisa_player_stop_sync(player->player);
        lisa_player_destory(player->player);
    }

    if (s_tts_player == player) {
        s_tts_player = NULL;
    }

    lisa_mem_free(player);
    LISA_LOGI(TAG, "TTS player destroyed");
}

int xz_tts_player_start(xz_tts_player_t player)
{
    if (!player) {
        LISA_LOGE(TAG, "Invalid player");
        return -1;
    }

    LISA_LOGI(TAG, "TTS player start: is_playing=true");
    player->is_playing = true;
    return 0;
}

int xz_tts_player_prewarm(xz_tts_player_t player)
{
    if (!player) {
        LISA_LOGE(TAG, "Invalid player");
        return -1;
    }

    /* 如果播放器已经在准备或准备就绪状态，无需预热 */
    if (player->state != TTS_PLAYER_STATE_IDLE || player->is_preparing) {
        LISA_LOGD(TAG, "Player already initialized (state=%d, is_preparing=%d), skipping prewarm",
                  player->state, player->is_preparing);
        return 0;
    }

    LISA_LOGI(TAG, "TTS player prewarming: initializing audio device early");

    /* 设置 PCM 流 URL，触发底层音频设备初始化
     * 这样可以提前完成 ALSA/DMA 配置，避免第一帧数据到达时的延迟
     */
    char url[128];
    snprintf(url, sizeof(url), "stream://type=pcm&rate=%d&channel=%d&bits=%d",
             player->config.sample_rate, player->config.channels, player->config.bits);

    uint32_t t1 = lisa_os_get_tick_ms();
    int ret = lisa_player_seturl(player->player, url);
    uint32_t t2 = lisa_os_get_tick_ms();

    if (ret != PLAYER_OK) {
        LISA_LOGE(TAG, "Failed to set stream URL during prewarm: ret=%d", ret);
        return -1;
    }

    LISA_LOGI(TAG, "lisa_player_seturl() completed in %u ms", t2 - t1);

    /* 标记为准备中，防止重复初始化 */
    player->is_preparing = true;
    player->total_written = 0;

    /* 提前获取音频通道，避免资源竞争 */
    pa_manager_refresh(PA_MGR_ON, LS_PA_FOREVER, "tts_player_prewarm");
    listen_audiomgr_acquire_channel(player->audio_mgr, TTS);
    LISA_LOGI(TAG, "TTS channel acquired during prewarm");

    /* 写入静音数据触发 lisa_player 内部初始化
     * 但 prepared 回调不会立即启动播放，等待真实数据缓冲
     */
    int frame_samples = (player->config.sample_rate * 60) / 1000;
    int prebuffer_frames = 3;
    int total_samples = frame_samples * prebuffer_frames;
    int16_t *silence = (int16_t *)lisa_mem_calloc(total_samples, sizeof(int16_t));

    if (silence) {
        ret = lisa_player_put_stream_data(player->player, (uint8_t *)silence,
                                          total_samples * sizeof(int16_t), 1000);
        lisa_mem_free(silence);

        uint32_t t3 = lisa_os_get_tick_ms();
        LISA_LOGI(TAG, "Prewarm %d silence frames written in %u ms, ret=%d",
                  prebuffer_frames, t3 - t2, ret);
    } else {
        LISA_LOGW(TAG, "Failed to allocate silence buffer for prewarm");
    }

    LISA_LOGI(TAG, "TTS player prewarm completed");
    return 0;
}

int xz_tts_player_stop(xz_tts_player_t player)
{
    if (!player) {
        LISA_LOGE(TAG, "Invalid player");
        return -1;
    }

    if (player->is_playing) {
        LISA_LOGI(TAG, "TTS player stop: is_playing=false");
        player->is_playing = false;
    }

    return 0;
}

int xz_tts_player_write(xz_tts_player_t player, const int16_t *samples, uint32_t count)
{
    if (!player) {
        LISA_LOGE(TAG, "Invalid player");
        return -1;
    }

    if (!samples || count == 0) {
        LISA_LOGW(TAG, "Invalid samples: samples=%p, count=%u", samples, count);
        return -1;
    }

    /* 只有在 is_playing=true 时才写入数据 */
    if (!player->is_playing) {
        LISA_LOGD(TAG, "Not playing (is_playing=false), discarding %u samples", count);
        return 0;
    }

    /* 准备播放器（如果未准备好） */
    if (player->state == TTS_PLAYER_STATE_IDLE && !player->is_preparing) {
        /* 设置 PCM 流 URL */
        char url[128];
        snprintf(url, sizeof(url), "stream://type=pcm&rate=%d&channel=%d&bits=%d",
                 player->config.sample_rate, player->config.channels, player->config.bits);

        int ret = lisa_player_seturl(player->player, url);
        if (ret != PLAYER_OK) {
            LISA_LOGE(TAG, "Failed to set stream URL: ret=%d, attempting recovery", ret);

            /* 无论当前状态是什么，都尝试停止播放器并重试
             * lisa_player 可能处于错误状态（如 state=7），需要恢复
             */
            LISA_LOGW(TAG, "Stopping player to recover from error state");
            lisa_player_stop_sync(player->player);
            vTaskDelay(pdMS_TO_TICKS(20));

            /* 重试设置 URL */
            ret = lisa_player_seturl(player->player, url);
            if (ret != PLAYER_OK) {
                LISA_LOGE(TAG, "Failed to set stream URL after recovery retry: ret=%d", ret);
                player->is_playing = false;
                return -1;
            }
            LISA_LOGI(TAG, "Stream URL set successfully after recovery");
        }

        player->is_preparing = true;
        player->total_written = 0;

        pa_manager_refresh(PA_MGR_ON, LS_PA_FOREVER, "tts_player_start");
        listen_audiomgr_acquire_channel(player->audio_mgr, TTS);
        LISA_LOGI(TAG, "TTS player preparing, TTS channel acquired");
    }

    /* 写入 PCM 数据 */
    uint32_t size = count * sizeof(int16_t);

    /* 测量 put_stream_data 耗时 */
    uint32_t write_start = lisa_os_get_tick_ms();
    int ret = lisa_player_put_stream_data(player->player,
                                          (uint8_t *)samples,
                                          size,
                                          1000);
    uint32_t write_end = lisa_os_get_tick_ms();
    uint32_t write_time_ms = write_end - write_start;

    if (ret < 0) {
        if (player->state == TTS_PLAYER_STATE_PREPARED) {
            LISA_LOGE(TAG, "Failed to write PCM data: ret=%d", ret);
            return -1;
        } else {
            /* 准备期间的写入失败是正常的 */
            return 0;
        }
    }

    player->total_written += size;

    /* 只在前几帧打印 put_stream_data 耗时 */
    static int perf_count = 0;
    if (perf_count < 3) {
        LISA_LOGI(TAG, "put_stream_data: %u bytes in %u ms", size, write_time_ms);
        perf_count++;
    }

    /* 预缓冲优化：如果是第一帧数据（total_written == size），立即标记为已处理
     * 这样解码器可以继续处理队列中的更多 Opus 数据
     * 目标是在 lisa_player 触发 PREPARED 之前，尽可能多地缓冲 PCM 数据
     */
    if (player->state == TTS_PLAYER_STATE_IDLE && player->total_written == size) {
        /* 第一帧数据已写入，让出 CPU 时间给解码器处理队列中的数据 */
        vTaskDelay(pdMS_TO_TICKS(10));
        LISA_LOGI(TAG, "First frame written, yielding to decoder for prebuffering");
    }

    /* 预缓冲检查：如果状态为 PREPARED 且缓冲数据达到阈值，则启动播放
     * @note 解码器已实现"第一帧缓存，第二帧到达时才开始播放"的策略
     * 第二帧到达的时间差就是自然的缓冲时间
     * @note 使用动态阈值，根据实时帧间隔自适应调整
     */
    if (player->state == TTS_PLAYER_STATE_PREPARED) {
        if (player->total_written >= player->dynamic_buffer_threshold) {
            /* 记录 play() 调用时间，用于测量底层启动延迟 */
            player->play_call_time_ms = lisa_os_get_tick_ms();

            /* 测量 lisa_player_play 调用耗时 */
            uint32_t play_start = lisa_os_get_tick_ms();
            lisa_player_play(player->player);
            uint32_t play_end = lisa_os_get_tick_ms();

            player->state = TTS_PLAYER_STATE_PLAYING;

            LISA_LOGI(TAG, "TTS playback started (buffered=%u bytes, threshold=%u bytes, play() took %u ms)",
                      player->total_written, player->dynamic_buffer_threshold, play_end - play_start);
        }
    }

    /* 调试输出 */
    static int write_count = 0;
    if (write_count++ < 5) {
        LISA_LOGI(TAG, "PCM write: %u samples, %u bytes, total: %u (state=%d)",
                  count, size, player->total_written, player->state);
    }

    return 0;
}

int xz_tts_player_end_stream(xz_tts_player_t player)
{
    if (!player || !player->player) {
        LISA_LOGE(TAG, "Invalid player");
        return -1;
    }

    LISA_LOGI(TAG, "End stream: waiting for playback to complete naturally");

    /* 如果状态是 PREPARED 且有数据，但还没达到阈值启动播放
     * 这是 TTS 内容很短的情况（少于阈值要求）
     * 需要强制启动播放，否则用户听不到任何声音
     */
    if (player->state == TTS_PLAYER_STATE_PREPARED && player->total_written > 0) {
        LISA_LOGI(TAG, "TTS end reached with data buffered=%u bytes, forcing playback start",
                 player->total_written);

        /* 记录 play() 调用时间，用于测量底层启动延迟 */
        player->play_call_time_ms = lisa_os_get_tick_ms();

        /* 测量 lisa_player_play 调用耗时 */
        uint32_t play_start = lisa_os_get_tick_ms();
        lisa_player_play(player->player);
        uint32_t play_end = lisa_os_get_tick_ms();

        player->state = TTS_PLAYER_STATE_PLAYING;

        LISA_LOGI(TAG, "Forced playback start (short content, buffered=%u bytes, play() took %u ms)",
                 player->total_written, play_end - play_start);
    }

    /* 不立即标记流结束
     * 让播放器自然完成缓冲区的音频播放
     * 等待 PLAYER_EVT_PLAYBACK_COMPLETE 事件来清理状态
     *
     * 这样可以避免：
     * 1. 播放器状态不一致（is_playing=false 但内部仍是 PLAYING）
     * 2. 底层流检测到 EOF 时的停止失败错误
     */

    /* 只停止接受新数据（解码器已停止）
     * 但不修改播放器状态，让它自然完成
     */

    LISA_LOGI(TAG, "End stream: player will complete naturally (state=%d, total_written=%u)",
              player->state, player->total_written);

    return 0;
}

void xz_tts_player_reset(xz_tts_player_t player)
{
    if (!player) {
        LISA_LOGW(TAG, "Cannot reset NULL player");
        return;
    }

    LISA_LOGI(TAG, "TTS player reset: clearing state");
    player->is_playing = false;
    player->is_preparing = false;
    player->state = TTS_PLAYER_STATE_IDLE;
    player->total_written = 0;
}

bool xz_tts_player_is_playing(xz_tts_player_t player)
{
    return player && player->is_playing;
}

uint32_t xz_tts_player_get_buffered(xz_tts_player_t player)
{
    return player ? player->total_written : 0;
}

void xz_tts_player_set_buffer_threshold(xz_tts_player_t player, uint32_t threshold)
{
    if (!player) {
        return;
    }
    player->dynamic_buffer_threshold = threshold;
    LISA_LOGD(TAG, "Buffer threshold set to %u bytes (%.1f frames)",
              threshold, threshold / (float)(3 * 960));  /* 24kHz 单声道 16bit: 1 帧 = 2880 字节 */
}
