#include <stdint.h>
#include <stdlib.h>

#define TAG "info_presenter"

#include "lisa_ui.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"
#include "info_view.h"
#include "model_qrcode.h"

struct info_nav_scr_data {
    lv_obj_t *view;          /**< Info view object */
    lv_timer_t *auto_return; /**< Auto return timer */
    lv_timer_t *retry_qr;    /**< QR data retry timer */
};

static void auto_return_timer_cb(lv_timer_t *timer)
{
    LISA_UI_LOGD("Auto return timer triggered");

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
    if (model_qrcode_get_config_data(&qr_data) == 0) {
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
    if (model_qrcode_get_config_data(&qr_data) == 0) {
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

#ifdef CONFIG_BOARD_ARCS_MINI
    scr_data->auto_return = lv_timer_create(auto_return_timer_cb, 30000, NULL);
#else // !CONFIG_BOARD_ARCS_MINI
    scr_data->auto_return = lv_timer_create(auto_return_timer_cb, 10000, NULL);
#endif
    lv_timer_set_repeat_count(scr_data->auto_return, 1);

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
