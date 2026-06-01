#include "model_log_upload.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lisa_ui_invoke.h"
#include "voice_msg.h"

struct model_log_upload_context {
    bool inited;
    struct {
        const struct model_log_upload_cb *cb;
        void *arg;
    } listeners[4];
};

static struct model_log_upload_context g_model_log_upload_ctx;

static void handle_log_upload_state_change(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    log_upload_state_t *state = (log_upload_state_t *)data;

    (void)unused;
    (void)msg_id;
    (void)len;
    (void)user_data;

    LISA_UI_INVOKE_UI_ARG_PTR(state, sizeof(log_upload_state_t), {
        for (size_t i = 0; i < sizeof(g_model_log_upload_ctx.listeners) / sizeof(g_model_log_upload_ctx.listeners[0]); i++) {
            if (g_model_log_upload_ctx.listeners[i].cb &&
                g_model_log_upload_ctx.listeners[i].cb->on_log_upload_state_change) {
                g_model_log_upload_ctx.listeners[i].cb->on_log_upload_state_change(
                    _invoke_state,
                    g_model_log_upload_ctx.listeners[i].arg);
            }
        }
    });
}

int model_log_upload_init(void)
{
    if (g_model_log_upload_ctx.inited) {
        return 0;
    }

    voice_msg_sub(VOICE_MSG_LOG_UPLOAD_STARTING, handle_log_upload_state_change, NULL);
    voice_msg_sub(VOICE_MSG_LOG_UPLOAD_UPLOADING, handle_log_upload_state_change, NULL);
    voice_msg_sub(VOICE_MSG_LOG_UPLOAD_SUCCESSED, handle_log_upload_state_change, NULL);
    voice_msg_sub(VOICE_MSG_LOG_UPLOAD_FAILED, handle_log_upload_state_change, NULL);

    g_model_log_upload_ctx.inited = true;
    return 0;
}

int model_log_upload_cb_register(const struct model_log_upload_cb *cb, void *arg)
{
    log_upload_state_t snapshot = {0};
    size_t i;

    if (!cb) {
        return -1;
    }

    for (i = 0; i < sizeof(g_model_log_upload_ctx.listeners) / sizeof(g_model_log_upload_ctx.listeners[0]); i++) {
        if (g_model_log_upload_ctx.listeners[i].cb == cb) {
            g_model_log_upload_ctx.listeners[i].arg = arg;
            if (cb->on_log_upload_state_change && log_upload_get_state_snapshot(&snapshot) == 0) {
                cb->on_log_upload_state_change(&snapshot, arg);
            }
            return 0;
        }
    }

    for (i = 0; i < sizeof(g_model_log_upload_ctx.listeners) / sizeof(g_model_log_upload_ctx.listeners[0]); i++) {
        if (!g_model_log_upload_ctx.listeners[i].cb) {
            g_model_log_upload_ctx.listeners[i].cb = cb;
            g_model_log_upload_ctx.listeners[i].arg = arg;
            if (cb->on_log_upload_state_change && log_upload_get_state_snapshot(&snapshot) == 0) {
                cb->on_log_upload_state_change(&snapshot, arg);
            }
            return 0;
        }
    }

    return -1;
}

int model_log_upload_cb_unregister(const struct model_log_upload_cb *cb)
{
    size_t i;

    if (!cb) {
        return -1;
    }

    for (i = 0; i < sizeof(g_model_log_upload_ctx.listeners) / sizeof(g_model_log_upload_ctx.listeners[0]); i++) {
        if (g_model_log_upload_ctx.listeners[i].cb == cb) {
            g_model_log_upload_ctx.listeners[i].cb = NULL;
            g_model_log_upload_ctx.listeners[i].arg = NULL;
            return 0;
        }
    }

    return -1;
}
