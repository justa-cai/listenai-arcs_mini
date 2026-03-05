#ifndef __LISTENAI_SOUND_PLAYER_H__
#define __LISTENAI_SOUND_PLAYER_H__

#include "listen_audiomgr.h"
#include "audio_out.h"
#include <stdbool.h>
#include <stdint.h>

typedef void (*soundplayer_cb)();

typedef struct {
	void (*on_focus_state)(focus_state_e state, int by_which);
	void (*on_play_state)(uint16_t state);
} sound_player_callback_t;

#define LS_SOUND_AUDIO_MAX_COUNT (10)

/**
 * @brief 播放器配置（用于新焦点管理，支持多实例）
 */
typedef struct sound_player_config_s {
	int id;
	char *name;
	int priority;
	int capture_ids[MAX_CAPTURE_COUNT];
	int capture_count;
} sound_player_config_t;

typedef struct sound_player_s {
	
	void (*stop)(struct sound_player_s *handle, bool playAfterStop);

	int m_id;
	char *m_player_name;

	// 需要一次播放的数量
	int m_wait_len;

	bool m_play_after_stop;

	focus_state_e m_focus_state;
	uint16_t m_play_state;

	void *m_player;
	listen_audiomgr_t *m_audio_mgr;

	sound_player_callback_t *m_callback;
	struct audio_out_s *m_item;
	struct audio_out_s *m_curr_item;
} sound_player_t;

/**
 * @brief 创建播放器（支持多实例，使用新焦点管理）
 * @param config 播放器配置
 * @param audio_mgr 焦点管理器
 * @return 播放器句柄
 */
sound_player_t *listen_soundplayer_create(sound_player_config_t *config, listen_audiomgr_t *audio_mgr);

void listen_soundplayer_destroy(sound_player_t *handle);

void listen_soundplayer_play(sound_player_t *handle, char *path, bool print_log);

/**
 * @brief 播放音频（支持打断/追加模式）
 * @param handle 播放器句柄
 * @param item 音频项
 * @param interrupt true=打断当前播放，false=等当前播完再播
 */
void listen_soundplayer_play_item(sound_player_t *handle, audio_out_t *item, bool interrupt);

/**
 * @brief 	播放音频结构体数组
 * @param 	handle 	播放器句柄
 * @param 	items 	音频结构体数组
 * @param 	len 	音频数量
 */
void listen_soundplayer_play_array(sound_player_t *handle, audio_out_t *items, uint8_t len);

/**
 * @brief 	添加回调
 * @param  	handle		播放器句柄
 * @param  	callback	回调
 */
void listen_soundplayer_add_callback(sound_player_t *handle, sound_player_callback_t *callback);

#endif
