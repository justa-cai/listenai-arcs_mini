#define TAG "shortplayer"

#include "short_player.h"
#include <stdlib.h>
#include <string.h>
#include "lisa_log.h"
#include "lisa_mem.h"
#include "app_tone.h"
#include "app_player.h"
#include "evs_utils.h"



static short_player_t *s_short_player = NULL;

static int _short_on_play_state(uint16_t st);

static void _short_play_next(short_player_t *handle)
{
	if (handle->m_wait_len > 0 && handle->m_cur_id >= 0) {
		handle->m_wait_len--;
		app_player_play_by_throw(PLAYER_T_TONE, app_tone_get_url(handle->m_cur_id), 100, _short_on_play_state);
	}
}

static int _play_state_func(void *user_data)
{
	uint16_t *state = (uint16_t *)(user_data);
	s_short_player->m_play_state = *state;
	if (*state == PLAYER_EVT_STOPED) {
		_short_play_next(s_short_player);
	} else if (*state == PLAYER_EVT_PLAYBACK_COMPLETE) {
		s_short_player->m_cur_id = -1;
	}
	lisa_mem_free(user_data);
	return 0;
}

static int _short_on_play_state(uint16_t st)
{
	LISA_LOGD(TAG, "shortplayer status: %d", st);
	uint16_t *state = NULL;
	switch (st) {
		case PLAYER_EVT_PREPARED:
			return 0;
		case PLAYER_EVT_STOPED:
		case PLAYER_EVT_ERROR:
		case PLAYER_EVT_PLAYING:
		case PLAYER_EVT_PLAYBACK_COMPLETE:
			state = (uint16_t *)lisa_mem_alloc(sizeof(uint16_t));
			*state = st;
			break;
		default:
			return 0;
	}
	if (evs_handler_post_runnable(_play_state_func, (void *)state) != 0) {
		if (state) lisa_mem_free(state);
	}
	return 0;
}

short_player_t *listen_shortplayer_create()
{
	short_player_t *handle = (short_player_t *)lisa_mem_calloc(1, sizeof(short_player_t));

	handle->m_player_name = "short_player";
	handle->m_wait_len = 0;
	handle->m_cur_id = -1;
	handle->m_play_state = PLAYER_EVT_ERROR;
	s_short_player = handle;

	return handle;
}

void listen_shortplayer_destroy(short_player_t *handle)
{
	if (handle != NULL) {
		lisa_mem_free(handle);
	}
}

void listen_shortplayer_stop(short_player_t *handle)
{
	if (handle->m_play_state == PLAYER_EVT_PLAYING || handle->m_play_state == PLAYER_EVT_PAUSED) {
		app_player_stop_sync(PLAYER_T_TONE);
	}
}

void listen_shortplayer_play(short_player_t *handle, uint16_t id)
{
	LISA_LOG(TAG, "wakeup_tts_callback, tts: %d", id);
	handle->m_cur_id = id;
	handle->m_wait_len = 1;
	if (handle->m_play_state == PLAYER_EVT_PLAYING || handle->m_play_state == PLAYER_EVT_PAUSED) {
		listen_shortplayer_stop(handle);
		_short_play_next(handle);
	} else {
		_short_play_next(handle);
	}
}
