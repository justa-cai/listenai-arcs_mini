#ifndef __VOICE_PLAYER_COMM_H__
#define __VOICE_PLAYER_COMM_H__

#include <stdint.h>
#include <stddef.h>
#include "voice_msg.h"
#include "app_player.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 播放器实例（在 voice_player_comm.c 中定义） */
extern app_player_t *tone_player;
extern app_player_t *tts_player;
extern app_player_t *music_player;
extern app_player_t *alert_player;

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

/**
 * @brief 播放音乐 URL，播放 /SD:/ 本地文件前会先确认路径可访问
 * @param url 音乐 URL 或本地文件路径
 * @return APP_PLAYER_OK 成功，其他失败
 */
int voice_player_play_music_url(const char *url);

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

/**
 * @brief 播放提示音 URL（带排队机制）。
 *        若当前有 tone 正在播放则入队，播完自动播下一个。url 为 NULL 时直接跳过。
 * @param url 提示音资源 URL（mem:// 或 http://）
 */
void voice_player_play_tone_url(const char *url);

/**
 * @brief 复制最近一次收到的 TTS URL
 *
 * @param url_buf 输出缓冲区
 * @param buf_len 输出缓冲区长度
 * @return true 复制成功，false 当前没有缓存可用 URL
 */
bool voice_player_latest_tts_url_copy(char *url_buf, size_t buf_len);

/**
 * @brief 重播指定的 TTS URL（绕开门控，直接入异步队列）
 *
 * 用于 intent 栈恢复被抢占的 VOICE_SESSION 时回放 TTS。
 *
 * @param url TTS 资源 URL
 */
void voice_player_replay_tts_url(const char *url);

/**
 * @brief 查询 TTS 播放器是否处于活跃状态（非阻塞）
 *
 * 基于 voice_player 内部维护的事件标志，不会获取播放器 operation_lock，
 * 适合在 ebus 回调等时序敏感路径中使用。状态可能略滞后于底层播放器实例。
 *
 * @return true 已下发 TTS 播放/恢复请求且尚未收到 stop/error/complete 事件
 */
bool voice_player_tts_is_active(void);

/**
 * @brief 异步播放 TTS URL（track URL + 入队播放，绕开门控）
 *
 * 供 camera 等模块在内部决定播放时机后直接下发 TTS。
 *
 * @param url TTS 资源 URL
 */
void voice_player_play_tts_url_async(const char *url);

#ifdef __cplusplus
}
#endif

#endif /* __VOICE_PLAYER_COMM_H__ */
