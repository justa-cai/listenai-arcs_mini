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
 * @brief 播放音频数组（播放列表）
 * @param items 音频项数组
 * @param count 数组长度
 * @return 0 成功, 其他失败
 */
int voice_player_play_array(const struct voice_msg_audio_item *items, int count);

/**
 * @brief 播放下一首
 * @return 0 成功, 其他失败
 */
int voice_player_play_next(void);

/**
 * @brief 播放上一首
 * @return 0 成功, 其他失败
 */
int voice_player_play_prev(void);

/**
 * @brief 重新播放当前歌曲
 * @return 0 成功, 其他失败
 */
int voice_player_replay_current(void);

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
 * @brief 通知语音播放器：相机拍照提示音即将开始
 *
 * 用于在拍照提示音结束后恢复被其打断的云端 TTS。
 */
void voice_player_notify_camera_capture_tone_start(void);

/**
 * @brief 复制最近一次收到的 TTS URL
 *
 * @param url_buf 输出缓冲区
 * @param buf_len 输出缓冲区长度
 * @return true 复制成功，false 当前没有缓存可用 URL
 */
bool voice_player_latest_tts_url_copy(char *url_buf, size_t buf_len);

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
