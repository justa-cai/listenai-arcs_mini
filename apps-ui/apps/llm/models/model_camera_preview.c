#include <stdint.h>
#include <string.h>

#define TAG "model_camera_preview"

#include "lisa_ui_invoke.h"
#include "lisa_ui_log.h"

#include "model_camera_preview.h"

#ifdef LISA_UI_PLATFORM_ARCS
#include "voice_msg.h"
#endif

struct model_camera_preview_context {
    uint32_t inited: 1;
    const struct model_camera_preview_cb *cbs;
    void *arg;
};

static struct model_camera_preview_context model_camera_preview_ctx = {
    .inited = 0,
    .cbs = NULL,
    .arg = NULL,
};

#ifdef LISA_UI_PLATFORM_ARCS

static model_camera_preview_source_t
model_camera_preview_source_from_voice_mode(uint8_t mode)
{
    return mode == CAMERA_PREVIEW_MODE_MCP ? MODEL_CAMERA_PREVIEW_SOURCE_MCP
                                                           : MODEL_CAMERA_PREVIEW_SOURCE_BUTTON;
}

static void model_camera_preview_start_msg_handle(void *unused, uint32_t msg_id, void *data,
                                                  uint32_t len, void *user_data)
{
    voice_msg_camera_preview_req_t *req = (voice_msg_camera_preview_req_t *)data;

    if (!req || len < sizeof(*req)) {
        LISA_UI_LOGE("invalid camera preview request");
        return;
    }

    LISA_UI_INVOKE_UI_ARG_PTR(req, sizeof(*req), {
        model_camera_preview_req_t preview_req;

        preview_req.source = model_camera_preview_source_from_voice_mode(_invoke_req->mode);
        preview_req.auto_capture_delay_ms = _invoke_req->auto_capture_delay_ms;

        LISA_UI_LOGI("camera preview request received, mode=%u, source=%u, delay_ms=%u",
                     _invoke_req->mode, preview_req.source, preview_req.auto_capture_delay_ms);

        if (model_camera_preview_ctx.cbs && model_camera_preview_ctx.cbs->on_start) {
            model_camera_preview_ctx.cbs->on_start(model_camera_preview_ctx.arg, &preview_req);
        }
    });
}

static void model_camera_preview_capture_msg_handle(void *unused, uint32_t msg_id, void *data,
                                                    uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    LISA_UI_INVOKE_UI_ARG_NONE({
        if (model_camera_preview_ctx.cbs && model_camera_preview_ctx.cbs->on_capture) {
            model_camera_preview_ctx.cbs->on_capture(model_camera_preview_ctx.arg);
        }
    });
}

static void model_camera_preview_exit_msg_handle(void *unused, uint32_t msg_id, void *data,
                                                 uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    LISA_UI_INVOKE_UI_ARG_NONE({
        if (model_camera_preview_ctx.cbs && model_camera_preview_ctx.cbs->on_exit) {
            model_camera_preview_ctx.cbs->on_exit(model_camera_preview_ctx.arg);
        }
    });
}

static void model_camera_preview_result_tts_ready_msg_handle(void *unused, uint32_t msg_id, void *data,
                                                             uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    LISA_UI_INVOKE_UI_ARG_NONE({
        LISA_UI_LOGI("photo result TTS ready");
        if (model_camera_preview_ctx.cbs && model_camera_preview_ctx.cbs->on_result_tts_ready) {
            model_camera_preview_ctx.cbs->on_result_tts_ready(model_camera_preview_ctx.arg);
        }
    });
}

#endif /* LISA_UI_PLATFORM_ARCS */

