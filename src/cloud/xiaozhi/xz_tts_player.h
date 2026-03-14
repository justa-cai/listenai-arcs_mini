/**
 * @file xz_tts_player.h
 * @brief 小智云端 TTS 播放器
 * @note 支持动态采样率配置 (16kHz 或 24kHz)
 */

#ifndef __XZ_TTS_PLAYER_H__
#define __XZ_TTS_PLAYER_H__

#include <stdint.h>
#include <stdbool.h>
#include "listen_audiomgr.h"

#ifdef __cplusplus
extern "C" {
#endif

/** TTS 播放器句柄 */
typedef struct xz_tts_player_s *xz_tts_player_t;

/** TTS 播放器回调 */
typedef struct {
    void (*on_play_start)(xz_tts_player_t player);
    void (*on_play_complete)(xz_tts_player_t player);
    void (*on_error)(xz_tts_player_t player, const char *error);
} xz_tts_player_callbacks_t;

/** TTS 播放器配置 */
typedef struct {
    int sample_rate;    /**< 采样率 (Hz): 16000 或 24000 */
    int channels;       /**< 声道数: 1 (单声道) */
    int bits;           /**< 位深: 16 */
} xz_tts_player_config_t;

/**
 * @brief 创建 TTS 播放器
 * @param audio_mgr 音频焦点管理器
 * @param config 播放器配置 (可为 NULL 使用默认值)
 * @param cbs 回调函数 (可为 NULL)
 * @return 播放器句柄，失败返回 NULL
 */
xz_tts_player_t xz_tts_player_create(listen_audiomgr_t *audio_mgr,
                                     const xz_tts_player_config_t *config,
                                     const xz_tts_player_callbacks_t *cbs);

/**
 * @brief 销毁 TTS 播放器
 * @param player 播放器句柄
 */
void xz_tts_player_destroy(xz_tts_player_t player);

/**
 * @brief 开始播放
 * @param player 播放器句柄
 * @return 0 成功, -1 失败
 */
int xz_tts_player_start(xz_tts_player_t player);

/**
 * @brief 停止播放
 * @param player 播放器句柄
 * @return 0 成功, -1 失败
 */
int xz_tts_player_stop(xz_tts_player_t player);

/**
 * @brief 写入 PCM 数据
 * @param player 播放器句柄
 * @param samples PCM 数据 (int16_t)
 * @param count 采样点数
 * @return 0 成功, -1 失败
 */
int xz_tts_player_write(xz_tts_player_t player, const int16_t *samples, uint32_t count);

/**
 * @brief 结束流
 * @param player 播放器句柄
 * @return 0 成功, -1 失败
 */
int xz_tts_player_end_stream(xz_tts_player_t player);

/**
 * @brief 重置播放器
 * @param player 播放器句柄
 */
void xz_tts_player_reset(xz_tts_player_t player);

/**
 * @brief 检查是否正在播放
 * @param player 播放器句柄
 * @return true 正在播放, false 未播放
 */
bool xz_tts_player_is_playing(xz_tts_player_t player);

/**
 * @brief 获取缓冲数据量
 * @param player 播放器句柄
 * @return 缓冲的数据字节数
 */
uint32_t xz_tts_player_get_buffered(xz_tts_player_t player);

#ifdef __cplusplus
}
#endif

#endif /* __XZ_TTS_PLAYER_H__ */
