#ifndef __VOICE_PLAYER_COMM_H__
#define __VOICE_PLAYER_COMM_H__

#include <stdint.h>
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

#ifdef __cplusplus
}
#endif

#endif /* __VOICE_PLAYER_COMM_H__ */
