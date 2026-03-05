#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define TAG "model_battery"

#include "lisa_ui_invoke.h"
#include "lisa_ui_log.h"

#include "model_battery.h"

#ifdef LISA_UI_PLATFORM_ARCS
#include "voice_msg.h"
#endif

struct model_battery_context {
    uint32_t inited: 1;

    model_battery_info_t info;
    model_battery_update_cb_t on_battery_status_update;
    void *arg;
};

static struct model_battery_context model_battery_ctx = {
    .inited = 0,
    .info = {
        .level = 0,
        .status = MODEL_BATTERY_STATUS_UNKNOWN,
    },
    .on_battery_status_update = NULL,
    .arg = NULL,
};

#ifdef LISA_UI_PLATFORM_ARCS

static model_battery_status_t convert_battery_status(voice_msg_battery_status_t status)
{
    switch (status) {
    case VOICE_MSG_BATTERY_STATUS_NO_BATTERY:
        return MODEL_BATTERY_STATUS_NO_BATTERY;
    case VOICE_MSG_BATTERY_STATUS_NOT_CONNECT:
        return MODEL_BATTERY_STATUS_NOT_CONNECT;
    case VOICE_MSG_BATTERY_STATUS_CHARGING:
        return MODEL_BATTERY_STATUS_CHARGING;
    case VOICE_MSG_BATTERY_STATUS_CHARGE_DONE:
        return MODEL_BATTERY_STATUS_CHARGE_DONE;
    case VOICE_MSG_BATTERY_STATUS_UNKNOWN:
    default:
        return MODEL_BATTERY_STATUS_UNKNOWN;
    }
}

static void battery_update_msg_handle(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (data == NULL || len < sizeof(voice_msg_battery_info_t)) {
        return;
    }

    voice_msg_battery_info_t msg = *(voice_msg_battery_info_t *)data;

    LISA_UI_INVOKE_UI_ARG_BASE(msg, {
        model_battery_ctx.info.level = _invoke_msg.level;
        model_battery_ctx.info.status = convert_battery_status((voice_msg_battery_status_t)_invoke_msg.status);

        if (model_battery_ctx.on_battery_status_update) {
            model_battery_ctx.on_battery_status_update(&model_battery_ctx.info, model_battery_ctx.arg);
        }
    });
}

#endif /* LISA_UI_PLATFORM_ARCS */

int model_battery_init(void)
{
    if (model_battery_ctx.inited) {
        return 0;
    }

#ifdef LISA_UI_PLATFORM_ARCS
    voice_msg_sub(VOICE_MSG_POWER_BATTERY_UPDATE, battery_update_msg_handle, NULL);
#endif

    model_battery_ctx.inited = 1;

    return 0;
}

int model_battery_deinit(void)
{
    if (!model_battery_ctx.inited) {
        return 0;
    }

#ifdef LISA_UI_PLATFORM_ARCS
    voice_msg_unsub(VOICE_MSG_POWER_BATTERY_UPDATE, battery_update_msg_handle);
#endif

    model_battery_ctx.inited = 0;
    model_battery_ctx.on_battery_status_update = NULL;
    model_battery_ctx.arg = NULL;

    return 0;
}

int model_battery_get_info(model_battery_info_t *info)
{
    if (info == NULL) {
        return -1;
    }

    *info = model_battery_ctx.info;
    return 0;
}

int model_battery_cb_register(model_battery_update_cb_t cb, void *arg)
{
    model_battery_ctx.on_battery_status_update = cb;
    model_battery_ctx.arg = arg;
    return 0;
}

int model_battery_cb_unregister(model_battery_update_cb_t cb)
{
    (void)cb;
    model_battery_ctx.on_battery_status_update = NULL;
    model_battery_ctx.arg = NULL;
    return 0;
}
