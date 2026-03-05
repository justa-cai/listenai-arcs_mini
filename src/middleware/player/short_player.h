#ifndef __LISTENAI_SHORT_PLAYER_H__
#define __LISTENAI_SHORT_PLAYER_H__

#include "listen_audiomgr.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct short_player_s {
	char *m_player_name;
	uint16_t m_play_state;
} short_player_t;

short_player_t *listen_shortplayer_create();
void listen_shortplayer_destroy(short_player_t *handle);

void listen_shortplayer_play(short_player_t *handle, const char *url);
void listen_shortplayer_stop(short_player_t *handle);

#endif
