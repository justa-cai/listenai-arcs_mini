#ifndef __LISTENAI_AUDIO_PLAYER_H__
#define __LISTENAI_AUDIO_PLAYER_H__

#include "listen_audiomgr.h"
#include "play_mode.h"
#include <stdbool.h>
#include <audio_out.h>
#include "lisa_semaphore.h"
#include <stdint.h>
#include "sys_queue.h"
#include "lisa_player.h"

typedef enum {
	AUDIO_PLAY,
	AUDIO_PAUSE,
	AUDIO_RESUME,
	AUDIO_STOP,
} audio_play_cmd;

typedef struct audioplayer_s {
	bool (*on_directive)(struct audioplayer_s *handle, audio_play_cmd cmd, audio_out_t *item, uint32_t item_size);
	void (*pause)(struct audioplayer_s *handle);
	void (*resumeByVoice)(struct audioplayer_s *handle);
	void (*next)(struct audioplayer_s *handle);
	void (*prev)(struct audioplayer_s *handle);
	void (*stop)(struct audioplayer_s *handle, bool play_after_stop);
	void (*replay)(struct audioplayer_s *handle);

	channel_type_e m_channel_name;
	char *m_player_name;
	bool m_is_need_play_after_op;
	bool m_is_pause_called;
	focus_state_e m_focus_state;
	PlayerEvt m_player_state;
	listen_audiomgr_t *m_audio_mgr;
	lisa_semaphore_t *pause_sema;
	play_mode_t *m_play_mode;
} audioplayer_t;

audioplayer_t *listen_audioplayer_create(const listen_audiomgr_t *audio_mgr, const play_mode_t *play_mode);
void listen_audioplayer_destroy(audioplayer_t *handle);
void listen_audioplayer_resume(audioplayer_t *handle);
void listen_audioplayer_puse(audioplayer_t *handle);

PlayerEvt listen_audioplayer_get_state(audioplayer_t *handle);

char *listen_audioplayer_get_res_id(audioplayer_t *handle);

void audio_player_stop_by_user(void);

#endif
