#ifndef __JK_PCM_PLAYER_H__
#define __JK_PCM_PLAYER_H__

#include <stdint.h>
#include <stdbool.h>
#include "lisa_player.h"
#include "listen_audiomgr.h"

#define JK_PCM_PLAYER_BUFFER_SIZE (32 * 1024)
#define JK_PCM_PLAYER_SAMPLE_RATE 16000

typedef struct jk_pcm_player jk_pcm_player_t;

typedef struct {
    void (*on_play_start)(jk_pcm_player_t *player);
    void (*on_play_complete)(jk_pcm_player_t *player);
    void (*on_error)(jk_pcm_player_t *player, const char *error);
} jk_pcm_player_callbacks_t;

struct jk_pcm_player {
    PLAYER_HANDLE player;
    listen_audiomgr_t *audio_mgr;
    bool is_playing;     /* Controls if data is sent to underlying player */
    bool is_prepared;    /* lisa_player has been configured (monotonic) */
    bool is_preparing;   /* lisa_player is currently preparing */
    jk_pcm_player_callbacks_t cbs;
    void *user_data;
    uint32_t total_written;
};

/* Creation and destruction */
jk_pcm_player_t *jk_pcm_player_create(listen_audiomgr_t *audio_mgr,
                                       jk_pcm_player_callbacks_t *cbs);
void jk_pcm_player_destroy(jk_pcm_player_t *player);

/* Playback control */
int jk_pcm_player_write(jk_pcm_player_t *player,
                        const int16_t *samples,
                        uint32_t count);
int jk_pcm_player_start(jk_pcm_player_t *player);
int jk_pcm_player_stop(jk_pcm_player_t *player);
int jk_pcm_player_end_stream(jk_pcm_player_t *player);
void jk_pcm_player_reset(jk_pcm_player_t *player);

/* State queries */
bool jk_pcm_player_is_playing(jk_pcm_player_t *player);
bool jk_pcm_player_is_prepared(jk_pcm_player_t *player);
bool jk_pcm_player_is_preparing(jk_pcm_player_t *player);
uint32_t jk_pcm_player_get_buffered(jk_pcm_player_t *player);

#endif /* __JK_PCM_PLAYER_H__ */
