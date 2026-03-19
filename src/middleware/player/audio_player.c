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
#include "player_mgr.h"

static audioplayer_t *s_audio_player = NULL;

static int _play_callback(uint16_t st);
static int _audio_on_play_end(void *player_st_ptr);

static void _audioplayer_acquire_channel(audioplayer_t *handle)
{
	LISA_LOGD(TAG, "_audioplayer_acquire_channel");
	listen_audiomgr_acquire_channel(handle->m_audio_mgr, handle->m_id);
}

static void _audioplayer_release_channel(audioplayer_t *handle)
{
	if (handle->m_focus_state != FOCUS_NONE) {
		listen_audiomgr_release_channel(handle->m_audio_mgr, handle->m_id);
	}
}

#ifdef LISTEN_CLOUD

__attribute__((weak)) void ls_req_url(const char *item_id, char *url)
{
    int r;
    r = lsc_music_request_url(item_id, url);
    if (r != 0) {
        LISA_LOGI(TAG, "ls_req_url failed, r=%d, id:%s", r, item_id);
    } else {
        LISA_LOGI(TAG, "ls_req_url success, r=%d, id:%s, url:%s", r, item_id, url);
    }
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
		if (handle->m_focus_state == FOCUS_NONE || handle->m_focus_state == BACKGROUND) {
			s_audio_player->m_is_pause_called = false;
			s_audio_player->m_is_need_play_after_op = true;
			_audioplayer_acquire_channel(handle);
		}
		app_player_play(PLAYER_T_CLOUD, item->m_url, _play_callback);
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
		LISA_LOGD(TAG, "_audio_play_pre url=%s", item->m_url);
		// 播放之前重置下状态
		handle->m_player_state = APP_PLAYER_PREPARING;
		if (handle->m_focus_state == FOCUS_NONE || handle->m_focus_state == BACKGROUND) {
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
	} else if (handle->m_player_state == APP_PLAYER_PREPARING ||
		   handle->m_player_state == PLAYER_EVT_PREPARED) {
		LISA_LOGD(TAG, "skip replay when preparing/prepared");
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

static void _audio_on_focus_state(focus_state_e focus_state, int by_which)
{
	PlayerEvt state = s_audio_player->m_player_state;
	LISA_LOGD(TAG, "focus %d, by_which %d", focus_state, by_which);
	if (s_audio_player->m_focus_state != focus_state) {
		s_audio_player->m_focus_state = focus_state;
		if (s_audio_player->m_is_pause_called) {
			_audioplayer_release_channel(s_audio_player);
		} else if (focus_state == FOCUS_NONE) {
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
			player_config_t *config = player_mgr_get_config(by_which);
			if (config != NULL && config->fg_action == PLAY_ACTION_RECOGNIZE) {
				// 识别结束先不主动恢复播放
			} else if (s_audio_player->m_is_need_play_after_op) {
				s_audio_player->m_is_need_play_after_op = false;
			} else {
				_audio_play_foreground(s_audio_player);
			}
		} else if (focus_state == BACKGROUND) {
			_audio_play_background(s_audio_player);
		}
		// 调用焦点回调
		if (s_audio_player->m_callback && s_audio_player->m_callback->on_focus_state) {
			s_audio_player->m_callback->on_focus_state(focus_state, by_which);
		}
		LISA_LOGD(TAG, "_audio_on_focus_state end");
	}
}

static bool _audio_on_directive(audioplayer_t *handle, audio_play_cmd cmd, audio_out_t *item, uint32_t item_size)
{
	if (cmd == AUDIO_PLAY) {
		LISA_LOGD(TAG, "_audio_on_directive play");
		s_audio_player->m_is_pause_called = false;
		handle->stop(handle, true);

		listen_playlist_init(handle->m_play_mode, item, item_size);
		
		if (handle->m_focus_state == FOCUS_NONE) {
			_audioplayer_acquire_channel(handle);
		} else if (handle->m_focus_state == FOREGROUND) {
			_audio_play_next(handle, false);
		}
	}
	return true;
}

static int _audio_on_play_end(void *arg)
{
	_audio_play_next(s_audio_player, false);
	return 0;
}

static void _audio_pause(audioplayer_t *handle)
{
	handle->m_is_pause_called = true;

	if (handle->m_player_state == PLAYER_EVT_PLAYING) {
		app_player_pause(PLAYER_T_CLOUD);
		LISA_LOGD(TAG, "audioplayer wait paused ...");
		lisa_semaphore_take(handle->pause_sema, AUDIOPLAYER_PAUSE_WAIT_TIMEOUT);
		LISA_LOGD(TAG, "audioplayer has paused");
	} else if (handle->m_player_state == PLAYER_EVT_PAUSED) {
		LISA_LOGD(TAG, "ready to pause");
	} else if (handle->m_player_state == APP_PLAYER_PREPARING) {
		LISA_LOGD(TAG, "audioplayer pause when preparing by user");
		app_player_pause(PLAYER_T_CLOUD);
	} else if (handle->m_player_state == PLAYER_EVT_PREPARED) {
		LISA_LOGD(TAG, "audioplayer pause when prepared by user");
		app_player_pause(PLAYER_T_CLOUD);
	} else {
		LISA_LOGD(TAG, "audioplayer pause requested in state %d", handle->m_player_state);
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
		if (handle->m_focus_state == FOCUS_NONE || handle->m_focus_state == BACKGROUND) {
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
	if (handle->m_focus_state == FOCUS_NONE || handle->m_focus_state == BACKGROUND) {
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

void listen_audioplayer_pause(audioplayer_t *handle)
{
	_audio_pause(handle);
}

void listen_audioplayer_pause_temp(audioplayer_t *handle)
{
	_audio_play_background(handle);
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
			// 调用状态回调
			if (handle->m_callback && handle->m_callback->on_play_state) {
				handle->m_callback->on_play_state(st);
			}
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

audioplayer_t *listen_audioplayer_create(audio_player_config_t *config, listen_audiomgr_t *audio_mgr)
{
	if (config == NULL || audio_mgr == NULL) {
		LISA_LOGE(TAG, "Invalid parameters");
		return NULL;
	}

	if (s_audio_player != NULL) {
		LISA_LOGW(TAG, "Audio player already created");
		return s_audio_player;
	}

	audioplayer_t *handle = (audioplayer_t *)lisa_mem_calloc(1, sizeof(audioplayer_t));

	handle->m_audio_mgr = audio_mgr;
	handle->m_id = config->id;
	handle->m_name = config->name;

	handle->m_is_need_play_after_op = false;
	handle->m_is_pause_called = false;

	handle->m_focus_state = FOCUS_NONE;
	handle->m_player_state = PLAYER_EVT_ERROR;

	handle->pause_sema = lisa_semaphore_create(2);

	handle->m_play_mode = listen_play_mode_create();

	// 注册到焦点管理器
	int ret = listen_audiomgr_register_channel(
		audio_mgr,
		config->id,
		config->name,
		config->priority,
		config->capture_ids,
		config->capture_count,
		_audio_on_focus_state
	);

	if (ret != 0) {
		LISA_LOGE(TAG, "Failed to register channel");
		listen_play_mode_destory(handle->m_play_mode);
		if (handle->pause_sema != NULL) {
			lisa_semaphore_delete(handle->pause_sema);
		}
		lisa_mem_free(handle);
		return NULL;
	}

	handle->pause = _audio_pause;
	handle->resumeByVoice = _audio_resume_by_voice;
	handle->stop = _audio_stop;
	handle->next = _audio_next;
	handle->prev = _audio_prev;
	handle->on_directive = _audio_on_directive;
	handle->replay = _audio_replay;

	s_audio_player = handle;

	LISA_LOGI(TAG, "Audio player created: id=%d, name=%s", config->id, config->name);

	return handle;
}

void listen_audioplayer_destroy(audioplayer_t *handle)
{
	if (handle != NULL) {
		_audio_clear_queue(handle);
		if (handle->pause_sema != NULL) {
			lisa_semaphore_delete(handle->pause_sema);
		}
		if (handle->m_play_mode != NULL) {
			listen_play_mode_destory(handle->m_play_mode);
		}
		lisa_mem_free(handle);
		s_audio_player = NULL;
	}
}

void listen_audioplayer_add_callback(audioplayer_t *handle, audio_player_callback_t *callback)
{
	if (handle) {
		handle->m_callback = callback;
	}
}

PlayerEvt listen_audioplayer_get_state(audioplayer_t *handle)
{
	return handle->m_player_state;
}

int listen_audioplayer_switch_mode(audioplayer_t *handle, PLAY_MODE_E mode)
{
	if (handle == NULL) {
		return -1;
	}
	return listen_switch_playmode(handle->m_play_mode, mode);
}
