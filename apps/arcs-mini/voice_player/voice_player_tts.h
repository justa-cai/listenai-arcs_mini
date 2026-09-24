#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @file voice_player_tts.h
 * @brief TTS 播放执行器的应用内接口。
 */

/**
 * @brief 初始化 TTS 播放队列、消息订阅和播放器事件回调。
 * @return 0 初始化成功，其他值表示失败。
 */
int voice_player_tts_init(void);

/** @brief 立即作废当前请求、清空播放队列并同步停止底层播放器。 */
void voice_player_tts_stop(void);

/** @brief 使当前 TTS 请求立即失效，播放器停止操作在异步任务中执行。 */
void voice_player_tts_stop_async(void);

/** @brief Wait briefly for a queued async stop to release the underlying track. */
bool voice_player_tts_wait_stop_complete(uint32_t timeout_ms);

/**
 * @brief 快照当前 TTS 后执行硬停止；快照不属于播放队列。
 * @return true 已保存复播快照，false 当前没有可保存的 TTS。
 */
bool voice_player_tts_snapshot_and_stop(void);

/**
 * @brief 快照并使当前 TTS 请求立即失效，播放器停止操作在异步任务中执行。
 * @return true 已保存可复播的 TTS 快照，false 当前没有可保存的 TTS。
 */
bool voice_player_tts_snapshot_and_stop_async(void);

/** @brief 丢弃尚未消费的 TTS 复播快照。 */
void voice_player_tts_discard_prepared_replay(void);

/**
 * @brief 消费并异步播放已准备的 TTS 快照，作为全新请求入队。
 * @return true 已提交复播，false 没有可复播快照或队列提交失败。
 */
bool voice_player_tts_replay_prepared(void);

/** @brief 查询是否存在已入队、正在预取或正在播放的 TTS 请求。 */
bool voice_player_tts_is_active(void);

/* Owned playback shares the same player but never cancels another owner. */
typedef enum {
    VOICE_TTS_OWNED_PENDING,
    VOICE_TTS_OWNED_COMPLETED,
    VOICE_TTS_OWNED_FAILED,
    VOICE_TTS_OWNED_INTERRUPTED,
} voice_tts_owned_result_t;
bool voice_player_tts_claim(uint32_t owner);
bool voice_player_tts_play_owned(uint32_t owner, const char *url);
void voice_player_tts_cancel_owned(uint32_t owner);
voice_tts_owned_result_t voice_player_tts_owned_result(uint32_t owner);
void voice_player_tts_release_owned(uint32_t owner);
