#include "sound_player.h"
#include <stdlib.h>
#include <string.h>
#include "evs_utils.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "audio_out.h"
#include "app_player.h"

#define TAG "soundplayer"

// ============== 多实例管理 ==============

#define MAX_SOUND_PLAYER_COUNT 8

static sound_player_t *s_players[MAX_SOUND_PLAYER_COUNT] = {NULL};
static int s_player_count = 0;
static int s_current_id = 0;

static int _register_player(sound_player_t *player)
{
	if (s_player_count >= MAX_SOUND_PLAYER_COUNT) {
		return -1;
	}
	s_players[s_player_count++] = player;
	return 0;
}

static sound_player_t *_get_current_player(void)
{
	for (int i = 0; i < s_player_count; i++) {
		if (s_players[i] != NULL && s_players[i]->m_id == s_current_id) {
			return s_players[i];
		}
	}
	return NULL;
}

// ============== 内部辅助函数 ==============

static void __release_item(sound_player_t *handle)
{
	if (handle) {
		if (handle->m_item != NULL) {
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
	LISA_LOGD(TAG, "[%s] _sound_play_next, handle->m_wait_len:%d", handle->m_player_name, handle->m_wait_len);
	if (handle->m_wait_len > 0) {
		handle->m_wait_len--;
		memcpy(handle->m_curr_item, &(handle->m_item)[handle->m_wait_len], sizeof(audio_out_t));
		if (handle->m_wait_len == 0) {
			lisa_mem_free(handle->m_item);
			handle->m_item = NULL;
		}
		LISA_LOGD(TAG, "[%s] play next url %s", handle->m_player_name, handle->m_curr_item->m_url);
		s_current_id = handle->m_id;
		app_player_play_by_throw(PLAYER_T_TONE, handle->m_curr_item->m_url, handle->m_curr_item->throw_time, _sound_on_play_state);
	}
}

static void _sound_play_background(sound_player_t *handle) 
{
	// 背景动作：停止
	LISA_LOGD(TAG, "[%s] play background, state %d", handle->m_player_name, handle->m_play_state);
	handle->m_wait_len = 0;
	if (handle->m_play_state != PLAYER_EVT_STOPED &&
			handle->m_play_state != PLAYER_EVT_PLAYBACK_COMPLETE &&
			handle->m_play_state != PLAYER_EVT_ERROR) {
		app_player_stop_sync(PLAYER_T_TONE);
	}
}

static void _sound_play_foreground(sound_player_t *handle)
{
	LISA_LOGD(TAG, "[%s] _sound_play_foreground, handle->m_play_state:%d", handle->m_player_name, handle->m_play_state);
	if (handle->m_play_state != PLAYER_EVT_PLAYING) {
		_sound_play_next(handle);
	}
}

static void _sound_acquire_channel(sound_player_t *handle)
{
	LISA_LOGD(TAG, "[%s] sound acquire channel", handle->m_player_name);
	handle->m_play_after_stop = true;
	listen_audiomgr_acquire_channel(handle->m_audio_mgr, handle->m_id);
}

static void _sound_release_channel(sound_player_t *handle)
{
	if (handle->m_focus_state != FOCUS_NONE) {
		listen_audiomgr_release_channel(handle->m_audio_mgr, handle->m_id);
	}
}

// ============== 焦点变化处理（多实例） ==============

typedef struct {
	sound_player_t *player;
} focus_cb_context_t;

static focus_cb_context_t s_focus_contexts[MAX_SOUND_PLAYER_COUNT];

static void _on_focus_change(int index, focus_state_e focus_state, int by_which)
{
	sound_player_t *handle = s_focus_contexts[index].player;
	if (handle == NULL) {
		return;
	}

	LISA_LOGI(TAG, "[%s] focus %s, by_which %d", handle->m_player_name,
			  listen_audiomgr_get_state_name(focus_state), by_which);

	if (handle->m_focus_state != focus_state) {
		handle->m_focus_state = focus_state;

		if (focus_state == FOCUS_NONE) {
			handle->m_wait_len = 0;
			if (handle->m_item != NULL) {
				lisa_mem_free(handle->m_item);
				handle->m_item = NULL;
			}
			if (handle->m_play_state != PLAYER_EVT_STOPED &&
					handle->m_play_state != PLAYER_EVT_PLAYBACK_COMPLETE &&
					handle->m_play_state != PLAYER_EVT_ERROR) {
				app_player_stop_sync(PLAYER_T_TONE);
			}
		} else if (focus_state == FOREGROUND) {
			_sound_play_foreground(handle);
		} else if (focus_state == BACKGROUND) {
			_sound_play_background(handle);
		}

		if (handle->m_callback && handle->m_callback->on_focus_state) {
			handle->m_callback->on_focus_state(focus_state, by_which);
		}
	}
}

// 为每个播放器生成独立的焦点回调
#define DEFINE_FOCUS_CALLBACK(n) \
	static void _focus_cb_##n(focus_state_e state, int by_which) { \
		_on_focus_change(n, state, by_which); \
	}

DEFINE_FOCUS_CALLBACK(0)
DEFINE_FOCUS_CALLBACK(1)
DEFINE_FOCUS_CALLBACK(2)
DEFINE_FOCUS_CALLBACK(3)
DEFINE_FOCUS_CALLBACK(4)
DEFINE_FOCUS_CALLBACK(5)
DEFINE_FOCUS_CALLBACK(6)
DEFINE_FOCUS_CALLBACK(7)

static void (*s_focus_callbacks[MAX_SOUND_PLAYER_COUNT])(focus_state_e, int) = {
	_focus_cb_0, _focus_cb_1, _focus_cb_2, _focus_cb_3,
	_focus_cb_4, _focus_cb_5, _focus_cb_6, _focus_cb_7,
};

// ============== 播放器回调 ==============

static int _handle_sound_play_end(void *user_data) 
{
	sound_player_t *handle = (sound_player_t *)user_data;
	if (handle == NULL) {
		return 0;
	}

	if (!handle->m_play_after_stop || handle->m_wait_len <= 0) {
		_sound_release_channel(handle);
	} else {
		_sound_play_next(handle);
	}
	return 0;
}

static int _sound_on_play_state(uint16_t st)
{
	// 查找当前前景的播放器
	sound_player_t *handle = _get_current_player();
	if (handle == NULL) {
		return 0;
	}

	LISA_LOGD(TAG, "[%s] soundplayer status: %#X", handle->m_player_name, st);
	if (st == APP_PLAYER_PREPARING || st == PLAYER_EVT_STOPED || st == PLAYER_EVT_ERROR ||
			st == PLAYER_EVT_PLAYING || st == PLAYER_EVT_PLAYBACK_COMPLETE) {
		if (handle->m_play_state != st) {
			handle->m_play_state = st;
			// 调用状态回调
			if (handle->m_callback && handle->m_callback->on_play_state) {
				handle->m_callback->on_play_state(st);
			}

			if (st == PLAYER_EVT_STOPED || st == PLAYER_EVT_PLAYBACK_COMPLETE ||
					st == PLAYER_EVT_ERROR) {
				evs_handler_post_runnable(_handle_sound_play_end, handle);
			}
		} else {
			// Fix: 第一次出错的情况下, 后面再出错, 不能释放焦点
			if (st == PLAYER_EVT_ERROR) {
				_sound_release_channel(handle);
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
		LISA_LOGD(TAG, "[%s] stop when preparing", handle->m_player_name);
		handle->m_play_after_stop = play_after_stop;
		app_player_stop_sync(PLAYER_T_TONE);
	} else if (handle->m_play_state == PLAYER_EVT_PREPARED) {
		LISA_LOGD(TAG, "[%s] sound reset when prepared", handle->m_player_name);
		app_player_reset(PLAYER_T_TONE);
	}
}

// ============== 公开接口实现 ==============

sound_player_t *listen_soundplayer_create(sound_player_config_t *config, listen_audiomgr_t *audio_mgr)
{
	if (config == NULL || audio_mgr == NULL) {
		LISA_LOGE(TAG, "Invalid parameters");
		return NULL;
	}

	if (s_player_count >= MAX_SOUND_PLAYER_COUNT) {
		LISA_LOGE(TAG, "Max player count reached");
		return NULL;
	}

	sound_player_t *handle = (sound_player_t *)lisa_mem_calloc(1, sizeof(sound_player_t));
	if (handle == NULL) {
		LISA_LOGE(TAG, "Failed to allocate player");
		return NULL;
	}

	handle->m_audio_mgr = audio_mgr;
	handle->m_id = config->id;
	handle->m_player_name = config->name;

	handle->m_play_after_stop = false;
	handle->m_curr_item = (audio_out_t *)lisa_mem_alloc(sizeof(audio_out_t));
	handle->m_wait_len = 0;
	handle->m_item = NULL;

	handle->m_focus_state = FOCUS_NONE;
	handle->m_play_state = PLAYER_EVT_ERROR;
	handle->m_callback = NULL;

	// 保存回调上下文
	int idx = s_player_count;
	s_focus_contexts[idx].player = handle;

	// 注册到焦点管理器
	int ret = listen_audiomgr_register_channel(
		audio_mgr,
		config->id,
		config->name,
		config->priority,
		config->capture_ids,
		config->capture_count,
		s_focus_callbacks[idx]
	);

	if (ret != 0) {
		LISA_LOGE(TAG, "Failed to register channel");
		if (handle->m_curr_item) lisa_mem_free(handle->m_curr_item);
		lisa_mem_free(handle);
		return NULL;
	}

	handle->stop = _sound_stop;

	_register_player(handle);

	LISA_LOGI(TAG, "Sound player created: id=%d, name=%s", config->id, config->name);

	return handle;
}

void listen_soundplayer_destroy(sound_player_t *handle)
{
	if (handle != NULL) {
		__release_item(handle);
		if (handle->m_curr_item) lisa_mem_free(handle->m_curr_item);
		lisa_mem_free(handle);
	}
}

void listen_soundplayer_play(sound_player_t *handle, char *path, bool print_log)
{
	if (print_log) LISA_LOG(TAG, "[%s] play: %s", handle->m_player_name, path);
	if (path == NULL) {
		LISA_LOGW(TAG, "[%s] path is NULL", handle->m_player_name);
		return;
	}

	__release_item(handle);
	audio_out_t item;
	memset(&item, 0, sizeof(audio_out_t));
	strcpy(item.m_url, path);
	_item_backup(handle, &item, 1);
	handle->stop(handle, true);
	if (handle->m_focus_state == FOCUS_NONE) {
		_sound_acquire_channel(handle);
	}
}

void listen_soundplayer_play_item(sound_player_t *handle, audio_out_t *item, bool interrupt)
{
	if (handle == NULL || item == NULL) {
		return;
	}

	LISA_LOGD(TAG, "[%s] Play item: %s, interrupt=%d", handle->m_player_name, item->m_url, interrupt);

	// 非打断模式且正在播放，保存到队列
	if (!interrupt && handle->m_play_state == PLAYER_EVT_PLAYING) {
		// 备份单个项到队列
		__release_item(handle);
		_item_backup(handle, item, 1);
		LISA_LOGD(TAG, "[%s] Save to queue, current playing", handle->m_player_name);
		return;
	}

	// 打断模式
	__release_item(handle);
	_item_backup(handle, item, 1);
	handle->stop(handle, true);
	if (handle->m_focus_state == FOCUS_NONE) {
		_sound_acquire_channel(handle);
	} else if (handle->m_focus_state == FOREGROUND) {
		_sound_play_next(handle);
	}
}

void listen_soundplayer_play_array(sound_player_t *handle, audio_out_t *items, uint8_t len)
{
	__release_item(handle);
	handle->m_wait_len = (len <= LS_SOUND_AUDIO_MAX_COUNT)? len:LS_SOUND_AUDIO_MAX_COUNT;
		
	// 循环赋值音频结构体
	_item_backup(handle, items, len);

	/** 
	 * Note: The prompt tone will not play correctly 
	 * if it is in the PLAYER_EVT_PLAYING state.
	 */
	if (handle->m_play_state == PLAYER_EVT_PLAYING) {
		LISA_LOGW(TAG, "[%s] play_array when playing, state:%d", handle->m_player_name, handle->m_play_state);
		handle->m_play_state = PLAYER_EVT_ERROR;
	}
	handle->stop(handle, true);
	if (handle->m_focus_state == FOCUS_NONE) {
		_sound_acquire_channel(handle);
	}
}

void listen_soundplayer_add_callback(sound_player_t *handle, sound_player_callback_t *callback)
{
	if (handle) {
		handle->m_callback = callback;
	}
}
