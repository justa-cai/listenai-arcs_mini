#include "model_ota.h"

#include "lisa_ui.h"
#include "lisa_ui_invoke.h"
#include "lisa_ui_nav_scr_ids.h"

#include "voice_msg.h"
#include "ota_manager.h"

struct model_ota_context {
    const struct model_ota_cb *cbs;
    void *arg;
};

static struct model_ota_context model_ota_ctx;

static void handle_ota_state_change(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    ota_state_t *state = (ota_state_t *)data;
    LISA_UI_INVOKE_UI_ARG_PTR(state, sizeof(ota_state_t), {
        if (model_ota_ctx.cbs && model_ota_ctx.cbs->on_ota_state_change) {
            model_ota_ctx.cbs->on_ota_state_change(_invoke_state, model_ota_ctx.arg);
        }
    });
}

int model_ota_init(void)
{
    voice_msg_sub(VOICE_MSG_OTA_UPDATING, handle_ota_state_change, NULL);
    voice_msg_sub(VOICE_MSG_OTA_SUCCESSED, handle_ota_state_change, NULL);
    voice_msg_sub(VOICE_MSG_OTA_FAILED, handle_ota_state_change, NULL);
    return 0;
}

int model_ota_cb_register(const struct model_ota_cb *cb, void *arg)
{
    model_ota_ctx.cbs = cb;
    model_ota_ctx.arg = arg;
    return 0;
}

int model_ota_cb_unregister(const struct model_ota_cb *cb)
{
    model_ota_ctx.cbs = NULL;
    model_ota_ctx.arg = NULL;
    return 0;
}
