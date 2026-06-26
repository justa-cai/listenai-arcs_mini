#ifndef __MODEL_CAMERA_PREVIEW_H__
#define __MODEL_CAMERA_PREVIEW_H__

#include <stdbool.h>
#include <stdint.h>

#include "voice_intent_photo_flow.h"

typedef enum {
    MODEL_CAMERA_PREVIEW_SOURCE_NONE = 0,
    MODEL_CAMERA_PREVIEW_SOURCE_BUTTON,
    MODEL_CAMERA_PREVIEW_SOURCE_MCP,
} model_camera_preview_source_t;

typedef enum {
    MODEL_CAMERA_PREVIEW_COUNTDOWN_STOP = 0,
    MODEL_CAMERA_PREVIEW_COUNTDOWN_UPDATE,
    MODEL_CAMERA_PREVIEW_COUNTDOWN_CAPTURE,
} model_camera_preview_countdown_result_t;

typedef enum {
    MODEL_CAMERA_PREVIEW_CAPTURE_NONE = 0,
    MODEL_CAMERA_PREVIEW_CAPTURE_BUTTON,
    MODEL_CAMERA_PREVIEW_CAPTURE_VOICE,
} model_camera_preview_capture_kind_t;

typedef enum {
    MODEL_CAMERA_PREVIEW_SUBMIT_NONE = 0,
    MODEL_CAMERA_PREVIEW_SUBMIT_BUTTON,
    MODEL_CAMERA_PREVIEW_SUBMIT_VOICE,
} model_camera_preview_submit_kind_t;

typedef struct {
    model_camera_preview_source_t source;
    uint32_t auto_capture_delay_ms;
} model_camera_preview_req_t;

typedef struct {
    uint8_t preview_active;
    uint8_t captured;
    uint8_t source;
    uint8_t result_tts_ready;
    uint8_t result_tts_started;
    uint32_t countdown_remaining_s;
} model_camera_preview_t;

struct model_camera_preview_cb {
    void (*on_start)(void *arg, const model_camera_preview_req_t *req);
    void (*on_capture)(void *arg);
    void (*on_exit)(void *arg);
    void (*on_result_tts_ready)(void *arg);
};

int model_camera_preview_evt_init(void);
int model_camera_preview_cb_register(const struct model_camera_preview_cb *cb, void *arg);
int model_camera_preview_cb_unregister(const struct model_camera_preview_cb *cb);

void model_camera_preview_init(model_camera_preview_t *preview);
void model_camera_preview_reset(model_camera_preview_t *preview);
void model_camera_preview_start_preview(model_camera_preview_t *preview,
                                        model_camera_preview_source_t source,
                                        uint32_t auto_capture_delay_ms,
                                        uint32_t countdown_interval_ms);
void model_camera_preview_mark_result_tts_started(model_camera_preview_t *preview);

bool model_camera_preview_is_active(const model_camera_preview_t *preview);
bool model_camera_preview_is_preview_active(const model_camera_preview_t *preview);
bool model_camera_preview_keep_preview_alive(const model_camera_preview_t *preview);
bool model_camera_preview_is_captured(const model_camera_preview_t *preview);
bool model_camera_preview_is_result_active(const model_camera_preview_t *preview);
bool model_camera_preview_is_result_tts_ready(const model_camera_preview_t *preview);
bool model_camera_preview_is_result_tts_started(const model_camera_preview_t *preview);

model_camera_preview_source_t model_camera_preview_source_get(const model_camera_preview_t *preview);
uint32_t model_camera_preview_countdown_remaining_get(const model_camera_preview_t *preview);
bool model_camera_preview_should_start_countdown(const model_camera_preview_t *preview);
model_camera_preview_countdown_result_t model_camera_preview_countdown_tick(
    model_camera_preview_t *preview);
model_camera_preview_capture_kind_t model_camera_preview_request_capture(
    model_camera_preview_t *preview);
model_camera_preview_submit_kind_t model_camera_preview_prepare_submit(model_camera_preview_t *preview);
bool model_camera_preview_restore_after_submit_failure(
    model_camera_preview_t *preview,
    model_camera_preview_submit_kind_t submit_kind);
bool model_camera_preview_handle_result_tts_ready(model_camera_preview_t *preview);
const char *model_camera_preview_source_name(model_camera_preview_source_t source);
void model_camera_preview_publish_state(const model_camera_preview_t *preview);
void model_camera_preview_fill_voice_state(const model_camera_preview_t *preview,
                                           voice_msg_camera_preview_state_t *state);

#endif // __MODEL_CAMERA_PREVIEW_H__
