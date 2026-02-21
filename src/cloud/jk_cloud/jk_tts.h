#ifndef __JK_TTS_H__
#define __JK_TTS_H__

#include <stdbool.h>
#include <stdint.h>
#include "jk_tts_parser.h"

#define JK_TTS_DEFAULT_HOST "ws://"
#define JK_TTS_DEFAULT_PORT "9300"
#define JK_TTS_DEFAULT_PATH "/tts"
#define JK_TTS_VOICE_ID "趣味口音-湾湾小何"
#define JK_TTS_REQUEST_ID_LEN 32
#define JK_TTS_PENDING_TEXT_LEN 512

typedef enum {
    JK_TTS_STATE_DISCONNECTED = 0,
    JK_TTS_STATE_CONNECTING,
    JK_TTS_STATE_CONNECTED,
    JK_TTS_STATE_PLAYING,
} jk_tts_state_e;

struct jk_tts;
typedef struct jk_tts jk_tts_t;

typedef struct {
    void (*on_connected)(jk_tts_t *tts);
    void (*on_disconnected)(jk_tts_t *tts);
    void (*on_audio_data)(jk_tts_t *tts, int16_t *samples, uint32_t count, jk_tts_metadata_t *meta);
    void (*on_complete)(jk_tts_t *tts);
    void (*on_error)(jk_tts_t *tts, const char *error_msg);
} jk_tts_callbacks_t;

struct jk_tts {
    void *ws;
    jk_tts_state_e state;
    jk_tts_callbacks_t cbs;
    char *host;
    char *port;
    char *voice_id;
    char current_request_id[JK_TTS_REQUEST_ID_LEN];
    void *user_data;
    bool data_complete;
    char pending_text[JK_TTS_PENDING_TEXT_LEN];
    bool has_pending;
};

jk_tts_t *jk_tts_create(const char *host, const char *port, jk_tts_callbacks_t *cbs);
void jk_tts_destroy(jk_tts_t *tts);

int jk_tts_connect(jk_tts_t *tts);
int jk_tts_disconnect(jk_tts_t *tts);

int jk_tts_request(jk_tts_t *tts, const char *text, const char *voice_id);
int jk_tts_stop(jk_tts_t *tts);

bool jk_tts_is_connected(jk_tts_t *tts);
bool jk_tts_is_playing(jk_tts_t *tts);
bool jk_tts_is_data_complete(jk_tts_t *tts);

void jk_tts_set_voice(jk_tts_t *tts, const char *voice_id);
void jk_tts_check_pending(jk_tts_t *tts);

#endif
