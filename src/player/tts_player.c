#define TAG "ttsplayer"

#include "tts_player.h"
#include <stdlib.h>
#include <string.h>
#include "evs_utils.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "app_player.h"
#include "lisa_time.h"
#include "assistant_controller.h"


static tts_player_t *s_tts_player = NULL;

static void _tts_play_next(tts_player_t *handle);

static void _tts_acquire_channel(tts_player_t *handle)
{
	LISA_LOGD(TAG, "_tts_acquire_channel");
	listen_audiomgr_acquire_channel(handle->m_audio_mgr, handle->m_channel_name);
}

static void _tts_release_channel(tts_player_t *handle)
{
	if (handle->m_focus_state != NONE) {
		listen_audiomgr_release_channel(handle->m_audio_mgr, handle->m_channel_name);
	}
}

static int _handle_tts_play_end(void *user_data)
{
	if (s_tts_player->m_ask_item != NULL || s_tts_player->m_item != NULL) {
		_tts_play_next(s_tts_player);
	} else {
		_tts_release_channel(s_tts_player);
	}
	return 0;
}

static int _tts_on_play_state(uint16_t st)
{
	LISA_LOGI(TAG, "ttsplayer status: %d", st);
	if (st == APP_PLAYER_PREPARING || st == PLAYER_EVT_STOPED || st == PLAYER_EVT_ERROR ||
			st == PLAYER_EVT_PLAYING || st == PLAYER_EVT_PLAYBACK_COMPLETE) {
		tts_player_t *handle = s_tts_player;
		if (handle->m_play_state != st) {
			handle->m_play_state = st;
		}
		if (st == PLAYER_EVT_PLAYING) {
			assist_controller_trigger_event(CONTROLLER_EVENT_STATE_TTS_PLAY_START, NULL, 0);
		}
		if (st == PLAYER_EVT_STOPED || st == PLAYER_EVT_PLAYBACK_COMPLETE ||
				st == PLAYER_EVT_ERROR) {
			evs_handler_post_runnable(_handle_tts_play_end, NULL);
			assist_controller_trigger_event(CONTROLLER_EVENT_STATE_TTS_PLAY_FINISH, NULL, 0);
		}
	}
	return 0;
}

static void _tts_play_next(tts_player_t *handle)
{
	audio_out_t *item = NULL;
	if (handle->m_item != NULL) {
		item = handle->m_item;
		handle->m_item = NULL;
	} else if (handle->m_ask_item != NULL) {
		item = handle->m_ask_item;
		handle->m_ask_item = NULL;
	}
	if (item != NULL) {
		LISA_LOGD(TAG, "ttsplayer play %s", item->m_url);
		memcpy(handle->m_curr_item, item, sizeof(audio_out_t));
		// app_player_play(PLAYER_T_TONE, item->m_url);
		// printk("tts start %lld\n", lisa_os_get_tick_ms());
		app_player_play_by_throw(PLAYER_T_TONE, item->m_url, item->throw_time, _tts_on_play_state);
		lisa_mem_free(item);
	}
}

static void _tts_force_stop(tts_player_t *handle)
{
	if (handle->m_play_state == PLAYER_EVT_PLAYING) {
		LISA_LOGD(TAG, "ttsplayer stop when playing");
		app_player_stop_sync(PLAYER_T_TONE);
	} else if (handle->m_play_state == APP_PLAYER_PREPARING) {
		LISA_LOGD(TAG, "ttsplayer stop when preparing");
		app_player_stop_sync(PLAYER_T_TONE);
	} else if (handle->m_play_state == PLAYER_EVT_PREPARED) {
		LISA_LOGD(TAG, "ttsplayer stop when prepared");
		app_player_reset(PLAYER_T_TONE);
	}
}

static void _tts_play_foreground(tts_player_t *handle)
{
	// if (handle->m_play_state != PLAYER_EVT_PLAYING) {
		_tts_play_next(handle);
	// }
}

static void _tts_play_background(tts_player_t *handle)
{
	LISA_LOGD(TAG, "play background, state %d", handle->m_play_state);
	_tts_force_stop(handle);
}

