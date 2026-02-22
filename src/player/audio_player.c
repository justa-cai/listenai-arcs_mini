#define TAG "audioplayer"

#include "audio_player.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "evs_utils.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_typedef.h"
#include "lisa_thread.h"
#include "app_player.h"
#include "assistant_controller.h"
#include "player/music_manager.h"


static audioplayer_t *s_audio_player = NULL;

static int _play_callback(uint16_t st);
static int _audio_on_play_end(void *player_st_ptr);

static void _audioplayer_acquire_channel(audioplayer_t *handle)
{
	LISA_LOGD(TAG, "_audioplayer_acquire_channel");
	listen_audiomgr_acquire_channel(handle->m_audio_mgr, handle->m_channel_name);
}

static void _audioplayer_release_channel(audioplayer_t *handle)
{
	if (handle->m_focus_state != NONE) {
		listen_audiomgr_release_channel(handle->m_audio_mgr, handle->m_channel_name);
	}
}

#ifdef LISTEN_CLOUD

__attribute__((weak)) void ls_req_url(const char *item_id, char *url)
{
	LISA_LOGD(TAG, "ls_req_url");
}

#endif

static void _audio_play_next(audioplayer_t *handle, bool force)
{
	int code = -1;
	audio_out_t *item = listen_next_audio(handle->m_play_mode, &code);
	LISA_LOGD(TAG, "_audio_play_next code %d, force %d", code, force);
	if (code == -1) {
		LISA_LOGD(TAG, "_audio_play_next fail by list is empty");
		_audioplayer_release_channel(handle);
		assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_PLAY_STOP, NULL, 0);
		return;
	}
	if (item != NULL && (code == 0 || (code == 1 && force))) {
#ifdef LISTEN_CLOUD
		if (item->m_url[0] == '\0') {
			// 请求url
			ls_req_url(item->mid, item->m_url);
			if (item->m_url[0] == '\0') {
				LISA_LOGI(TAG, "request url failed");
				_audioplayer_release_channel(handle);
				return;
			}
		}
	#endif
		LISA_LOGD(TAG, "_audio_play_next url=%s", item->m_url);
		// 播放之前重置下状态
		handle->m_player_state = APP_PLAYER_PREPARING;
		if (handle->m_focus_state == NONE || handle->m_focus_state == BACKGROUND) {
			s_audio_player->m_is_pause_called = false;
			s_audio_player->m_is_need_play_after_op = true;
			_audioplayer_acquire_channel(handle);
		}
		app_player_play(PLAYER_T_CLOUD, item->m_url, _play_callback);
		assistant_controller_event_update_music_payload_t payload = {
			.name = item->m_name,
			.artist = item->m_artist,
			.image_url = item->m_image_url,
			.image_dsc = item->m_image_dsc
		};
		assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_UPDATE_MUSIC, &payload, sizeof(assistant_controller_event_update_music_payload_t));
	}
	else{
		assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_PLAY_STOP, NULL, 0);
	}
}

static void _audio_play_pre(audioplayer_t *handle, bool force)
{
	int code = -1;
	audio_out_t *item = listen_pre_audio(handle->m_play_mode, &code);
	LISA_LOGD(TAG, "_audio_play_pre code %d, force %d", code, force);
	if (code == -1) {
		LISA_LOGD(TAG, "_audio_play_pre fail by list is empty");
		_audioplayer_release_channel(handle);
		return;
	}
	if (item != NULL && (code == 0 || (code == 1 && force))) {
		LISA_LOGD(TAG, "_audio_play_pre url=%s", item->m_url);
		// 播放之前重置下状态
		handle->m_player_state = APP_PLAYER_PREPARING;
		if (handle->m_focus_state == NONE || handle->m_focus_state == BACKGROUND) {
			s_audio_player->m_is_pause_called = false;
			s_audio_player->m_is_need_play_after_op = true;
			_audioplayer_acquire_channel(handle);
		}
		app_player_play(PLAYER_T_CLOUD, item->m_url, _play_callback);
	}
}

static void _audio_play_foreground(audioplayer_t *handle)
{
	LISA_LOGD(TAG, "play foreground, state %d", handle->m_player_state);
	if (handle->m_player_state == PLAYER_EVT_PAUSED) {
		app_player_resume_sync(PLAYER_T_CLOUD);
		handle->m_player_state = PLAYER_EVT_PLAYING;
	} else if (handle->m_player_state != PLAYER_EVT_PLAYING) {
		_audio_play_next(handle, false);
	}
}

