/**
 * @file setting_common_presenter.c
 * @brief Common settings presenter - handles data logic only
 */

#include "lisa_ui_nav_scr.h"
#include "lisa_ui.h"
#include "lisa_ui_llm_base.h"
#include "lisa_ui_nav_scr_ids.h"
#include "setting_common_view.h"
#include "model_common.h"

#define TAG "setting_common_presenter"

struct setting_common_nav_scr_data {
    lv_obj_t *view;
};

/**
 * @brief Back button callback - handles navigation back to settings
 * This is business logic in presenter layer
 */
static void setting_common_back_btn_cb(void *user_data)
{
    (void)user_data;
    LISA_UI_LOGD("Common settings back button clicked, returning to settings");
    lisa_ui_nav_scr_nav_back();
}

/**
 * @brief Volume changed callback - handles volume setting
 * This is business logic in presenter layer
 */
static void setting_common_volume_changed_cb(uint8_t volume, void *user_data)
{
    (void)user_data;
    LISA_UI_LOGD("Volume changed to: %d%%", volume);
    model_common_volume_set(volume);
}

/**
 * @brief Brightness changed callback - handles brightness setting
 * This is business logic in presenter layer
 */
static void setting_common_brightness_changed_cb(uint8_t brightness, void *user_data)
{
    (void)user_data;
    LISA_UI_LOGD("Brightness changed to: %d%%", brightness);
    model_common_brightness_set(brightness);
}

static int setting_common_nav_scr_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    (void)scr;
    LISA_UI_LOGD("Common settings nav scr open");
    
    struct setting_common_nav_scr_data *scr_data = lisa_ui_malloc(sizeof(struct setting_common_nav_scr_data));
    if (!scr_data) {
        return -1;
    }
    memset(scr_data, 0, sizeof(struct setting_common_nav_scr_data));

    // Create view
    scr_data->view = lisa_ui_setting_common_view_create(lv_scr_act());
    if (!scr_data->view) {
        lisa_ui_free(scr_data);
        return -1;
    }
    
    // Sync with system services before showing UI
    model_common_sync_from_system();
    // Get current values from model and update view
    uint8_t current_volume = model_common_volume_get();
    uint8_t current_brightness = model_common_brightness_get();
    
    lisa_ui_setting_common_view_set_volume(scr_data->view, current_volume);
    lisa_ui_setting_common_view_set_brightness(scr_data->view, current_brightness);
    
    // Set callbacks
    lisa_ui_setting_common_view_set_back_cb(scr_data->view, setting_common_back_btn_cb, NULL);
    lisa_ui_setting_common_view_set_volume_changed_cb(scr_data->view, setting_common_volume_changed_cb, NULL);
    lisa_ui_setting_common_view_set_brightness_changed_cb(scr_data->view, setting_common_brightness_changed_cb, NULL);
    
    LISA_UI_LOGD("Loaded settings - Volume: %d%%, Brightness: %d%%", 
                 current_volume, current_brightness);
    
    *data = scr_data;
    return 0;
}

static int setting_common_nav_scr_show(const struct lisa_ui_nav_scr *scr, void *data)
{
    struct setting_common_nav_scr_data *scr_data = (struct setting_common_nav_scr_data *)data;
    if (!scr_data || !scr_data->view) {
        return -1;
    }
    
    lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    return 0;
}

static int setting_common_nav_scr_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    struct setting_common_nav_scr_data *scr_data = (struct setting_common_nav_scr_data *)data;
    if (scr_data) {
        if (scr_data->view) {
            lv_obj_del(scr_data->view);
        }
        lisa_ui_free(scr_data);
    }
    return 0;
}

const struct lisa_ui_nav_scr setting_common_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_SETTING_COMMON,
    .open = setting_common_nav_scr_open,
    .show = setting_common_nav_scr_show,
    .close = setting_common_nav_scr_close,
};