int model_camera_preview_evt_init(void)
{
    if (model_camera_preview_ctx.inited) {
        return 0;
    }

#ifdef LISA_UI_PLATFORM_ARCS
    voice_msg_sub(VOICE_MSG_APP_CAMERA_PREVIEW_START, model_camera_preview_start_msg_handle, NULL);
    voice_msg_sub(VOICE_MSG_APP_CAMERA_PREVIEW_CAPTURE, model_camera_preview_capture_msg_handle, NULL);
    voice_msg_sub(VOICE_MSG_APP_CAMERA_PREVIEW_EXIT, model_camera_preview_exit_msg_handle, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_TTS_URL, model_camera_preview_result_tts_ready_msg_handle, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_PUSHUP_TTS_URL, model_camera_preview_result_tts_ready_msg_handle, NULL);
#endif

    model_camera_preview_ctx.inited = 1;

    return 0;
}

int model_camera_preview_cb_register(const struct model_camera_preview_cb *cb, void *arg)
{
    model_camera_preview_ctx.cbs = cb;
    model_camera_preview_ctx.arg = arg;
    return 0;
}

int model_camera_preview_cb_unregister(const struct model_camera_preview_cb *cb)
{
    (void)cb;
    model_camera_preview_ctx.cbs = NULL;
    model_camera_preview_ctx.arg = NULL;
    return 0;
}

static uint32_t model_camera_preview_countdown_calc(uint32_t auto_capture_delay_ms,
                                                    uint32_t countdown_interval_ms)
{
    if (auto_capture_delay_ms == 0) {
        return 0;
    }

    if (countdown_interval_ms == 0) {
        return 1;
    }

    return (auto_capture_delay_ms + countdown_interval_ms - 1U) / countdown_interval_ms;
}

static void model_camera_preview_restore_preview(model_camera_preview_t *preview)
{
    if (!preview) {
        return;
    }

    preview->preview_active = 1;
    preview->captured = 0;
    preview->result_tts_ready = 0;
    preview->result_tts_started = 0;
    preview->countdown_remaining_s = 0;
}

static void model_camera_preview_begin_capture(model_camera_preview_t *preview)
{
    if (!preview) {
        return;
    }

    preview->captured = 1;
    preview->countdown_remaining_s = 0;
}

static void model_camera_preview_start_processing(model_camera_preview_t *preview)
{
    if (!preview) {
        return;
    }

    preview->preview_active = 0;
    preview->result_tts_ready = 0;
    preview->result_tts_started = 0;
}

static void model_camera_preview_begin_button_submit(model_camera_preview_t *preview)
{
    if (!preview) {
        return;
    }

    preview->preview_active = 0;
    preview->captured = 1;
    preview->result_tts_ready = 0;
    preview->result_tts_started = 0;
    preview->countdown_remaining_s = 0;
}

static void model_camera_preview_set_result_tts_ready(model_camera_preview_t *preview)
{
    if (!preview) {
        return;
    }

    preview->preview_active = 0;
    preview->result_tts_ready = 1;
    preview->result_tts_started = 0;
}

static bool model_camera_preview_countdown_step(model_camera_preview_t *preview)
{
    if (!model_camera_preview_keep_preview_alive(preview) ||
        model_camera_preview_source_get(preview) != MODEL_CAMERA_PREVIEW_SOURCE_MCP) {
        return false;
    }

    if (preview->countdown_remaining_s > 1) {
        preview->countdown_remaining_s--;
        return false;
    }

    preview->countdown_remaining_s = 0;
    return true;
}

void model_camera_preview_init(model_camera_preview_t *preview)
{
    model_camera_preview_reset(preview);
}

void model_camera_preview_reset(model_camera_preview_t *preview)
{
    if (!preview) {
        return;
    }

    memset(preview, 0, sizeof(*preview));
}

void model_camera_preview_start_preview(model_camera_preview_t *preview,
                                        model_camera_preview_source_t source,
                                        uint32_t auto_capture_delay_ms,
                                        uint32_t countdown_interval_ms)
{
    uint32_t countdown_remaining_s = 0;

    if (!preview) {
        return;
    }

    countdown_remaining_s =
        model_camera_preview_countdown_calc(auto_capture_delay_ms, countdown_interval_ms);

    model_camera_preview_reset(preview);
    preview->preview_active = 1;
    preview->source = (uint8_t)source;
    preview->countdown_remaining_s = countdown_remaining_s;

    if (source == MODEL_CAMERA_PREVIEW_SOURCE_MCP && preview->countdown_remaining_s == 0) {
        preview->countdown_remaining_s = 1;
    }
}