#define AUDIOPLAYER_PAUSE_WAIT_TIMEOUT (1000)
static void _audio_play_background(audioplayer_t *handle)
{
	LISA_LOGD(TAG, "play background, state %d", handle->m_player_state);
	if (handle->m_player_state == PLAYER_EVT_PLAYING) {
		app_player_pause(PLAYER_T_CLOUD);
		LISA_LOGD(TAG, "audioplayer wait paused ...");
		lisa_semaphore_take(handle->pause_sema, AUDIOPLAYER_PAUSE_WAIT_TIMEOUT);
		LISA_LOGD(TAG, "audioplayer has paused");
	} else if(handle->m_player_state == APP_PLAYER_PREPARING) {
		LISA_LOGD(TAG, "audioplayer pause when preparing");
		app_player_pause(PLAYER_T_CLOUD);
	} else if(handle->m_player_state == PLAYER_EVT_PREPARED) {
		LISA_LOGD(TAG, "audioplayer pause when prepared");
		app_player_pause(PLAYER_T_CLOUD);
	}
}

static void _audio_clear_queue(audioplayer_t *handle)
{
	listen_playlist_deinit(handle->m_play_mode);
}

static void _audio_on_focus_state(focus_state_e focus_state, channel_type_e by_which)
{
	PlayerEvt state = s_audio_player->m_player_state;
	LISA_LOGD(TAG, "focus %d, by_which %d, player_state=%d", focus_state, by_which, state);
	if (s_audio_player->m_focus_state != focus_state) {
		s_audio_player->m_focus_state = focus_state;
		if (s_audio_player->m_is_pause_called) {
			_audioplayer_release_channel(s_audio_player);
		} else if (focus_state == NONE) {
			if (!s_audio_player->m_is_pause_called) {
				if (state == PLAYER_EVT_PLAYING) {
					app_player_stop_sync(PLAYER_T_CLOUD);
				}
			} else {
				s_audio_player->m_is_pause_called = false;
			}
			if (state == PLAYER_EVT_PLAYING) {
				app_player_stop_sync(PLAYER_T_CLOUD);
			}
		} else if (focus_state == FOREGROUND) {
			if (by_which == AIP) {
			} else if (s_audio_player->m_is_need_play_after_op) {
				/* TTS completed, resume music playback */
				LISA_LOGI(TAG, "Resuming music playback after TTS (state=%d)", state);
				s_audio_player->m_is_need_play_after_op = false;
				_audio_play_foreground(s_audio_player);
			} else {
				_audio_play_foreground(s_audio_player);
			}
		} else if (focus_state == BACKGROUND) {
			_audio_play_background(s_audio_player);
		}
		LISA_LOGD(TAG, "_audio_on_focus_state end");
	}
}

static channel_callback_cb s_channel_cb = {
		.on_focus_state = _audio_on_focus_state,
		.m_channel_type = CONTENT,
};

static bool _audio_on_directive(audioplayer_t *handle, audio_play_cmd cmd, audio_out_t *item, uint32_t item_size)
{
	if (cmd == AUDIO_PLAY) {
		LISA_LOGD(TAG, "_audio_on_directive play");
		s_audio_player->m_is_pause_called = false;
		// _audio_clear_queue(handle);
		handle->stop(handle, true);

		listen_playlist_init(handle->m_play_mode, item, item_size);
		
		if (handle->m_focus_state == NONE) {
			_audioplayer_acquire_channel(handle);
		} else if (handle->m_focus_state == FOREGROUND) {
			_audio_play_next(handle, false);
		}
		assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_PLAY_START, NULL, 0);
	}
	return true;
}

static int _audio_on_play_end(void *arg)
{
	// Check if music manager is active for auto-next
	if (music_manager_is_active()) {
		LISA_LOGI(TAG, "Auto-playing next song via music manager");
		music_manager_play_next_random(NULL, 0);
	} else {
		// Fall back to playlist-based playback
		LISA_LOGI(TAG, "Playing next from playlist");
		_audio_play_next(s_audio_player, false);
	}
	return 0;
}

static void _audio_pause(audioplayer_t *handle)
{
	if (handle->m_player_state == PLAYER_EVT_PLAYING) {
		handle->m_is_pause_called = true;
		app_player_pause(PLAYER_T_CLOUD);
		LISA_LOGD(TAG, "audioplayer wait paused ...");
		lisa_semaphore_take(handle->pause_sema, AUDIOPLAYER_PAUSE_WAIT_TIMEOUT);
		LISA_LOGD(TAG, "audioplayer has paused");
	} else if (handle->m_player_state == PLAYER_EVT_PAUSED) {
		handle->m_is_pause_called = true;
		LISA_LOGD(TAG, "ready to pause");
		// _audioplayer_release_channel(handle);
	}
}

static void _audio_stop(audioplayer_t *handle, bool play_after_stop)
{
	if (handle->m_player_state == PLAYER_EVT_PLAYING || handle->m_player_state == PLAYER_EVT_PAUSED) {
		app_player_stop_sync(PLAYER_T_CLOUD);
	}
}

void audio_player_stop_by_user(void)
{
	if (s_audio_player) {
		_audio_stop(s_audio_player, false);
		_audioplayer_release_channel(s_audio_player);
	}
}

