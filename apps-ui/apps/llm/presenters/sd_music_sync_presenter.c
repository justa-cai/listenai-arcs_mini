#define LOG_TAG "sd_music_sync_presenter"

#include <string.h>

#include "lisa_ui.h"
#include "lisa_ui_invoke.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"

#include "model_sd_music_sync.h"
#include "sd_music_sync_view.h"

#define SD_MUSIC_SYNC_AUTO_RETURN_MS 5000
#define SD_MUSIC_SYNC_WARNING_AUTO_RETURN_MS 10000

struct sd_music_sync_nav_scr_data {
    lv_obj_t *view;
    voice_msg_sd_music_sync_state_t state;
    lv_timer_t *auto_return_timer;
};

static void model_sd_music_sync_on_state_change(const voice_msg_sd_music_sync_state_t *state,
                                                 void *arg);

static const struct model_sd_music_sync_cb model_sd_music_sync_cbs = {
    .on_sd_music_sync_state_change = model_sd_music_sync_on_state_change,
};

static void sd_music_sync_nav_to_home_ui(void *arg, uint32_t len)
{
    (void)arg;
    (void)len;

    if (lisa_ui_nav_scr_get_top_id() == LISA_UI_NAV_SCR_ID_SD_MUSIC_SYNC) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
    }
}

static void sd_music_sync_auto_return_cb(lv_timer_t *timer)
{
    struct sd_music_sync_nav_scr_data *scr_data = NULL;

    if (!timer) {
        return;
    }

    scr_data = (struct sd_music_sync_nav_scr_data *)timer->user_data;
    if (scr_data) {
        scr_data->auto_return_timer = NULL;
    }

    lisa_ui_invoke_ui_delayed(sd_music_sync_nav_to_home_ui, NULL, 0, 0);
}

static void model_sd_music_sync_on_state_change(const voice_msg_sd_music_sync_state_t *state,
                                                 void *arg)
{
    struct sd_music_sync_nav_scr_data *scr_data = (struct sd_music_sync_nav_scr_data *)arg;

    if (!scr_data || !scr_data->view || !state) {
        return;
    }

    memcpy(&scr_data->state, state, sizeof(*state));

    if (state->state == VOICE_MSG_SD_MUSIC_SYNC_STATE_FINISHED ||
        state->state == VOICE_MSG_SD_MUSIC_SYNC_STATE_IDLE) {
        if (scr_data->auto_return_timer) {
            lv_timer_pause(scr_data->auto_return_timer);
        }
        lisa_ui_invoke_ui_delayed(sd_music_sync_nav_to_home_ui, NULL, 0, 0);
        return;
    }

    lisa_ui_sd_music_sync_view_update(scr_data->view, &scr_data->state);

    if (!scr_data->auto_return_timer) {
        return;
    }

    if (state->state == VOICE_MSG_SD_MUSIC_SYNC_STATE_SUCCESSED ||
        state->state == VOICE_MSG_SD_MUSIC_SYNC_STATE_FAILED) {
        uint32_t auto_return_ms = SD_MUSIC_SYNC_AUTO_RETURN_MS;

        if (state->state == VOICE_MSG_SD_MUSIC_SYNC_STATE_SUCCESSED &&
            state->skipped_count > 0) {
            auto_return_ms = SD_MUSIC_SYNC_WARNING_AUTO_RETURN_MS;
        }
        lv_timer_set_period(scr_data->auto_return_timer, auto_return_ms);
        lv_timer_reset(scr_data->auto_return_timer);
        lv_timer_resume(scr_data->auto_return_timer);
    } else {
        lv_timer_pause(scr_data->auto_return_timer);
    }
}

static int sd_music_sync_nav_scr_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    struct sd_music_sync_nav_scr_data *scr_data;

    LISA_UI_LOGD("nav scr open, id: %d", scr->unique_id);

    model_sd_music_sync_init();

    scr_data = lisa_ui_malloc(sizeof(struct sd_music_sync_nav_scr_data));
    if (!scr_data) {
        return -1;
    }
    memset(scr_data, 0, sizeof(*scr_data));

    scr_data->view = lisa_ui_sd_music_sync_view_create(lv_scr_act());
    if (!scr_data->view) {
        lisa_ui_free(scr_data);
        return -1;
    }

    scr_data->auto_return_timer =
        lv_timer_create(sd_music_sync_auto_return_cb, SD_MUSIC_SYNC_AUTO_RETURN_MS, scr_data);
    if (scr_data->auto_return_timer) {
        lv_timer_set_repeat_count(scr_data->auto_return_timer, 1);
        lv_timer_pause(scr_data->auto_return_timer);
    }

    *data = scr_data;
    model_sd_music_sync_cb_register(&model_sd_music_sync_cbs, scr_data);

    return 0;
}

static int sd_music_sync_nav_scr_show(const struct lisa_ui_nav_scr *scr, void *data)
{
    struct sd_music_sync_nav_scr_data *scr_data = (struct sd_music_sync_nav_scr_data *)data;

    LISA_UI_LOGD("nav scr show, id: %d", scr->unique_id);

    if (!scr_data || !scr_data->view) {
        return -1;
    }

    lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    return 0;
}

static int sd_music_sync_nav_scr_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    struct sd_music_sync_nav_scr_data *scr_data = (struct sd_music_sync_nav_scr_data *)data;

    LISA_UI_LOGD("nav scr close, id: %d", scr->unique_id);

    model_sd_music_sync_cb_unregister(&model_sd_music_sync_cbs);

    if (!scr_data) {
        return 0;
    }

    if (scr_data->auto_return_timer) {
        lv_timer_del(scr_data->auto_return_timer);
        scr_data->auto_return_timer = NULL;
    }
    if (scr_data->view) {
        lv_obj_del(scr_data->view);
    }

    lisa_ui_free(scr_data);
    return 0;
}

const struct lisa_ui_nav_scr sd_music_sync_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_SD_MUSIC_SYNC,
    .open = sd_music_sync_nav_scr_open,
    .show = sd_music_sync_nav_scr_show,
    .close = sd_music_sync_nav_scr_close,
};