void model_camera_preview_mark_result_tts_started(model_camera_preview_t *preview)
{
    if (!preview) {
        return;
    }

    if (model_camera_preview_is_result_tts_ready(preview)) {
        preview->result_tts_started = 1;
    }
}

bool model_camera_preview_is_active(const model_camera_preview_t *preview)
{
    return model_camera_preview_is_preview_active(preview) ||
           model_camera_preview_is_result_active(preview);
}

bool model_camera_preview_is_preview_active(const model_camera_preview_t *preview)
{
    return preview && preview->preview_active;
}

bool model_camera_preview_keep_preview_alive(const model_camera_preview_t *preview)
{
    return model_camera_preview_is_preview_active(preview) &&
           !model_camera_preview_is_captured(preview);
}

bool model_camera_preview_is_captured(const model_camera_preview_t *preview)
{
    return preview && preview->captured;
}

bool model_camera_preview_is_result_active(const model_camera_preview_t *preview)
{
    if (!preview || !preview->captured) {
        return false;
    }

    return preview->source == MODEL_CAMERA_PREVIEW_SOURCE_BUTTON ||
           preview->source == MODEL_CAMERA_PREVIEW_SOURCE_MCP;
}

bool model_camera_preview_is_result_tts_ready(const model_camera_preview_t *preview)
{
    return model_camera_preview_is_result_active(preview) && preview->result_tts_ready;
}

bool model_camera_preview_is_result_tts_started(const model_camera_preview_t *preview)
{
    return model_camera_preview_is_result_tts_ready(preview) && preview->result_tts_started;
}

model_camera_preview_source_t model_camera_preview_source_get(const model_camera_preview_t *preview)
{
    if (!preview) {
        return MODEL_CAMERA_PREVIEW_SOURCE_NONE;
    }

    return (model_camera_preview_source_t)preview->source;
}

uint32_t model_camera_preview_countdown_remaining_get(const model_camera_preview_t *preview)
{
    if (!preview) {
        return 0;
    }

    return preview->countdown_remaining_s;
}

bool model_camera_preview_should_start_countdown(const model_camera_preview_t *preview)
{
    return model_camera_preview_source_get(preview) == MODEL_CAMERA_PREVIEW_SOURCE_MCP &&
           model_camera_preview_countdown_remaining_get(preview) > 0;
}

model_camera_preview_countdown_result_t model_camera_preview_countdown_tick(
    model_camera_preview_t *preview)
{
    if (!model_camera_preview_keep_preview_alive(preview) ||
        model_camera_preview_source_get(preview) != MODEL_CAMERA_PREVIEW_SOURCE_MCP) {
        return MODEL_CAMERA_PREVIEW_COUNTDOWN_STOP;
    }

    if (!model_camera_preview_countdown_step(preview)) {
        return MODEL_CAMERA_PREVIEW_COUNTDOWN_UPDATE;
    }

    return MODEL_CAMERA_PREVIEW_COUNTDOWN_CAPTURE;
}

model_camera_preview_capture_kind_t model_camera_preview_request_capture(
    model_camera_preview_t *preview)
{
    if (!model_camera_preview_is_preview_active(preview) ||
        model_camera_preview_is_captured(preview)) {
        return MODEL_CAMERA_PREVIEW_CAPTURE_NONE;
    }

    model_camera_preview_begin_capture(preview);

    if (model_camera_preview_source_get(preview) == MODEL_CAMERA_PREVIEW_SOURCE_BUTTON) {
        return MODEL_CAMERA_PREVIEW_CAPTURE_BUTTON;
    }

    return MODEL_CAMERA_PREVIEW_CAPTURE_VOICE;
}

