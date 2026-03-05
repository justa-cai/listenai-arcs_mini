#ifndef __LISTENAI_PLAY_MODE_H__
#define __LISTENAI_PLAY_MODE_H__

#include "audio_out.h"

typedef enum {
	MODE_ORDER,  //顺序播放
	MODE_CYCLE,  //列表循环
	MODE_SINGLE,  //单曲循环
	MODE_RANDOM,  //随机模式
} PLAY_MODE_E;

typedef struct play_mode_s {
	PLAY_MODE_E m_play_mode;
	int m_curr_index;
	audio_out_t *m_music_list;
	int m_music_size;
} play_mode_t;

/**
 * @brief 播放模式管理创建
 *
 * @return play_mode_t* 句柄
 */
play_mode_t *listen_play_mode_create();

/**
 * @brief 初始化播放列表
 *
 * @param handle 句柄
 * @param item 歌曲列表
 * @param item_size 歌曲数量
 * @return int
 */
int listen_playlist_init(play_mode_t *handle, audio_out_t *item, int item_size);

/**
 * @brief 切换播放模式
 *
 * @param handle 句柄
 * @param mode 播放模式
 * @return int
 */
int listen_switch_playmode(play_mode_t *handle, PLAY_MODE_E mode);

/**
 * @brief 根据播放模式获取下一首资源
 *
 * @param handle 句柄
 * @param code 返回值，0:正常 1:已经是最后一首了 -1:没有歌曲
 * @return audio_out_t* 歌曲信息，外部不需要释放
 */
audio_out_t *listen_next_audio(play_mode_t *handle, int *code);

/**
 * @brief 根据播放模式获取上一首资源
 *
 * @param handle 句柄
 * @param code 返回值，0:正常 1:已经是第一首了 -1:没有歌曲
 * @return audio_out_t* 歌曲信息，外部不需要释放
 */
audio_out_t *listen_pre_audio(play_mode_t *handle, int *code);

/**
 * @brief 获取当前播放资源
 * 
 * @param handle 句柄
 * @return audio_out_t* 歌曲信息，外部不需要释放
 */
audio_out_t *listen_get_curr_audio(play_mode_t *handle);

/**
 * @brief 播放列表逆初始化，会释放申请的播放列表资源
 *
 * @param handle 句柄
 * @return int
 */
int listen_playlist_deinit(play_mode_t *handle);

#endif  //__LISTENAI_PLAY_MODE_H__