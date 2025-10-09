#define TAG "soundplayer"

#include "sound_player.h"
#include <stdlib.h>
#include <string.h>
#include "evs_utils.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "audio_out.h"
#include "app_tone.h"
#include "app_player.h"
#include "controller/assistant_controller.h"

#define FILE_TYPE_FLASH	(0)
#define FILE_TYPE_FATFS	(1)

static sound_player_t *s_sound_player = NULL;

static void __release_item(sound_player_t *handle)
{
	if (handle) {
		if (handle->m_item != NULL) {
			LISA_LOGD(TAG, "release alert item");
			lisa_mem_free(handle->m_item);
			handle->m_item = NULL;
		}
		handle->m_wait_len = 0;
	}
}

static void _item_backup(sound_player_t *handle, audio_out_t *item, uint32_t item_size)
{
	if (handle) {
		handle->m_wait_len = item_size;
		handle->m_item = (audio_out_t *)lisa_mem_calloc(item_size, sizeof(audio_out_t));
		for (int i = 0; i < item_size; i++) {
			memcpy(&((handle->m_item)[i]), &(item[item_size - 1 - i]), sizeof(audio_out_t));
			// LISA_LOGD(TAG, "Backup Item URL %d: %s", i + 1, handle->m_item[i].m_url);
		}
	}
}

static int _sound_on_play_state(uint16_t st);

static void _sound_play_next(sound_player_t *handle)
{
	LISA_LOGD(TAG, "_sound_play_next, handle->m_wait_len:%d", handle->m_wait_len);
	if (handle->m_wait_len > 0) {
		handle->m_wait_len--;
		if (handle->m_file_type == FILE_TYPE_FLASH) {
			uint16_t next_id = handle->m_cur_ids[handle->m_wait_len];
			char *url = app_tone_get_url(next_id);
			LISA_LOGD(TAG, "play next tone %d: %s", next_id, url);
			app_player_play(PLAYER_T_TONE, url, _sound_on_play_state);
		} else if (handle->m_file_type == FILE_TYPE_FATFS) {
			memcpy(handle->m_curr_item, &(handle->m_item)[handle->m_wait_len], sizeof(audio_out_t));
			if (handle->m_wait_len == 0) {
				lisa_mem_free(handle->m_item);
				handle->m_item = NULL;
			}
			LISA_LOGD(TAG, "play next url %s", handle->m_curr_item->m_url);
			app_player_play(PLAYER_T_TONE, handle->m_curr_item->m_url, _sound_on_play_state);
		}
	}
}

static void _sound_play_background(sound_player_t *handle) {}

static void _sound_play_foreground(sound_player_t *handle)
{
	LISA_LOGD(TAG, "_sound_play_foreground, handle->m_play_state:%d", handle->m_play_state);
	if (handle->m_play_state != PLAYER_EVT_PLAYING) {
		_sound_play_next(handle);
	}
}

static void _sound_acquire_channel(sound_player_t *handle)
{
	LISA_LOGD(TAG, "sound acquire channel");
	handle->m_play_after_stop = true;
	listen_audiomgr_acquire_channel(handle->m_audio_mgr, handle->m_channel_name);
}

static void _sound_release_channel(sound_player_t *handle)
{
	if (handle->m_focus_state != NONE) {
		listen_audiomgr_release_channel(handle->m_audio_mgr, handle->m_channel_name);
	}
}

static void _sound_on_focus_state(focus_state_e focus_state, channel_type_e by_which)
{
	LISA_LOGI(TAG, "focus %d, by_which %d", focus_state, by_which);
	if (s_sound_player->m_focus_state != focus_state) {
		s_sound_player->m_focus_state = focus_state;

		if (focus_state == NONE) {
			s_sound_player->m_wait_len = 0;
			if (s_sound_player->m_play_state != PLAYER_EVT_STOPED &&
					s_sound_player->m_play_state != PLAYER_EVT_PLAYBACK_COMPLETE &&
					s_sound_player->m_play_state != PLAYER_EVT_ERROR) {
				app_player_stop_sync(PLAYER_T_TONE);
			}
		} else if (focus_state == FOREGROUND) {
			_sound_play_foreground(s_sound_player);
		} else if (focus_state == BACKGROUND) {
			_sound_play_background(s_sound_player);
		}

		if (s_sound_player->m_focus_cb) {
			s_sound_player->m_focus_cb->on_focus_state(focus_state, by_which);
		}
	}
}

static channel_callback_cb s_channel_cb = {
		.on_focus_state = _sound_on_focus_state,
		.m_channel_type = LOCAL,
};

static int _handle_sound_play_end(void *user_data) 
{
	sound_player_t *handle = s_sound_player;
	if (!handle->m_play_after_stop || handle->m_wait_len <= 0) {
		_sound_release_channel(handle);
	} else {
		_sound_play_next(handle);
	}
	return 0;
}

static int _sound_on_play_state(uint16_t st)
{
	LISA_LOGD(TAG, "soundplayer status: %#X", st);
	if (st == APP_PLAYER_PREPARING || st == PLAYER_EVT_STOPED || st == PLAYER_EVT_ERROR ||
			st == PLAYER_EVT_PLAYING || st == PLAYER_EVT_PLAYBACK_COMPLETE) {
		sound_player_t *handle = s_sound_player;
		if (handle->m_play_state != st) {
			handle->m_play_state = st;

			if (st == APP_PLAYER_PREPARING) {
				assist_controller_trigger_event(CONTROLLER_EVENT_STATE_SOUND_PLAY_START, NULL, 0);
			}
			if (st == PLAYER_EVT_STOPED || st == PLAYER_EVT_PLAYBACK_COMPLETE ||
					st == PLAYER_EVT_ERROR) {
				evs_handler_post_runnable(_handle_sound_play_end, NULL);
				assist_controller_trigger_event(CONTROLLER_EVENT_STATE_SOUND_PLAY_FINISH, NULL, 0);
			}
		} else {
			// Fix: 第一次出错的情况下, 后面再出错, 不能释放焦点
			if (st == PLAYER_EVT_ERROR) {
				_sound_release_channel(s_sound_player);
			}
		}
	}
	return 0;
}