model_camera_preview_submit_kind_t model_camera_preview_prepare_submit(model_camera_preview_t *preview)
{
    if (!preview || !model_camera_preview_is_captured(preview)) {
        return MODEL_CAMERA_PREVIEW_SUBMIT_NONE;
    }

    if (model_camera_preview_source_get(preview) == MODEL_CAMERA_PREVIEW_SOURCE_BUTTON) {
        model_camera_preview_begin_button_submit(preview);
        return MODEL_CAMERA_PREVIEW_SUBMIT_BUTTON;
    }

    if (model_camera_preview_source_get(preview) == MODEL_CAMERA_PREVIEW_SOURCE_MCP) {
        model_camera_preview_start_processing(preview);
        return MODEL_CAMERA_PREVIEW_SUBMIT_VOICE;
    }

    return MODEL_CAMERA_PREVIEW_SUBMIT_NONE;
}

bool model_camera_preview_restore_after_submit_failure(
    model_camera_preview_t *preview,
    model_camera_preview_submit_kind_t submit_kind)
{
    if (!preview || submit_kind != MODEL_CAMERA_PREVIEW_SUBMIT_BUTTON) {
        return false;
    }

    model_camera_preview_restore_preview(preview);
    return true;
}

bool model_camera_preview_handle_result_tts_ready(model_camera_preview_t *preview)
{
    if (!model_camera_preview_is_result_active(preview)) {
        return false;
    }

    model_camera_preview_set_result_tts_ready(preview);
    return true;
}

const char *model_camera_preview_source_name(model_camera_preview_source_t source)
{
    switch (source) {
    case MODEL_CAMERA_PREVIEW_SOURCE_BUTTON:
        return "button";
    case MODEL_CAMERA_PREVIEW_SOURCE_MCP:
        return "voice";
    default:
        return "none";
    }
}

void model_camera_preview_publish_state(const model_camera_preview_t *preview)
{
#ifdef LISA_UI_PLATFORM_ARCS
    voice_msg_camera_preview_state_t state = {0};

    model_camera_preview_fill_voice_state(preview, &state);
    voice_msg_pub(VOICE_MSG_APP_CAMERA_PREVIEW_STATE, &state, sizeof(state));
#else
    (void)preview;
#endif
}

void model_camera_preview_fill_voice_state(const model_camera_preview_t *preview,
                                           voice_msg_camera_preview_state_t *state)
{
    bool publish_result_state = false;

    if (!state) {
        return;
    }

    memset(state, 0, sizeof(*state));
    if (!preview) {
        return;
    }

    state->active = preview->preview_active ? 1 : 0;
    state->captured = preview->captured ? 1 : 0;
    publish_result_state = model_camera_preview_source_get(preview) ==
                               MODEL_CAMERA_PREVIEW_SOURCE_MCP &&
                           model_camera_preview_is_result_active(preview);

    /* 按键拍照在提交后仍由 UI 保持结果态，但对 CP 继续发布 NONE。
     * PHOTO_FLOW 依赖该状态识别“提交完成”，并据此放行照片结果 TTS。 */
    if (model_camera_preview_is_preview_active(preview) || publish_result_state) {
        state->mode = model_camera_preview_source_get(preview) == MODEL_CAMERA_PREVIEW_SOURCE_MCP
                          ? CAMERA_PREVIEW_MODE_MCP
                          : CAMERA_PREVIEW_MODE_BUTTON;
    } else {
        state->mode = CAMERA_PREVIEW_MODE_NONE;
    }

    if (model_camera_preview_keep_preview_alive(preview)) {
        state->phase = CAMERA_FLOW_PHASE_PREVIEW;
    } else if (publish_result_state) {
        state->phase = model_camera_preview_is_result_tts_ready(preview)
                           ? CAMERA_FLOW_PHASE_RESULT_TTS
                           : CAMERA_FLOW_PHASE_PROCESSING;
    } else {
        state->phase = CAMERA_FLOW_PHASE_NONE;
    }
}
