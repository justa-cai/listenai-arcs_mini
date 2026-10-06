#include "model_sd_music_sync.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lisa_ui_invoke.h"
#include "voice_msg.h"

#if defined(LISA_UI_PLATFORM_ARCS) && defined(CONFIG_MIDDLEWARE_SD_MUSIC)
#include "service_sd_music.h"
#endif

struct model_sd_music_sync_context {
    bool inited;
    bool card_available;
    voice_msg_sd_music_sync_state_t snapshot;
    struct {
        const struct model_sd_music_sync_cb *cb;
        void *arg;
    } listeners[4];
};

static struct model_sd_music_sync_context g_model_sd_music_sync_ctx = {
    .snapshot = {
        .state = VOICE_MSG_SD_MUSIC_SYNC_STATE_IDLE,
    },
};

static voice_msg_sd_music_sync_state_e state_from_msg_id(uint32_t msg_id)
{
    switch (msg_id) {
    case VOICE_MSG_APP_SD_MUSIC_SYNC_START:
        return VOICE_MSG_SD_MUSIC_SYNC_STATE_START;
    case VOICE_MSG_APP_SD_MUSIC_SYNC_UPLOADING:
        return VOICE_MSG_SD_MUSIC_SYNC_STATE_UPLOADING;
    case VOICE_MSG_APP_SD_MUSIC_SYNC_SUCCESSED:
        return VOICE_MSG_SD_MUSIC_SYNC_STATE_SUCCESSED;
    case VOICE_MSG_APP_SD_MUSIC_SYNC_FAILED:
        return VOICE_MSG_SD_MUSIC_SYNC_STATE_FAILED;
    case VOICE_MSG_APP_SD_MUSIC_SYNC_FINISHED:
    default:
        return VOICE_MSG_SD_MUSIC_SYNC_STATE_FINISHED;
    }
}

static void notify_listeners(const voice_msg_sd_music_sync_state_t *state)
{
    for (size_t i = 0;
         i < sizeof(g_model_sd_music_sync_ctx.listeners) / sizeof(g_model_sd_music_sync_ctx.listeners[0]);
         i++) {
        if (g_model_sd_music_sync_ctx.listeners[i].cb &&
            g_model_sd_music_sync_ctx.listeners[i].cb->on_sd_music_sync_state_change) {
            g_model_sd_music_sync_ctx.listeners[i].cb->on_sd_music_sync_state_change(
                state,
                g_model_sd_music_sync_ctx.listeners[i].arg);
        }
    }
}

static void notify_removed_listeners(void)
{
    for (size_t i = 0;
         i < sizeof(g_model_sd_music_sync_ctx.listeners) / sizeof(g_model_sd_music_sync_ctx.listeners[0]);
         i++) {
        if (g_model_sd_music_sync_ctx.listeners[i].cb &&
            g_model_sd_music_sync_ctx.listeners[i].cb->on_sd_music_card_removed) {
            g_model_sd_music_sync_ctx.listeners[i].cb->on_sd_music_card_removed(
                g_model_sd_music_sync_ctx.listeners[i].arg);
        }
    }
}

static void notify_card_state_listeners(void)
{
    for (size_t i = 0;
         i < sizeof(g_model_sd_music_sync_ctx.listeners) / sizeof(g_model_sd_music_sync_ctx.listeners[0]);
         i++) {
        if (g_model_sd_music_sync_ctx.listeners[i].cb &&
            g_model_sd_music_sync_ctx.listeners[i].cb->on_sd_music_card_state_change) {
            g_model_sd_music_sync_ctx.listeners[i].cb->on_sd_music_card_state_change(
                g_model_sd_music_sync_ctx.card_available,
                g_model_sd_music_sync_ctx.listeners[i].arg);
        }
    }
}

static void notify_play_failed_listeners(void)
{
    for (size_t i = 0;
         i < sizeof(g_model_sd_music_sync_ctx.listeners) / sizeof(g_model_sd_music_sync_ctx.listeners[0]);
         i++) {
        if (g_model_sd_music_sync_ctx.listeners[i].cb &&
            g_model_sd_music_sync_ctx.listeners[i].cb->on_sd_music_play_failed) {
            g_model_sd_music_sync_ctx.listeners[i].cb->on_sd_music_play_failed(
                g_model_sd_music_sync_ctx.listeners[i].arg);
        }
    }
}

static void handle_sd_music_sync_state_change(void *unused, uint32_t msg_id,
                                               void *data, uint32_t len,
                                               void *user_data)
{
    voice_msg_sd_music_sync_state_t state = {0};

    (void)unused;
    (void)user_data;

    if (data && len >= sizeof(state)) {
        memcpy(&state, data, sizeof(state));
    } else {
        state.state = (uint32_t)state_from_msg_id(msg_id);
    }

    voice_msg_sd_music_sync_state_t *state_ptr = &state;
    LISA_UI_INVOKE_UI_ARG_PTR(state_ptr, sizeof(state), {
        memcpy(&g_model_sd_music_sync_ctx.snapshot, _invoke_state_ptr, sizeof(g_model_sd_music_sync_ctx.snapshot));
        notify_listeners(&g_model_sd_music_sync_ctx.snapshot);
    });
}

