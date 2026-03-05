#include "quota_qrcode_presenter.h"
#include "quota_qrcode_view.h"
#include "model_qrcode.h"
#include "lisa_ui.h"
#include "lisa_ui_nav_scr_ids.h"
#include "lvgl.h"

#define TAG "quota_qrcode_presenter"

struct quota_qrcode_nav_scr_data {
    lv_obj_t *view;
    lv_timer_t *retry_qr;
    lv_timer_t *auto_return;
};

static void retry_qr_timer_cb(lv_timer_t *timer)
{
    struct quota_qrcode_nav_scr_data *scr_data = timer->user_data;
    
    qrcode_data_t qr_data;
    if (model_qrcode_get_quota_data(&qr_data) == 0) {
        lisa_ui_quota_qrcode_view_set_title(scr_data->view, qr_data.top_text);
        lisa_ui_quota_qrcode_view_set_message(scr_data->view, qr_data.bottom_text);

        if (qr_data.qr_image) {
            lisa_ui_quota_qrcode_view_set_qr_image(scr_data->view, qr_data.qr_image);
            lv_timer_del(timer);
            scr_data->retry_qr = NULL;
            LISA_UI_LOGI("QR code image loaded successfully");
        }
    }
}

static void auto_return_timer_cb(lv_timer_t *timer)
{
    LISA_UI_LOGI("Auto return to home page");
    lisa_ui_nav_scr_nav_back();
}

static int quota_qrcode_nav_scr_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    struct quota_qrcode_nav_scr_data *scr_data = lisa_ui_malloc(sizeof(struct quota_qrcode_nav_scr_data));
    memset(scr_data, 0, sizeof(struct quota_qrcode_nav_scr_data));

    scr_data->view = lisa_ui_quota_qrcode_view_create(lv_scr_act());

    qrcode_data_t qr_data;
    if (model_qrcode_get_quota_data(&qr_data) == 0) {
        lisa_ui_quota_qrcode_view_set_title(scr_data->view, qr_data.top_text);
        lisa_ui_quota_qrcode_view_set_message(scr_data->view, qr_data.bottom_text);

        if (qr_data.qr_image) {
            lisa_ui_quota_qrcode_view_set_qr_image(scr_data->view, qr_data.qr_image);
            LISA_UI_LOGI("QR code image set immediately");
        } else {
            scr_data->retry_qr = lv_timer_create(retry_qr_timer_cb, 100, scr_data);
            LISA_UI_LOGI("QR code image not ready, will retry");
        }
    } else {
        LISA_UI_LOGE("Failed to get QR code data");
    }

    scr_data->auto_return = lv_timer_create(auto_return_timer_cb, 10000, NULL);
    lv_timer_set_repeat_count(scr_data->auto_return, 1);

    *data = scr_data;
    LISA_UI_LOGI("Quota QR code page opened");
    return 0;
}

static int quota_qrcode_nav_scr_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    struct quota_qrcode_nav_scr_data *scr_data = data;

    if (scr_data->retry_qr) {
        lv_timer_del(scr_data->retry_qr);
    }

    if (scr_data->auto_return) {
        lv_timer_del(scr_data->auto_return);
    }

    if (scr_data->view) {
        lv_obj_del(scr_data->view);
    }

    lisa_ui_free(scr_data);
    LISA_UI_LOGI("Quota QR code page closed");
    return 0;
}

const struct lisa_ui_nav_scr quota_qrcode_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_QUOTA_QRCODE,
    .open = quota_qrcode_nav_scr_open,
    .close = quota_qrcode_nav_scr_close,
};