static void _audio_next(audioplayer_t *handle)
{
	app_player_stop_sync(PLAYER_T_CLOUD);
	_audio_play_next(handle, true);
}

static void _audio_prev(audioplayer_t *handle)
{
	app_player_stop_sync(PLAYER_T_CLOUD);
	_audio_play_pre(handle, true);
}

static void _audio_replay(audioplayer_t *handle)
{
	app_player_stop_sync(PLAYER_T_CLOUD);
	audio_out_t *item = listen_get_curr_audio(handle->m_play_mode);
	if (item != NULL) {
#ifdef LISTEN_CLOUD
		if (item->m_url[0] == '\0') {
			// 请求url
			ls_req_url(item->mid, item->m_url);
			if (item->m_url[0] == '\0') {
				LISA_LOGI(TAG, "request url failed");
				_audioplayer_release_channel(handle);
				return;
			}
		}
#endif
		LISA_LOGD(TAG, "_audio_replay url=%s", item->m_url);
		if (handle->m_focus_state == NONE || handle->m_focus_state == BACKGROUND) {
			s_audio_player->m_is_pause_called = false;
			s_audio_player->m_is_need_play_after_op = true;
			_audioplayer_acquire_channel(handle);
		}
		app_player_play(PLAYER_T_CLOUD, item->m_url, _play_callback);
	}
}

static void _audio_resume_by_voice(audioplayer_t *handle)
{
	s_audio_player->m_is_pause_called = false;
	if (handle->m_focus_state == NONE || handle->m_focus_state == BACKGROUND) {
		_audioplayer_acquire_channel(handle);
	} else if (handle->m_focus_state == FOREGROUND) {
		_audio_play_foreground(s_audio_player);
	}
}

void listen_audioplayer_resume(audioplayer_t *handle)
{
	if (handle->m_focus_state == FOREGROUND && handle->m_player_state == PLAYER_EVT_PAUSED &&
			!handle->m_is_pause_called) {
		_audio_play_foreground(s_audio_player);
	}
}

void listen_audioplayer_puse(audioplayer_t *handle)
{
	_audio_pause(handle);
}

static int _play_callback(uint16_t st)
{
	LISA_LOGD(TAG, "audioplayer state %d", st);
	if (st == PLAYER_EVT_STOPED || st == PLAYER_EVT_PAUSED ||
		st == APP_PLAYER_PREPARING || st == PLAYER_EVT_PLAYING ||
		st == PLAYER_EVT_PLAYBACK_COMPLETE || st == PLAYER_EVT_ERROR) {
		audioplayer_t *handle = s_audio_player;
		if (handle->m_player_state != st) {
			handle->m_player_state = st;
		}
		if (st == PLAYER_EVT_PLAYBACK_COMPLETE || st == PLAYER_EVT_ERROR) {
			evs_handler_post_runnable(_audio_on_play_end, NULL);
		} else if (st == PLAYER_EVT_PAUSED) {
			lisa_semaphore_give(handle->pause_sema);
		}

		if(st == PLAYER_EVT_PLAYING){
			// assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_PLAY_START, NULL, 0);
		}
		else if(st == PLAYER_EVT_STOPED){
			// assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_PLAY_STOP, NULL, 0);
		}
	}
	return 0;
}

audioplayer_t *listen_audioplayer_create(const listen_audiomgr_t *audio_mgr, const play_mode_t *play_mode)
{
	audioplayer_t *handle = (audioplayer_t *)lisa_mem_calloc(1, sizeof(audioplayer_t));

	handle->m_audio_mgr = (listen_audiomgr_t *)audio_mgr;
	handle->m_channel_name = CONTENT;
	handle->m_player_name = "content_player";

	handle->m_is_need_play_after_op = false;
	handle->m_is_pause_called = false;

	handle->m_focus_state = NONE;
	handle->m_player_state = PLAYER_EVT_ERROR;

	handle->pause_sema = lisa_semaphore_create(2);

	handle->m_play_mode = (play_mode_t *)play_mode;

	listen_audiomgr_add_channel_callback(handle->m_audio_mgr, &s_channel_cb);

	handle->pause = _audio_pause;
	handle->resumeByVoice = _audio_resume_by_voice;
	handle->stop = _audio_stop;
	handle->next = _audio_next;
	handle->prev = _audio_prev;
	handle->on_directive = _audio_on_directive;
	handle->replay = _audio_replay;
	s_audio_player = handle;
	return handle;
}

void listen_audioplayer_destroy(audioplayer_t *handle)
{
	if (handle != NULL) {
		_audio_clear_queue(handle);
		lisa_mem_free(handle);
	}
}

PlayerEvt listen_audioplayer_get_state(audioplayer_t *handle)
{
	return handle->m_player_state;
}
