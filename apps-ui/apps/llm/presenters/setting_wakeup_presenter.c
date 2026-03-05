/**
 * @file setting_wakeup_presenter.c
 * @brief Wakeup settings presenter - handles data logic only
 */

#include "lisa_ui_nav_scr.h"
#include "lisa_ui.h"
#include "lisa_ui_nav_scr_ids.h"
#include "setting_wakeup_view.h"
#include "model_voice.h"

#define TAG "setting_wakeup_presenter"

struct setting_wakeup_nav_scr_data {
    lv_obj_t *view;
};

/**
 * @brief 返回按钮回调 - 处理返回到设置主页面的导航
 * 这是presenter层的业务逻辑
 */
static void setting_wakeup_back_btn_cb(void *user_data)
{
    (void)user_data;
    LISA_UI_LOGD("Wakeup settings back button clicked, returning to settings");
    lisa_ui_nav_scr_nav_back();
}

static void on_wakeup_mode_changed(uint16_t mode, void *user_data)
{
    (void)user_data;
    
    LISA_UI_LOGD("Wakeup mode changed to: %d", mode);
    
    int ret = model_voice_wakeup_mode_set((model_voice_wakeup_mode_t)mode);
    if (ret == 0) {
        LISA_UI_LOGD("Successfully set wakeup mode: %s", 
                     model_voice_wakeup_mode_name_get((model_voice_wakeup_mode_t)mode));
    } else {
        LISA_UI_LOGE("Failed to set wakeup mode: %d", ret);
    }
}

static int setting_wakeup_nav_scr_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    LISA_UI_LOGD("Wakeup settings nav scr open");
    
    struct setting_wakeup_nav_scr_data *scr_data = lisa_ui_malloc(sizeof(struct setting_wakeup_nav_scr_data));
    if (!scr_data) {
        return -1;
    }
    memset(scr_data, 0, sizeof(struct setting_wakeup_nav_scr_data));

    // Create wakeup view (UI layout handled by view)
    scr_data->view = lisa_ui_setting_wakeup_view_create(lv_scr_act());
    if (!scr_data->view) {
        LISA_UI_LOGE("Failed to create wakeup view");
        lisa_ui_free(scr_data);
        return -1;
    }
    
    /* 注册返回按钮回调 */
    lisa_ui_setting_wakeup_view_set_back_cb(scr_data->view, setting_wakeup_back_btn_cb, NULL);
    lisa_ui_setting_wakeup_view_set_mode_changed_cb(scr_data->view, on_wakeup_mode_changed, NULL);

    /* 初始化UI选中状态为当前模式 */
    lisa_ui_setting_wakeup_view_set_mode(scr_data->view, (uint16_t)model_voice_wakeup_mode_get());
    
    *data = scr_data;
    return 0;
}

static int setting_wakeup_nav_scr_show(const struct lisa_ui_nav_scr *scr, void *data)
{
    struct setting_wakeup_nav_scr_data *scr_data = (struct setting_wakeup_nav_scr_data *)data;
    if (!scr_data || !scr_data->view) {
        return -1;
    }
    
    /* 同步当前模式，防止模型状态更新后UI未刷新 */
    lisa_ui_setting_wakeup_view_set_mode(scr_data->view, (uint16_t)model_voice_wakeup_mode_get());
    lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    return 0;
}

static int setting_wakeup_nav_scr_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    struct setting_wakeup_nav_scr_data *scr_data = (struct setting_wakeup_nav_scr_data *)data;
    if (scr_data) {
        if (scr_data->view) {
            lv_obj_del(scr_data->view);
        }
        lisa_ui_free(scr_data);
    }
    return 0;
}

const struct lisa_ui_nav_scr setting_wakeup_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_SETTING_WAKEUP,
    .open = setting_wakeup_nav_scr_open,
    .show = setting_wakeup_nav_scr_show,
    .close = setting_wakeup_nav_scr_close,
};
