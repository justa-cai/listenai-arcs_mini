#define LOG_TAG "sip_call_presenter"

#include <string.h>

#include "lisa_ui.h"
#include "lisa_ui_invoke.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"

#include "model_sip_call.h"
#include "sip_call_view.h"

/*
 * SIP 通话页：open 创建视图（服务事件由模型层桥接推送），
 * pause 即挂断 —— 实时通话离开页面就没有继续的意义
 * （与 video_player_presenter 的"离开页面即停播"同一语义）。
 */

struct sip_nav_scr_data {
    lv_obj_t *view;
};

static int sip_call_nav_scr_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    LISA_UI_LOGD("nav scr open, id: %d", scr->unique_id);

    struct sip_nav_scr_data *scr_data = lisa_ui_malloc(sizeof(struct sip_nav_scr_data));
    if (!scr_data) {
        return -1;
    }
    memset(scr_data, 0, sizeof(*scr_data));

    scr_data->view = lisa_ui_sip_view_create(lv_scr_act());
    if (!scr_data->view) {
        lisa_ui_free(scr_data);
        return -1;
    }

    *data = scr_data;
    return 0;
}

static int sip_call_nav_scr_show(const struct lisa_ui_nav_scr *scr, void *data)
{
    struct sip_nav_scr_data *scr_data = data;

    LISA_UI_LOGD("nav scr show, id: %d", scr->unique_id);
    if (!scr_data || !scr_data->view) {
        return -1;
    }
    lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    return 0;
}

static int sip_call_nav_scr_pause(const struct lisa_ui_nav_scr *scr, void *data)
{
    (void)scr;
    (void)data;
    LISA_UI_LOGD("nav scr pause: hang up");
    model_sip_call_hangup();
    return 0;
}

static int sip_call_nav_scr_resume(const struct lisa_ui_nav_scr *scr, void *data)
{
    (void)scr;
    (void)data;
    return 0;
}

static int sip_call_nav_scr_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    struct sip_nav_scr_data *scr_data = data;

    LISA_UI_LOGD("nav scr close, id: %d", scr->unique_id);
    if (!scr_data) {
        return 0;
    }
    if (scr_data->view) {
        lv_obj_del(scr_data->view);
    }
    lisa_ui_free(scr_data);
    return 0;
}

const struct lisa_ui_nav_scr sip_call_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_SIP_CALL,
    .open = sip_call_nav_scr_open,
    .show = sip_call_nav_scr_show,
    .pause = sip_call_nav_scr_pause,
    .resume = sip_call_nav_scr_resume,
    .close = sip_call_nav_scr_close,
};
