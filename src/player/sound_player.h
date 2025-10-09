#ifndef __LISTENAI_SOUND_PLAYER_H__
#define __LISTENAI_SOUND_PLAYER_H__

#include "listen_audiomgr.h"
#include <stdbool.h>
#include <stdint.h>

typedef void (*soundplayer_cb)();

typedef struct {
	void (*on_focus_state)(focus_state_e state, channel_type_e by_which);
} sound_focus_callback_t;

#define LS_SOUND_AUDIO_MAX_COUNT (10)

typedef struct sound_player_s {
	
	void (*stop)(struct sound_player_s *handle, bool playAfterStop);

	channel_type_e m_channel_name;
	char *m_player_name;

	// 播放列表
	uint16_t m_cur_ids[LS_SOUND_AUDIO_MAX_COUNT];
	// 需要一次播放的数量
	int m_wait_len;

	bool m_play_after_stop;

	focus_state_e m_focus_state;
	uint16_t m_play_state;

	void *m_player;
	listen_audiomgr_t *m_audio_mgr;

	sound_focus_callback_t *m_focus_cb;
	int m_file_type;
	struct audio_out_s *m_item;
	struct audio_out_s *m_curr_item;
} sound_player_t;

sound_player_t *listen_soundplayer_create(listen_audiomgr_t *audio_mgr);
void listen_soundplayer_destroy(sound_player_t *handle);

void listen_soundplayer_play(sound_player_t *handle, uint16_t toneid, bool print_log);
void listen_soundplayer_play_fs(sound_player_t *handle, struct audio_out_s *item, int size, bool print_log);

/**
 * @brief 	播放音频数组
 * @param 	handle 	播放器句柄
 * @param 	tones 	音频ID数组
 * @param 	len 	音频数量
 */
void listen_soundplayer_play_array(sound_player_t *handle, uint16_t *tones, uint8_t len);

/**
 * @brief 	设置焦点回调
 * @param  	handle		播放器句柄
 * @param  	callback	焦点回调
 */
void listen_soundplayer_add_focus_callback(sound_player_t *handle, sound_focus_callback_t *callback);

#endif
