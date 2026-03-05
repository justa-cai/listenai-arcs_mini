#include "short_player.h"
#include <stdlib.h>
#include <string.h>
#include "lisa_log.h"
#include "lisa_mem.h"
#include "app_player.h"

#define TAG "shortplayer"


static short_player_t *s_short_player = NULL;

static int _short_on_play_state(uint16_t st)
{
	LISA_LOGD(TAG, "shortplayer status: %d", st);
	if (st == APP_PLAYER_PREPARING || st == PLAYER_EVT_STOPED || st == PLAYER_EVT_ERROR ||
		st == PLAYER_EVT_PLAYING || st == PLAYER_EVT_PLAYBACK_COMPLETE) {
		s_short_player->m_play_state = st;
	}
	return 0;
}

short_player_t *listen_shortplayer_create()
{
	short_player_t *handle = (short_player_t *)lisa_mem_calloc(1, sizeof(short_player_t));

	handle->m_player_name = "short_player";
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
	LISA_LOGD(TAG, "listen_shortplayer_stop %d", handle->m_play_state);
	if (handle->m_play_state == PLAYER_EVT_PLAYING ||
		handle->m_play_state == PLAYER_EVT_PAUSED ||
		handle->m_play_state == APP_PLAYER_PREPARING) {
		app_player_stop_sync(PLAYER_T_TONE);
	}
}

void listen_shortplayer_play(short_player_t *handle, const char *url)
{
	LISA_LOG(TAG, "wakeup_tts_callback, tts: %s", url);
	listen_shortplayer_stop(handle);
	app_player_play_by_throw(PLAYER_T_TONE, (char *)url, 100, _short_on_play_state);
}
