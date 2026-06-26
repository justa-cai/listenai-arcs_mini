#include "stdint.h"
#include "stdbool.h"

#define TAG "service_image"

#include "lisa_log.h"
#include "lisa_mutex.h"

#include "voice_msg.h"
#include "service_image.h"

/* ---- 模块级状态 --------------------------------------------------------- */

typedef struct {
    bool inited;
    enum {
        SERVICE_IMAGE_STATE_INIT = 0,
        SERVICE_IMAGE_STATE_WAITING,
        SERVICE_IMAGE_STATE_CANCELED,
    } state;
    lisa_mutex_t *lock;
} service_image_ctx_t;

static service_image_ctx_t g_service_image = {0};

/* ---- 公开 API ----------------------------------------------------------- */

int service_image_init(void)
{
    if (g_service_image.inited) {
        return 0;
    }

    g_service_image.lock = lisa_mutex_create();
    if (!g_service_image.lock) {
        LOGE("create image service mutex failed");
        return -1;
    }

    g_service_image.state = SERVICE_IMAGE_STATE_INIT;
    g_service_image.inited = true;
    return 0;
}

void service_image_waiting_start(void)
{
    if (!g_service_image.inited || !g_service_image.lock) {
        return;
    }

    lisa_mutex_lock(g_service_image.lock, LISA_WAIT_FOREVER);
    g_service_image.state = SERVICE_IMAGE_STATE_WAITING;
    lisa_mutex_unlock(g_service_image.lock);

    LOGI("image waiting start");
}

/*
 * 取消当前图片等待状态，并通知云端丢弃待发送的图片。
 *
 * 在按键、唤醒等用户主动交互事件发生时调用，确保过时的图片不会
 * 被注入到新的对话上下文中。
 */
void service_image_waiting_cancel(void)
{
    if (!g_service_image.inited || !g_service_image.lock) {
        return;
    }

    lisa_mutex_lock(g_service_image.lock, LISA_WAIT_FOREVER);
    if (g_service_image.state != SERVICE_IMAGE_STATE_WAITING) {
        lisa_mutex_unlock(g_service_image.lock);
        return;
    }
    g_service_image.state = SERVICE_IMAGE_STATE_CANCELED;
    lisa_mutex_unlock(g_service_image.lock);

    voice_msg_pub(VOICE_MSG_CLOUD_MCP_LOADING, NULL, 0);
    LOGI("image waiting cancel, drop next image");
}

/*
 * 查询当前是否允许发送图片。
 *
 * 仅在 WAITING 或 INIT 状态返回 true，CANCELED 状态返回 false。
 * 调用后会重置状态为 INIT，因此每次图片发送前只能调用一次。
 */
bool service_image_waiting_get(void)
{
    bool allowed = false;

    if (!g_service_image.inited || !g_service_image.lock) {
        return false;
    }

    lisa_mutex_lock(g_service_image.lock, LISA_WAIT_FOREVER);
    if (g_service_image.state == SERVICE_IMAGE_STATE_WAITING) {
        allowed = true;
        g_service_image.state = SERVICE_IMAGE_STATE_INIT;
    } else if (g_service_image.state == SERVICE_IMAGE_STATE_CANCELED) {
        allowed = false;
        g_service_image.state = SERVICE_IMAGE_STATE_INIT;
    } else if (g_service_image.state == SERVICE_IMAGE_STATE_INIT) {
        allowed = true;
    }
    lisa_mutex_unlock(g_service_image.lock);

    return allowed;
}
