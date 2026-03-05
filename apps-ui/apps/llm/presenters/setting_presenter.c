/**
 * @file setting_presenter.c
 * @brief Setting main page presenter
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TAG "setting_presenter"

#include "lisa_ui.h"
#include "lisa_ui_llm_base.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"
#include "lisa_ui_assets.h"
#include "setting_view.h"

struct setting_nav_scr_data {
    lv_obj_t *view;
};

/**
 * @brief Setting item click callback - handles navigation to sub-pages
 * This is business logic in presenter layer
 */
static void setting_item_click_cb(const char *name, void *user_data)
{
    (void)user_data;
    LISA_UI_LOGD("Setting item clicked: %s", name);
    
    // Navigate to corresponding setting page based on name
    if (strcmp(name, _("common setting")) == 0) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_SETTING_COMMON);
    } else if (strcmp(name, _("network setting")) == 0) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_SETTING_WIFI);
    } else if (strcmp(name, _("wakeup setting")) == 0) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_SETTING_WAKEUP);
    } else if (strcmp(name, _("alarm setting")) == 0) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_SETTING_CLOCK);
    }
}

/**
 * @brief Back button click callback - handles navigation back to home
 * This is business logic in presenter layer
 */
static void setting_back_btn_cb(void *user_data)
{
    (void)user_data;
    LISA_UI_LOGD("Setting back button clicked, returning to home");
    lisa_ui_nav_scr_nav_back();
}

static int setting_nav_scr_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    LISA_UI_LOGD("Setting nav scr open, id: %d", scr->unique_id);
    
    struct setting_nav_scr_data *scr_data = lisa_ui_malloc(sizeof(struct setting_nav_scr_data));
    if (!scr_data) {
        LISA_UI_LOGE("Failed to allocate memory for setting_nav_scr_data");
        return -1;
    }

    memset(scr_data, 0, sizeof(struct setting_nav_scr_data));

    // Create view
    scr_data->view = lisa_ui_setting_view_create(lv_scr_act());
    if (!scr_data->view) {
        LISA_UI_LOGE("Failed to create setting view");
        lisa_ui_free(scr_data);
        return -1;
    }
    
    // Add setting items in 2x2 grid (col, row) with PNG icons from generated assets
    // Row 0: Common Settings (left), Network Settings (right)
    lisa_ui_setting_view_item_add(scr_data->view, _("common setting"), &icons_Icon_basics_png, 0, 0);
    lisa_ui_setting_view_item_add(scr_data->view, _("network setting"), &icons_Icon_wifi_png, 1, 0);
    // Row 1: Wakeup Interaction (left), Alarm Settings (right)
    lisa_ui_setting_view_item_add(scr_data->view, _("wakeup setting"), &icons_Icon_wake_up_png, 0, 1);
    lisa_ui_setting_view_item_add(scr_data->view, _("alarm setting"), &icons_Icon_alarm_png, 1, 1);
    
    // Set click callback for setting items
    lisa_ui_setting_view_set_click_cb(scr_data->view, setting_item_click_cb, NULL);
    
    // Set back button callback
    lisa_ui_setting_view_set_back_cb(scr_data->view, setting_back_btn_cb, NULL);
    
    *data = scr_data;
    return 0;
}

static int setting_nav_scr_show(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("Setting nav scr show, id: %d", scr->unique_id);

    struct setting_nav_scr_data *scr_data = (struct setting_nav_scr_data *)data;
    if (!scr_data || !scr_data->view) {
        LISA_UI_LOGE("Invalid setting nav scr data");
        return -1;
    }

    // View already created on current screen, just show it
    lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);

    return 0;
}

static int setting_nav_scr_pause(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("Setting nav scr pause, id: %d", scr->unique_id);
    
    struct setting_nav_scr_data *scr_data = (struct setting_nav_scr_data *)data;
    if (scr_data && scr_data->view) {
        lv_obj_add_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    }
    
    return 0;
}

static int setting_nav_scr_resume(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("Setting nav scr resume, id: %d", scr->unique_id);
    
    struct setting_nav_scr_data *scr_data = (struct setting_nav_scr_data *)data;
    if (scr_data && scr_data->view) {
        lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    }
    
    return 0;
}

static int setting_nav_scr_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("Setting nav scr close, id: %d", scr->unique_id);

    struct setting_nav_scr_data *scr_data = (struct setting_nav_scr_data *)data;
    if (scr_data) {
        if (scr_data->view) {
            lv_obj_del(scr_data->view);
        }
        lisa_ui_free(scr_data);
    }

    return 0;
}

const struct lisa_ui_nav_scr setting_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_SETTING,
    .open = setting_nav_scr_open,
    .show = setting_nav_scr_show,
    .pause = setting_nav_scr_pause,
    .resume = setting_nav_scr_resume,
    .close = setting_nav_scr_close,
};

