#ifndef __LISA_EVS_APP_H__
#define __LISA_EVS_APP_H__

#include <stdint.h>
#include "lisa_evs_rec.h"
#include "lisa_evs_websocket.h"

#define EVS_EVALUATE_RECORD_FILE "/SD:/audio/evs_evaluate.pcm"

typedef enum {
    TRANS_COMPLETE = 0,
    TRANS_ERROR,
} trans_err_e;

typedef enum {
    TTS_COMPLETE = 0,
    TTS_ERROR,
    TTS_PLAY_START,
    TTS_PLAY_END,
} tts_err_e;

typedef struct {
    bool (*is_ws_connect)(void);
    void (*evaluate_start)(lisa_evs_rec_evaluate_param_t *evaluate_param);
    void (*evaluate_stop)(void);
    void (*evaluate_play)(void);
    void (*translate_start)(uint8_t *text);
    void (*tts_start)(uint8_t *text);
} lisa_evs_ops_t;

typedef struct {
    void (*evaluate_result)(const uint8_t *const msg, uint32_t len);
    void (*translate_result)(const uint8_t *msg, uint32_t len, trans_err_e status);
    void (*tts_result)(tts_err_e status);
} lisa_evs_cbs_t;

typedef struct {
    lisa_evs_ops_t ops;
    lisa_evs_cbs_t cbs;
} lisa_evs_context_t;

lisa_evs_context_t *lisa_evs_init(lisa_evs_cbs_t *cbs);

#endif // __LISA_EVS_APP_H__
