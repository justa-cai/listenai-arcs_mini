#ifndef __JK_ASR_H__
#define __JK_ASR_H__

#include <stdbool.h>
#include <stdint.h>

#define JK_ASR_DEFAULT_HOST "ws://"
#define JK_ASR_DEFAULT_PORT "9200"
#define JK_ASR_SAMPLE_RATE 16000
#define JK_ASR_FRAME_SIZE 320  /* 20ms at 16kHz: 16000 * 0.02 = 320 samples */
#define JK_ASR_OPUS_BITRATE 24000  /* 24 kbps */

typedef enum {
    JK_ASR_STATE_DISCONNECTED = 0,
    JK_ASR_STATE_CONNECTING,
    JK_ASR_STATE_CONNECTED,
} jk_asr_state_e;

typedef enum {
    JK_ASR_CODEC_PCM = 0,   /* Raw PCM mode */
    JK_ASR_CODEC_OPUS = 1,  /* Opus encoded mode */
} jk_asr_codec_e;

struct jk_asr;
typedef struct jk_asr jk_asr_t;

typedef struct {
    void (*on_connected)(jk_asr_t *asr);
    void (*on_disconnected)(jk_asr_t *asr);
    void (*on_text_result)(jk_asr_t *asr, const char *text, bool is_final);
    void (*on_vad_event)(jk_asr_t *asr, const char *event, float duration);
    void (*on_error)(jk_asr_t *asr, const char *error_msg);
} jk_asr_callbacks_t;

struct jk_asr {
    void *ws;
    jk_asr_state_e state;
    jk_asr_callbacks_t cbs;
    char *host;
    char *port;
    void *user_data;

    /* Opus encoder state */
    jk_asr_codec_e codec_mode;
    void *opus_encoder;
    int16_t *pcm_buffer;
    uint32_t pcm_buffer_pos;
    bool opus_enabled;
};

/* Core API */
jk_asr_t *jk_asr_create(const char *host, const char *port, jk_asr_callbacks_t *cbs);
void jk_asr_destroy(jk_asr_t *asr);

int jk_asr_connect(jk_asr_t *asr);
int jk_asr_disconnect(jk_asr_t *asr);

/* Audio sending (supports both PCM and Opus modes) */
int jk_asr_send_audio(jk_asr_t *asr, const int16_t *samples, uint32_t count);

/* Opus codec control */
int jk_asr_opus_enable(jk_asr_t *asr);
int jk_asr_opus_disable(jk_asr_t *asr);
bool jk_asr_is_opus_enabled(jk_asr_t *asr);

/* Utility */
bool jk_asr_is_connected(jk_asr_t *asr);

#endif