static void _sound_stop(sound_player_t *handle, bool play_after_stop)
{
	if (handle->m_play_state == PLAYER_EVT_PLAYING || handle->m_play_state == PLAYER_EVT_PAUSED) {
		handle->m_play_after_stop = play_after_stop;
		app_player_stop_sync(PLAYER_T_TONE);
	} else if (handle->m_play_state == APP_PLAYER_PREPARING) {
		LISA_LOGD(TAG, "stop when preparing");
		handle->m_play_after_stop = play_after_stop;
		app_player_stop_sync(PLAYER_T_TONE);
	} else if (handle->m_play_state == PLAYER_EVT_PREPARED) {
		LISA_LOGD(TAG, "sound reset when prepared");
		app_player_reset(PLAYER_T_TONE);
	}
}

sound_player_t *listen_soundplayer_create(listen_audiomgr_t *audio_mgr)
{
	sound_player_t *handle = (sound_player_t *)lisa_mem_calloc(1, sizeof(sound_player_t));

	handle->m_audio_mgr = audio_mgr;
	handle->m_channel_name = LOCAL;
	handle->m_player_name = "local_player";

	memset(handle->m_cur_ids, 0, sizeof(uint16_t) * LS_SOUND_AUDIO_MAX_COUNT);
	handle->m_play_after_stop = false;
	handle->m_curr_item = (audio_out_t *)lisa_mem_alloc(sizeof(audio_out_t));
	handle->m_wait_len = 0;

	handle->m_focus_state = NONE;
	handle->m_play_state = PLAYER_EVT_ERROR;
	handle->m_focus_cb = NULL;
	handle->m_file_type = FILE_TYPE_FLASH;

	listen_audiomgr_add_channel_callback(handle->m_audio_mgr, &s_channel_cb);

	handle->stop = _sound_stop;

	s_sound_player = handle;
	return handle;
}

void listen_soundplayer_destroy(sound_player_t *handle)
{
	if (handle != NULL) {
		if (handle->m_curr_item) lisa_mem_free(handle->m_curr_item);
		lisa_mem_free(handle);
	}
}

void listen_soundplayer_play(sound_player_t *handle, uint16_t toneid, bool print_log)
{
	if (print_log) LISA_LOG(TAG, "offline_tts_callbak, tts: %d", toneid);
	if (app_tone_get_url(toneid) == NULL) {
		LISA_LOGW(TAG, "tts %d can't play", toneid);
		return;
	}

	__release_item(handle);
	handle->m_cur_ids[0] = toneid;
	handle->m_wait_len = 1;
	handle->m_file_type = FILE_TYPE_FLASH;
	handle->stop(handle, true);
	if (handle->m_focus_state == NONE) {
		_sound_acquire_channel(handle);
	}
}

void listen_soundplayer_play_array(sound_player_t *handle, uint16_t *tones, uint8_t len)
{
	__release_item(handle);
	handle->m_wait_len = (len <= LS_SOUND_AUDIO_MAX_COUNT)? len:LS_SOUND_AUDIO_MAX_COUNT;
	uint16_t item_id;
	// 忽略的音频计数
	uint8_t ignore_len = 0;
	// 需要播放的音频计数
	uint8_t play_len = 0;
	// 循环赋值
	for(int i = 0; i < handle->m_wait_len; i++) {
		item_id = tones[handle->m_wait_len - i - 1];
		// 过滤不能播放的音频
		if (app_tone_get_url(item_id) == NULL) {
			LISA_LOGW(TAG, "tts %d can't play", item_id);
			ignore_len++;
			continue;
		}
		handle->m_cur_ids[play_len++] = item_id;
	}
	// 更新 Wait len
	handle->m_wait_len = play_len;
	if (handle->m_wait_len <= 0) {
		return;
	}

	LISA_LOGH(TAG, tones, len, "offline_tts_callbak, tts");

	/** 
	 * Note: The prompt tone will not play correctly 
	 * if it is in the PLAYER_EVT_PLAYING state.
	 */
	if (handle->m_play_state == PLAYER_EVT_PLAYING) {
		LISA_LOGW(TAG, "offline_tts_callbak, handle->m_play_state:%d", handle->m_play_state);
		handle->m_play_state = PLAYER_EVT_ERROR;
	}
	handle->m_file_type = FILE_TYPE_FLASH;
	handle->stop(handle, true);
	if (handle->m_focus_state == NONE) {
		_sound_acquire_channel(handle);
	}
}

void listen_soundplayer_add_focus_callback(sound_player_t *handle, sound_focus_callback_t *callback)
{
	handle->m_focus_cb = callback;
}

void listen_soundplayer_play_fs(sound_player_t *handle, audio_out_t *item, int size, bool print_log)
{
	if (print_log) LISA_LOG(TAG, "offline_tts_callbak");

	__release_item(handle);
	_item_backup(handle, item, size);
	handle->m_file_type = FILE_TYPE_FATFS;
	handle->stop(handle, true);
	if (handle->m_focus_state == NONE) {
		_sound_acquire_channel(handle);
	}
}
