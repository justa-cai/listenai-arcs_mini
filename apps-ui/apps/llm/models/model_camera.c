#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#define TAG "model_camera"

#include "lisa_ui_invoke.h"
#include "lisa_ui_log.h"

#ifdef LISA_UI_PLATFORM_ARCS
#include "service_camera.h"
#include "voice_msg.h"
#endif

struct model_camera_context {
    uint32_t inited: 1;
    uint32_t running: 1;
};

static struct model_camera_context model_camera_ctx = {
    .inited = 0,
    .running = 0,
};

#ifdef LISA_UI_PLATFORM_ARCS
static void bn_camera_hw_init(void *data, uint32_t len, struct voice_invoke_rsp *rsp)
{
    int r = service_camera_init();
    rsp->err = r;
}
#endif

int model_camera_init(void)
{
    if (model_camera_ctx.inited) {
        LISA_UI_LOGW("Camera model already initialized");
        return 0;
    }

#ifdef LISA_UI_PLATFORM_ARCS
    struct voice_invoke_rsp rsp = {
        .err = -1,
    };

    int r = voice_invoke_sync(bn_camera_hw_init, NULL, 0, &rsp, 1000);
    if (r) {
        LISA_UI_LOGE("bn_camera_hw_init sync invoke failed, r:%d", r);
    } else {
        if (rsp.err) {
            LISA_UI_LOGE("bn_camera_hw_init sync invoke resp, r:%d", r);
        }
    }
#else
    LISA_UI_LOGW("Camera not supported on this platform");
#endif

    model_camera_ctx.inited = 1;
    LISA_UI_LOGD("Camera model initialized");

    return 0;
}

#ifdef LISA_UI_PLATFORM_ARCS
struct service_camera_rsp {
    struct voice_invoke_rsp base;
    uint8_t *cap_buf;
    uint32_t buf_len;
};

static void bn_camera_capture(void *data, uint32_t data_len, struct voice_invoke_rsp *rsp)
{
    struct service_camera_rsp *camera_rsp = (struct service_camera_rsp *)rsp;

    if (camera_rsp->cap_buf == NULL) {
        LISA_UI_LOGE("cap buf is null");
        camera_rsp->base.err = -1;
        return;
    }

    int ret = service_camera_capture(camera_rsp->cap_buf, camera_rsp->buf_len);
    camera_rsp->base.err = ret;
}
#endif

int model_camera_capture(uint8_t *in, uint32_t len)
{
#ifdef LISA_UI_PLATFORM_ARCS
    if (!model_camera_ctx.inited) {
        LISA_UI_LOGE("Camera model not initialized");
        return -1;
    }

    struct service_camera_rsp rsp = {
        .base = {
            .err = -1,
        },
        .cap_buf = in,
        .buf_len = len,
    };

    int r = voice_invoke_sync(bn_camera_capture, NULL, 0, (struct voice_invoke_rsp *)&rsp, 1000);
    if (r) {
        LISA_UI_LOGE("camera cap invoke sync failed, r: %d", r);
        return r;
    } else {
        if (rsp.base.err) {
            LISA_UI_LOGE("camera cap invoke sync rsp err, r: %d", rsp.base.err);
            return rsp.base.err;
        }
    }
#else
    LISA_UI_LOGW("Camera not supported on this platform");
    return -5;
#endif

    return 0;
}

int model_camera_get_framesize(uint16_t *width, uint16_t *height)
{
    if (!model_camera_ctx.inited) {
        LISA_UI_LOGE("Camera model not initialized");
        return -1;
    }

#ifdef LISA_UI_PLATFORM_ARCS
    return service_camera_get_framesize(width, height);
#else
    if (width && height) {
        *width = 0;
        *height = 0;
    }
    return 0;
#endif
}

bool model_camera_is_running(void)
{
    return model_camera_ctx.running;
}

bool model_camera_is_inited(void)
{
    return model_camera_ctx.inited;
}