static void handle_sd_music_notify(void *unused, uint32_t msg_id,
                                   void *data, uint32_t len,
                                   void *user_data)
{
    (void)unused;
    (void)data;
    (void)len;
    (void)user_data;

    if (msg_id == VOICE_MSG_APP_SD_MUSIC_CARD_REMOVED) {
        LISA_UI_INVOKE_UI_ARG_NONE({
            notify_removed_listeners();
        });
    } else if (msg_id == VOICE_MSG_APP_SD_MUSIC_PLAY_FAILED) {
        LISA_UI_INVOKE_UI_ARG_NONE({
            notify_play_failed_listeners();
        });
    }
}

static void handle_sd_music_card_state(void *unused, uint32_t msg_id,
                                       void *data, uint32_t len,
                                       void *user_data)
{
    voice_msg_sd_card_state_t state = {0};

    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (data && len >= sizeof(state)) {
        memcpy(&state, data, sizeof(state));
    }

    bool available = state.available != 0;
    LISA_UI_INVOKE_UI_ARG_BASE(available, {
        g_model_sd_music_sync_ctx.card_available = _invoke_available;
        notify_card_state_listeners();
    });
}

int model_sd_music_sync_init(void)
{
    if (g_model_sd_music_sync_ctx.inited) {
        return 0;
    }

    voice_msg_sub(VOICE_MSG_APP_SD_MUSIC_SYNC_START, handle_sd_music_sync_state_change, NULL);
    voice_msg_sub(VOICE_MSG_APP_SD_MUSIC_SYNC_UPLOADING, handle_sd_music_sync_state_change, NULL);
    voice_msg_sub(VOICE_MSG_APP_SD_MUSIC_SYNC_SUCCESSED, handle_sd_music_sync_state_change, NULL);
    voice_msg_sub(VOICE_MSG_APP_SD_MUSIC_SYNC_FAILED, handle_sd_music_sync_state_change, NULL);
    voice_msg_sub(VOICE_MSG_APP_SD_MUSIC_SYNC_FINISHED, handle_sd_music_sync_state_change, NULL);
    voice_msg_sub(VOICE_MSG_APP_SD_MUSIC_CARD_REMOVED, handle_sd_music_notify, NULL);
    voice_msg_sub(VOICE_MSG_APP_SD_MUSIC_CARD_STATE, handle_sd_music_card_state, NULL);
    voice_msg_sub(VOICE_MSG_APP_SD_MUSIC_PLAY_FAILED, handle_sd_music_notify, NULL);

#if defined(LISA_UI_PLATFORM_ARCS) && defined(CONFIG_MIDDLEWARE_SD_MUSIC)
    g_model_sd_music_sync_ctx.card_available = service_sd_music_is_card_ready();
#endif

    g_model_sd_music_sync_ctx.inited = true;
    return 0;
}

int model_sd_music_sync_cb_register(const struct model_sd_music_sync_cb *cb, void *arg)
{
    size_t i;

    if (!cb) {
        return -1;
    }

    for (i = 0; i < sizeof(g_model_sd_music_sync_ctx.listeners) / sizeof(g_model_sd_music_sync_ctx.listeners[0]); i++) {
        if (g_model_sd_music_sync_ctx.listeners[i].cb == cb) {
            g_model_sd_music_sync_ctx.listeners[i].arg = arg;
            if (cb->on_sd_music_sync_state_change) {
                cb->on_sd_music_sync_state_change(&g_model_sd_music_sync_ctx.snapshot, arg);
            }
            if (cb->on_sd_music_card_state_change) {
                cb->on_sd_music_card_state_change(g_model_sd_music_sync_ctx.card_available, arg);
            }
            return 0;
        }
    }

    for (i = 0; i < sizeof(g_model_sd_music_sync_ctx.listeners) / sizeof(g_model_sd_music_sync_ctx.listeners[0]); i++) {
        if (!g_model_sd_music_sync_ctx.listeners[i].cb) {
            g_model_sd_music_sync_ctx.listeners[i].cb = cb;
            g_model_sd_music_sync_ctx.listeners[i].arg = arg;
            if (cb->on_sd_music_sync_state_change) {
                cb->on_sd_music_sync_state_change(&g_model_sd_music_sync_ctx.snapshot, arg);
            }
            if (cb->on_sd_music_card_state_change) {
                cb->on_sd_music_card_state_change(g_model_sd_music_sync_ctx.card_available, arg);
            }
            return 0;
        }
    }

    return -1;
}

int model_sd_music_sync_cb_unregister(const struct model_sd_music_sync_cb *cb)
{
    size_t i;

    if (!cb) {
        return -1;
    }

    for (i = 0; i < sizeof(g_model_sd_music_sync_ctx.listeners) / sizeof(g_model_sd_music_sync_ctx.listeners[0]); i++) {
        if (g_model_sd_music_sync_ctx.listeners[i].cb == cb) {
            g_model_sd_music_sync_ctx.listeners[i].cb = NULL;
            g_model_sd_music_sync_ctx.listeners[i].arg = NULL;
            return 0;
        }
    }

    return -1;
}

int model_sd_music_sync_get_state_snapshot(voice_msg_sd_music_sync_state_t *state)
{
    if (!state) {
        return -1;
    }

    memcpy(state, &g_model_sd_music_sync_ctx.snapshot, sizeof(*state));
    return 0;
}

bool model_sd_music_card_is_available(void)
{
    return g_model_sd_music_sync_ctx.card_available;
}
