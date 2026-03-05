#include "model_common.h"
#include "lisa_ui.h"

#ifdef LISA_UI_PLATFORM_ARCS
#include "lisa_ui_invoke.h"
#include "service_brightness.h"
#include "service_volume.h"
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
#endif

int model_common_init(void)
{
    if (model_common_ctx.inited) {
        return 0;
    }
#ifdef LISA_UI_PLATFORM_ARCS
    model_common_ctx.brightness = service_brightness_get();
    model_common_ctx.volume = service_volume_get();
    model_common_ctx.inited = 1;
#endif

    LISA_UI_LOGD("Common model initialized");

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
