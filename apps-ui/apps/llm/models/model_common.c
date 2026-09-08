#include "model_common.h"
#include "lisa_ui.h"

#ifdef LISA_UI_PLATFORM_ARCS
#include "lisa_ui_invoke.h"
#include "app_usb_cherry.h"
#include "service_brightness.h"
#include "service_power_policy.h"
#include "service_volume.h"
#include "voice_msg.h"
#endif

#define TAG "model_common"

struct model_common_context {
    uint8_t volume;
    uint8_t brightness;
    uint8_t inited;
};

static struct model_common_context model_common_ctx = {
    .volume = 70,
    .brightness = 70,
    .inited = 0,
};

static model_common_power_state_cb_t s_power_state_cb;
static void *s_power_state_cb_arg;

#ifdef LISA_UI_PLATFORM_ARCS

static void model_volume_set_invoke_bn(void *arg, uint32_t arg_len)
{
    uint8_t volume = *(uint8_t *)arg;
    service_volume_set(volume);
}

static void model_brightness_set_invoke_bn(void *arg, uint32_t arg_len)
{
    uint8_t brightness = *(uint8_t *)arg;
    service_brightness_set(brightness);
}

static void model_brightness_set_temp_invoke_bn(void *arg, uint32_t arg_len)
{
    uint8_t brightness = *(uint8_t *)arg;
    service_brightness_set_temp(brightness);
}

static void model_display_set_blanked_invoke_bn(void *arg, uint32_t arg_len)
{
    bool blanked = *(bool *)arg;
    service_brightness_set_blanked(blanked);
}

static void model_usb_set_suspended_invoke_bn(void *arg, uint32_t arg_len)
{
    bool suspended = *(bool *)arg;

    if (suspended) {
        app_usb_suspend();
    } else {
        app_usb_resume();
    }
}

static void model_power_experiment_set_suspended_invoke_bn(void *arg, uint32_t arg_len)
{
    bool suspended = *(bool *)arg;

    (void)service_power_policy_set_runtime_suspended(suspended);
}

static void model_power_state_changed(void *unused, uint32_t msg_id, void *data,
                                      uint32_t len, void *user_data)
{
    voice_msg_power_policy_state_event_t *event = data;
    voice_msg_power_policy_state_event_t state_event;

    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (event == NULL || len < sizeof(*event)) {
        return;
    }

    state_event = *event;
    LISA_UI_INVOKE_UI_ARG_BASE(state_event, {
        if (s_power_state_cb) {
            s_power_state_cb((model_common_power_state_t)_invoke_state_event.state,
                             _invoke_state_event.reason,
                             s_power_state_cb_arg);
        }
    });
}
#endif

int model_common_init(void)
{
    if (model_common_ctx.inited) {
        return 0;
    }
#ifdef LISA_UI_PLATFORM_ARCS
    model_common_ctx.brightness = service_brightness_get();
    model_common_ctx.volume = service_volume_get();
    voice_msg_sub(VOICE_MSG_POWER_POLICY_STATE_CHANGED,
                  model_power_state_changed, NULL);
    model_common_ctx.inited = 1;
#endif

    LISA_UI_LOGD("Common model initialized");

    return 0;
}

int model_common_display_set_blanked(bool blanked)
{
#ifdef LISA_UI_PLATFORM_ARCS
    LISA_UI_INVOKE_BN(model_display_set_blanked_invoke_bn, &blanked, sizeof(blanked));
#endif

    LISA_UI_LOGD("Display blanking requested: %d", blanked);
    return 0;
}

int model_common_usb_set_suspended(bool suspended)
{
#ifdef LISA_UI_PLATFORM_ARCS
    LISA_UI_INVOKE_BN(model_usb_set_suspended_invoke_bn, &suspended, sizeof(suspended));
#endif

    LISA_UI_LOGD("USB suspend requested: %d", suspended);
    return 0;
}

int model_common_power_experiment_set_suspended(bool suspended)
{
#ifdef LISA_UI_PLATFORM_ARCS
    LISA_UI_INVOKE_BN(model_power_experiment_set_suspended_invoke_bn,
                      &suspended, sizeof(suspended));
#endif

    LISA_UI_LOGD("Power experiment suspend requested: %d", suspended);
    return 0;
}

int model_common_power_state_cb_register(model_common_power_state_cb_t cb, void *arg)
{
    s_power_state_cb = cb;
    s_power_state_cb_arg = arg;

#ifdef LISA_UI_PLATFORM_ARCS
    if (cb) {
        cb((model_common_power_state_t)service_power_policy_get_state(),
           VOICE_MSG_POWER_POLICY_REASON_INIT, arg);
    }
#endif
    return 0;
}

int model_common_power_state_cb_unregister(model_common_power_state_cb_t cb)
{
    if (s_power_state_cb == cb) {
        s_power_state_cb = NULL;
        s_power_state_cb_arg = NULL;
    }
    return 0;
}

int model_common_sync_from_system(void)
{
#ifdef LISA_UI_PLATFORM_ARCS
    model_common_ctx.volume = service_volume_get();
    model_common_ctx.brightness = service_brightness_get();
    return 0;
#else
    return -1;
#endif
}

uint8_t model_common_volume_get(void)
{
    return model_common_ctx.volume;
}

int model_common_volume_set(uint8_t volume)
{
    if (volume > 100) {
        volume = 100;
    }

    model_common_ctx.volume = volume;

#ifdef LISA_UI_PLATFORM_ARCS
    LISA_UI_INVOKE_BN(model_volume_set_invoke_bn, &volume, sizeof(volume));
#endif

    LISA_UI_LOGD("Volume set to: %d", volume);

    return 0;
}

uint8_t model_common_brightness_get(void)
{
    return model_common_ctx.brightness;
}

int model_common_brightness_set(uint8_t brightness)
{
    if (brightness > 100) {
        LISA_UI_LOGE("Invalid brightness: %d", brightness);
        return -1;
    }

    model_common_ctx.brightness = brightness;

#ifdef LISA_UI_PLATFORM_ARCS
    LISA_UI_INVOKE_BN(model_brightness_set_invoke_bn, &brightness, sizeof(brightness));
#endif

    LISA_UI_LOGD("Brightness set to: %d", brightness);

    return 0;
}

int model_common_brightness_set_temp(uint8_t brightness)
{
    if (brightness > 100) {
        LISA_UI_LOGE("Invalid temporary brightness: %d", brightness);
        return -1;
    }

#ifdef LISA_UI_PLATFORM_ARCS
    LISA_UI_INVOKE_BN(model_brightness_set_temp_invoke_bn, &brightness, sizeof(brightness));
#endif

    LISA_UI_LOGD("Temporary brightness applied: %d", brightness);

    return 0;
}
