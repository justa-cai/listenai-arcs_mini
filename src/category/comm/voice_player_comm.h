#ifndef __VOICE_PLAYER_COMM_H__
#define __VOICE_PLAYER_COMM_H__

#include <stdint.h>
#include <stddef.h>
#include "voice_msg.h"
#include "app_player.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 播放器实例 ==================== */

/* 实例由 voice_player_comm.c 创建，业务模块只持有和调用。 */
extern app_player_t *tone_player;
extern app_player_t *tts_player;
extern app_player_t *music_player;
extern app_player_t *alert_player;

/* ==================== 平台初始化与状态查询 ==================== */

/**
 * @brief 初始化语音播放器平台
 * @return 0 成功, 其他失败
 */
int voice_player_platform_init(void);

/**
 * @brief 判断音乐播放器是否处于活跃音频播放管线中
 *
 * 包括准备中、准备完成和播放中；暂停/停止/空闲不算活跃。
 */
bool voice_player_is_music_active(void);

/**
 * @brief 判断任一播放器是否处于活跃音频播放管线中
 *
 * 包括准备中、准备完成和播放中；暂停/停止/空闲不算活跃。
 */
bool voice_player_is_audio_active(void);

/* ==================== 系统音量 API ==================== */

/**
 * @brief 设置系统音量（设置所有已创建播放器的音量）
 * @param volume 音量值 (0-100)
 * @return 0 成功, 其他失败
 */
int voice_player_set_system_volume(int volume);

/**
 * @brief 获取系统音量
 * @return 音量值 (0-100), 失败返回 -1
 */
int voice_player_get_system_volume(void);

/* ==================== 提示音 API ==================== */

/**
 * @brief 播放提示音 URL（带排队机制）。
 *        若当前有 tone 正在播放则入队，播完自动播下一个。url 为 NULL 时直接跳过。
 * @param url 提示音资源 URL（mem:// 或 http://）
 */
void voice_player_play_tone_url(const char *url);

/**
 * @brief 播放提示音 URL（带 intent 栈管理）。
 *
 *        与 voice_player_play_tone_url 的区别：播放前会先 push INTENT_PROMPT_TONE，
 *        主动触发当前栈顶意图（如 MUSIC）的 on_preempted → app_player_pause，
 *        避免 tone 在音频焦点层直接抢占导致的 focus 状态不一致。
 *        tone 播完后自动 pop INTENT_PROMPT_TONE 恢复被抢占意图。
 *
 *        适用于相机快门音等需要在播放期间暂停后台音乐的短促音效。
 *
 * @param url 提示音资源 URL（mem:// 或 http://）
 */
void voice_player_play_prompt_tone_url(const char *url);

/* ==================== TTS API ==================== */

/**
 * @brief 查询 TTS 播放器是否处于活跃状态（非阻塞）
 *
 * 基于 voice_player 内部维护的事件标志，不会获取播放器 operation_lock，
 * 适合在 ebus 回调等时序敏感路径中使用。状态可能略滞后于底层播放器实例。
 *
 * @return true 已下发 TTS 播放/恢复请求且尚未收到 stop/error/complete 事件
 */
bool voice_player_tts_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* __VOICE_PLAYER_COMM_H__ */
