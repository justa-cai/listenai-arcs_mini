#define LOG_TAG "log_upload_presenter"

#include <errno.h>
#include <string.h>

#include "lisa_ui.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"

#include "log_upload.h"
#include "log_upload_view.h"
#include "model_log_upload.h"

#define LOG_UPLOAD_SUCCESS_AUTO_RETURN_MS 3000

struct log_upload_nav_scr_data {
    lv_obj_t *view;
    log_upload_state_t state;
    lv_timer_t *success_auto_return_timer;
};

static void model_log_upload_on_state_change(const log_upload_state_t *state, void *arg);

static const struct model_log_upload_cb model_log_upload_cbs = {
    .on_log_upload_state_change = model_log_upload_on_state_change,
};

static void log_upload_nav_to_home_ui(void *arg, uint32_t len)
{
    (void)arg;
    (void)len;

    if (lisa_ui_nav_scr_get_top_id() == LISA_UI_NAV_SCR_ID_LOG_UPLOAD) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
    }
}

static void log_upload_success_auto_return_cb(lv_timer_t *timer)
{
    struct log_upload_nav_scr_data *scr_data = NULL;

    if (!timer) {
        return;
    }

    scr_data = (struct log_upload_nav_scr_data *)timer->user_data;
    if (scr_data) {
        scr_data->success_auto_return_timer = NULL;
    }

    lisa_ui_invoke_ui_delayed(log_upload_nav_to_home_ui, NULL, 0, 0);
}

static void model_log_upload_on_state_change(const log_upload_state_t *state, void *arg)
{
    struct log_upload_nav_scr_data *scr_data = (struct log_upload_nav_scr_data *)arg;

    if (!scr_data || !scr_data->view || !state) {
        return;
    }

    memcpy(&scr_data->state, state, sizeof(*state));
    lisa_ui_log_upload_view_update(scr_data->view, &scr_data->state);

    if (!scr_data->success_auto_return_timer) {
        return;
    }

    if (state->state == LOG_UPLOAD_STATE_SUCCESSED || state->state == LOG_UPLOAD_STATE_FAILED) {
        lv_timer_reset(scr_data->success_auto_return_timer);
        lv_timer_resume(scr_data->success_auto_return_timer);
    } else {
        lv_timer_pause(scr_data->success_auto_return_timer);
    }
}

static int log_upload_nav_scr_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    struct log_upload_nav_scr_data *scr_data;

    LISA_UI_LOGD("nav scr open, id: %d", scr->unique_id);

    model_log_upload_init();

    scr_data = lisa_ui_malloc(sizeof(struct log_upload_nav_scr_data));
    if (!scr_data) {
        return -1;
    }
    memset(scr_data, 0, sizeof(*scr_data));

    scr_data->view = lisa_ui_log_upload_view_create(lv_scr_act());
    if (!scr_data->view) {
        lisa_ui_free(scr_data);
        return -1;
    }

    scr_data->success_auto_return_timer =
        lv_timer_create(log_upload_success_auto_return_cb, LOG_UPLOAD_SUCCESS_AUTO_RETURN_MS, scr_data);
    if (scr_data->success_auto_return_timer) {
        lv_timer_set_repeat_count(scr_data->success_auto_return_timer, 1);
        lv_timer_pause(scr_data->success_auto_return_timer);
    }

    *data = scr_data;
    model_log_upload_cb_register(&model_log_upload_cbs, scr_data);

    return 0;
}

static int log_upload_nav_scr_show(const struct lisa_ui_nav_scr *scr, void *data)
{
    struct log_upload_nav_scr_data *scr_data = (struct log_upload_nav_scr_data *)data;

    LISA_UI_LOGD("nav scr show, id: %d", scr->unique_id);

    if (!scr_data || !scr_data->view) {
        return -1;
    }

    lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    return 0;
}

static int log_upload_nav_scr_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    struct log_upload_nav_scr_data *scr_data = (struct log_upload_nav_scr_data *)data;

    LISA_UI_LOGD("nav scr close, id: %d", scr->unique_id);

    model_log_upload_cb_unregister(&model_log_upload_cbs);

    if (!scr_data) {
        return 0;
    }

    if (scr_data->success_auto_return_timer) {
        lv_timer_del(scr_data->success_auto_return_timer);
        scr_data->success_auto_return_timer = NULL;
    }
    if (scr_data->view) {
        lv_obj_del(scr_data->view);
    }

    lisa_ui_free(scr_data);
    return 0;
}

const struct lisa_ui_nav_scr log_upload_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_LOG_UPLOAD,
    .open = log_upload_nav_scr_open,
    .show = log_upload_nav_scr_show,
    .close = log_upload_nav_scr_close,
};
