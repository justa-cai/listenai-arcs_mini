#ifndef __LISTENAI_TTS_PLAYER_H__
#define __LISTENAI_TTS_PLAYER_H__

#include "listen_audiomgr.h"
#include <stdbool.h>
#include <stdint.h>
#include "audio_out.h"

typedef struct {
	void (*on_focus_state)(focus_state_e state, channel_type_e by_which);
} tts_focus_callback_t;

typedef struct tts_player_s {
	void (*on_directive)(struct tts_player_s *handle, audio_out_t *item, bool interrupt);
    void (*on_reply_ask)(struct tts_player_s *handle, audio_out_t *item);

	void (*stop)(struct tts_player_s *handle);

	channel_type_e m_channel_name;
	char *m_player_name;

	struct audio_out_s *m_item;
	struct audio_out_s *m_curr_item;
	struct audio_out_s *m_ask_item;

	focus_state_e m_focus_state;
	uint16_t m_play_state;

	listen_audiomgr_t *m_audio_mgr;
	tts_focus_callback_t *m_focus_cb;

} tts_player_t;

tts_player_t *listen_ttsplayer_create(listen_audiomgr_t *audio_mgr);
void listen_ttsplayer_destroy(tts_player_t *handler);

void listen_ttsplayer_add_focus_callback(tts_player_t *handle, tts_focus_callback_t *callback);

#endif
