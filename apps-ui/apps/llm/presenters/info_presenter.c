#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define TAG "info_presenter"

#include "lisa_ui.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"
#include "info_view.h"
#include "model_qrcode.h"
#include "voice_cloud.h"

struct info_nav_scr_data {
    lv_obj_t *view;          /**< Info view object */
    lv_timer_t *auto_return; /**< Auto return timer */
    lv_timer_t *retry_qr;    /**< QR data retry timer */
    lv_timer_t *guard_timer; /**< Provisioning guard timer */
    bool provision_locked;   /**< Whether provisioning page is currently forced */
};

static void auto_return_timer_cb(lv_timer_t *timer);

static bool info_should_lock_page_by_cloud_state(void)
{
    voice_cloud_state_t cloud_state = voice_cloud_get_state();

    if (cloud_state == VOICE_CLOUD_STATE_CONNECTED) {
        return false;
    }

    switch (cloud_state) {
    case VOICE_CLOUD_STATE_CONNECTING:
        LISA_UI_LOGI("Info page not locked: cloud state CONNECTING");
        return false;
    case VOICE_CLOUD_STATE_NO_NETWORK:
        LISA_UI_LOGI("Keep info page: cloud state NO_NETWORK");
        return true;
    case VOICE_CLOUD_STATE_NO_INTERNET:
        LISA_UI_LOGI("Keep info page: cloud state NO_INTERNET");
        return true;
    case VOICE_CLOUD_STATE_TOKEN_FAILED:
        LISA_UI_LOGI("Keep info page: cloud state TOKEN_FAILED");
        return true;
    case VOICE_CLOUD_STATE_CONNECT_FAILED:
        LISA_UI_LOGI("Keep info page: cloud state CONNECT_FAILED");
        return true;
    case VOICE_CLOUD_STATE_CONNECTED:
    default:
        return false;
    }
}

static void info_auto_return_start(struct info_nav_scr_data *scr_data)
{
    if (!scr_data || scr_data->auto_return) {
        return;
    }

#ifdef CONFIG_BOARD_ARCS_MINI
    scr_data->auto_return = lv_timer_create(auto_return_timer_cb, 30000, scr_data);
#else
    scr_data->auto_return = lv_timer_create(auto_return_timer_cb, 10000, scr_data);
#endif
    if (scr_data->auto_return) {
        lv_timer_set_repeat_count(scr_data->auto_return, 1);
    }
}

static void auto_return_timer_cb(lv_timer_t *timer)
{
    struct info_nav_scr_data *scr_data = (struct info_nav_scr_data *)timer->user_data;

    LISA_UI_LOGD("Auto return timer triggered");

    /* repeat_count=1，LVGL 会在本回调返回后自动释放 timer，
     * 提前清空指针避免 close 时 double-free */
    if (scr_data) {
        scr_data->auto_return = NULL;
    }

    if (info_should_lock_page_by_cloud_state()) {
        LISA_UI_LOGI("Info page is locked by cloud state, skip auto return");
        return;
    }

    lisa_ui_nav_scr_nav_back();
}

static void retry_qr_timer_cb(lv_timer_t *timer)
{
    LISA_UI_LOGD("Retry QR timer triggered");

    struct info_nav_scr_data *scr_data = (struct info_nav_scr_data *)timer->user_data;
    if (!scr_data || !scr_data->view) {
        LISA_UI_LOGE("Invalid scr_data in retry timer");
        return;
    }

    qrcode_data_t qr_data;
    if (model_qrcode_get_data(&qr_data) == 0) {
        lisa_ui_info_view_set_top_text(scr_data->view, qr_data.top_text);
        lisa_ui_info_view_set_bottom_text(scr_data->view, qr_data.bottom_text);

        if (qr_data.qr_image) {
            lisa_ui_info_view_set_qr_image(scr_data->view, qr_data.qr_image);
            if (scr_data->retry_qr) {
                lv_timer_del(scr_data->retry_qr);
                scr_data->retry_qr = NULL;
            }
        }
    } else {
        LISA_UI_LOGE("Failed to get QR data on retry, will retry again");
    }
}

