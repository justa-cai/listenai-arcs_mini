#ifndef __JK_ASR_H__
#define __JK_ASR_H__

#include <stdbool.h>
#include <stdint.h>

#define JK_ASR_DEFAULT_HOST "ws://"
#define JK_ASR_DEFAULT_PORT "9200"
#define JK_ASR_SAMPLE_RATE 16000

typedef enum {
    JK_ASR_STATE_DISCONNECTED = 0,
    JK_ASR_STATE_CONNECTING,
    JK_ASR_STATE_CONNECTED,
} jk_asr_state_e;

struct jk_asr;
typedef struct jk_asr jk_asr_t;

typedef struct {
    void (*on_connected)(jk_asr_t *asr);
    void (*on_disconnected)(jk_asr_t *asr);
    void (*on_text_result)(jk_asr_t *asr, const char *text, bool is_final);
    void (*on_error)(jk_asr_t *asr, const char *error_msg);
} jk_asr_callbacks_t;

struct jk_asr {
    void *ws;
    jk_asr_state_e state;
    jk_asr_callbacks_t cbs;
    char *host;
    char *port;
    void *user_data;
};

jk_asr_t *jk_asr_create(const char *host, const char *port, jk_asr_callbacks_t *cbs);
void jk_asr_destroy(jk_asr_t *asr);

int jk_asr_connect(jk_asr_t *asr);
int jk_asr_disconnect(jk_asr_t *asr);

int jk_asr_send_audio(jk_asr_t *asr, const int16_t *samples, uint32_t count);

bool jk_asr_is_connected(jk_asr_t *asr);

#endif
