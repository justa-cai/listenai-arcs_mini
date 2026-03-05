#define LOG_TAG "ota_presenter"

#include "lisa_ui.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"

#include "model_ota.h"
#include "ota_manager.h"
#include "ota_view.h"

struct ota_nav_scr_data {
    lv_obj_t *view;
    ota_state_t state;
};

static void model_ota_on_state_change(const ota_state_t *state, void *arg);

const struct model_ota_cb model_ota_cbs = {
    .on_ota_state_change = model_ota_on_state_change,
};

static void model_ota_on_state_change(const ota_state_t *state, void *arg)
{
    struct ota_nav_scr_data *scr_data = (struct ota_nav_scr_data *)arg;
    if (!scr_data || !scr_data->view || !state) {
        return;
    }

    memcpy(&scr_data->state, state, sizeof(ota_state_t));

    lisa_ui_ota_view_update(scr_data->view, &scr_data->state);
}

static int ota_nav_scr_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    LISA_UI_LOGD("nav scr open, id: %d", scr->unique_id);

    model_ota_init();

    struct ota_nav_scr_data *scr_data = lisa_ui_malloc(sizeof(struct ota_nav_scr_data));
    if (!scr_data) {
        return -1;
    }
    memset(scr_data, 0, sizeof(struct ota_nav_scr_data));

    scr_data->view = lisa_ui_ota_view_create(lv_scr_act());

    *data = scr_data;

    model_ota_cb_register(&model_ota_cbs, scr_data);

    return 0;
}

static int ota_nav_scr_show(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("nav scr show, id: %d", scr->unique_id);

    struct ota_nav_scr_data *scr_data = (struct ota_nav_scr_data *)data;
    if (!scr_data || !scr_data->view) {
        return -1;
    }

    lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);

    return 0;
}

static int ota_nav_scr_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("nav scr close, id: %d", scr->unique_id);

    model_ota_cb_unregister(&model_ota_cbs);

    struct ota_nav_scr_data *scr_data = (struct ota_nav_scr_data *)data;
    if (scr_data) {
        if (scr_data->view) {
            lv_obj_del(scr_data->view);
        }
        lisa_ui_free(scr_data);
    }

    return 0;
}

const struct lisa_ui_nav_scr ota_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_OTA,
    .open = ota_nav_scr_open,
    .show = ota_nav_scr_show,
    .close = ota_nav_scr_close,
};