static void guard_timer_cb(lv_timer_t *timer)
{
    struct info_nav_scr_data *scr_data = (struct info_nav_scr_data *)timer->user_data;

    if (!scr_data || !scr_data->view) {
        return;
    }

    if (info_should_lock_page_by_cloud_state()) {
        return;
    }

    scr_data->provision_locked = false;
    lisa_ui_nav_scr_nav_back();
}

static int info_nav_scr_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    LISA_UI_LOGD("Info nav scr open, id: %d", scr->unique_id);

    struct info_nav_scr_data *scr_data = lisa_ui_malloc(sizeof(struct info_nav_scr_data));

    if (!scr_data) {
        LISA_UI_LOGE("Failed to allocate memory for info_nav_scr_data");
        return -1;
    }
    memset(scr_data, 0, sizeof(struct info_nav_scr_data));

    scr_data->view = lisa_ui_info_view_create(lv_scr_act());
    if (!scr_data->view) {
        LISA_UI_LOGE("Failed to create info view");
        lisa_ui_free(scr_data);
        return -1;
    }

    qrcode_data_t qr_data;
    if (model_qrcode_get_data(&qr_data) == 0) {
        lisa_ui_info_view_set_top_text(scr_data->view, qr_data.top_text);
        lisa_ui_info_view_set_bottom_text(scr_data->view, qr_data.bottom_text);

        if (qr_data.qr_image) {
            lisa_ui_info_view_set_qr_image(scr_data->view, qr_data.qr_image);
        } else {
            LISA_UI_LOGD("QR image not ready, creating retry timer");
            scr_data->retry_qr = lv_timer_create(retry_qr_timer_cb, 100, scr_data);
        }
    } else {
        LISA_UI_LOGE("Failed to get QR data from model");
    }

    scr_data->provision_locked = info_should_lock_page_by_cloud_state();

    if (scr_data->provision_locked) {
        scr_data->guard_timer = lv_timer_create(guard_timer_cb, 1000, scr_data);
    } else {
        info_auto_return_start(scr_data);
    }

    *data = scr_data;
    return 0;
}

static int info_nav_scr_show(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("Info nav scr show, id: %d", scr->unique_id);

    struct info_nav_scr_data *scr_data = (struct info_nav_scr_data *)data;
    if (!scr_data || !scr_data->view) {
        LISA_UI_LOGE("Invalid info nav scr data");
        return -1;
    }

    lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);

    return 0;
}

static int info_nav_scr_pause(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("Info nav scr pause, id: %d", scr->unique_id);

    struct info_nav_scr_data *scr_data = (struct info_nav_scr_data *)data;
    if (scr_data && scr_data->view) {
        lv_obj_add_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    }

    return 0;
}

static int info_nav_scr_resume(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("Info nav scr resume, id: %d", scr->unique_id);

    struct info_nav_scr_data *scr_data = (struct info_nav_scr_data *)data;
    if (scr_data && scr_data->view) {
        lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    }

    return 0;
}

static int info_nav_scr_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("Info nav scr close, id: %d", scr->unique_id);

    struct info_nav_scr_data *scr_data = (struct info_nav_scr_data *)data;
    if (scr_data) {
        if (scr_data->auto_return) {
            lv_timer_del(scr_data->auto_return);
            scr_data->auto_return = NULL;
        }

        if (scr_data->retry_qr) {
            lv_timer_del(scr_data->retry_qr);
            scr_data->retry_qr = NULL;
        }

        if (scr_data->guard_timer) {
            lv_timer_del(scr_data->guard_timer);
            scr_data->guard_timer = NULL;
        }

        if (scr_data->view) {
            lv_obj_del(scr_data->view);
        }

        lisa_ui_free(scr_data);
    }

    return 0;
}

const struct lisa_ui_nav_scr info_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_INFO,
    .open = info_nav_scr_open,
    .show = info_nav_scr_show,
    .pause = info_nav_scr_pause,
    .resume = info_nav_scr_resume,
    .close = info_nav_scr_close,
};