static void _tts_on_focus_state(focus_state_e focus_state, channel_type_e by_which)
{
	LISA_LOGI(TAG, "focus %d, by_which %d", focus_state, by_which);
	printk("***** tts focus %d, by_which %d\r\n", focus_state, by_which);
	if (s_tts_player->m_focus_state != focus_state) {
		s_tts_player->m_focus_state = focus_state;

		if (focus_state == NONE) {
			if (s_tts_player->m_item != NULL) {
				lisa_mem_free(s_tts_player->m_item);
				s_tts_player->m_item = NULL;
			}
			if (s_tts_player->m_play_state != PLAYER_EVT_STOPED &&
				s_tts_player->m_play_state != PLAYER_EVT_PLAYBACK_COMPLETE &&
				s_tts_player->m_play_state != PLAYER_EVT_ERROR) {
				_tts_force_stop(s_tts_player);
			}
		} else if (focus_state == FOREGROUND) {
			_tts_play_foreground(s_tts_player);
		} else if (focus_state == BACKGROUND) {
			_tts_play_background(s_tts_player);
		}
		if (s_tts_player->m_focus_cb) {
			s_tts_player->m_focus_cb->on_focus_state(focus_state, by_which);
		}
	}
}

static channel_callback_cb s_channel_cb = {
		.on_focus_state = _tts_on_focus_state,
		.m_channel_type = TTS,
};

static void _tts_on_directive(tts_player_t *handle, audio_out_t *item, bool interrupt)
{
	LISA_LOGD(TAG, "tts_on_directive, state %d, interrupt %d", handle->m_play_state, interrupt);
	if (handle->m_item != NULL) {
		lisa_mem_free(handle->m_item);
		handle->m_item = NULL;
	}
	// TTS 在播放中, 不能打断的场景
	if (!interrupt && handle->m_play_state == PLAYER_EVT_PLAYING) {
		handle->m_item = (audio_out_t *)lisa_mem_alloc(sizeof(audio_out_t));
		memcpy(handle->m_item, item, sizeof(audio_out_t));
		LISA_LOGD(TAG, "current tts is playing, save audio info");
		return;
	}
	// 停止TTS
	handle->stop(handle);
	// 复制 Item
	handle->m_item = (audio_out_t *)lisa_mem_alloc(sizeof(audio_out_t));
	memcpy(handle->m_item, item, sizeof(audio_out_t));

	if (handle->m_focus_state == NONE) {
		_tts_acquire_channel(handle);
	}
}

static void _tts_on_reply_ask(tts_player_t *handle, audio_out_t *item)
{
	if (handle->m_ask_item == NULL) {
		handle->m_ask_item = (audio_out_t *)lisa_mem_alloc(sizeof(audio_out_t));
	}
	if (handle->m_ask_item != NULL) {
		memcpy(handle->m_ask_item, item, sizeof(audio_out_t));
	}
}

static void _tts_stop(tts_player_t *handle)
{
	_tts_force_stop(handle);
}

tts_player_t *listen_ttsplayer_create(listen_audiomgr_t *audio_mgr)
{
	tts_player_t *handle = (tts_player_t *)lisa_mem_calloc(1, sizeof(tts_player_t));

	handle->m_audio_mgr = audio_mgr;
	handle->m_channel_name = TTS;
	handle->m_player_name = "tts_player";
	handle->m_item = NULL;
	handle->m_focus_state = NONE;
	handle->m_play_state = PLAYER_EVT_ERROR;
	handle->m_focus_cb = NULL;

	handle->m_curr_item = (audio_out_t *)lisa_mem_alloc(sizeof(audio_out_t));

	listen_audiomgr_add_channel_callback(handle->m_audio_mgr, &s_channel_cb);

	handle->stop = _tts_stop;
	handle->on_directive = _tts_on_directive;
	handle->on_reply_ask = _tts_on_reply_ask;

	s_tts_player = handle;
	return handle;
}

void listen_ttsplayer_destroy(tts_player_t *handle)
{
	if (handle != NULL) {
		if (handle->m_item != NULL) {
			lisa_mem_free(handle->m_item);
			handle->m_item = NULL;
		}
		if (handle->m_curr_item != NULL) {
			lisa_mem_free(handle->m_curr_item);
			handle->m_curr_item = NULL;
		}
		lisa_mem_free(handle);
	}
}

void listen_ttsplayer_add_focus_callback(tts_player_t *handle, tts_focus_callback_t *callback)
{
	handle->m_focus_cb = callback;
}
