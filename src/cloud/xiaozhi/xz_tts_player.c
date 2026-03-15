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
            vTaskDelay(pdMS_TO_TICKS(10));
            lisa_player_play(s_tts_player->player);
            LISA_LOGI(TAG, "TTS player prepared and started");
            break;

        case PLAYER_EVT_PLAYING:
            LISA_LOGD(TAG, "TTS player playing event");
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
    int ret = lisa_player_put_stream_data(player->player,
                                          (uint8_t *)samples,
                                          size,
                                          1000);
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
