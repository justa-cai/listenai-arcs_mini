/**
 * @file app_taskbar.c
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-02-26
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_taskbar.h"
#include "../app_common.h"
#include "lisaui_app_common.h"
#include "assets/assets_res.h"
#include "private_taskbar.h"

#include <stdio.h>
#include <time.h>

#define LISAUI_TASKBAR_TIME_UPDATE_INTERVAL 500
#define LISAUI_TASKBAR_PAD_LEFT             (80)
#define LISAUI_TASKBAR_PAD_RIGHT            (50)
#define LISAUI_TASKBAR_PAD_TOP              0
#define LISAUI_TASKBAR_PAD_BOTTOM           0

lv_obj_t *g_app_taskbar = NULL;

static const char *TAG = "app_taskbar";
#if CONFIG_LISAUI_DBUS_ENABLE
static void private_lisaui_app_enter_handler_cb(void *data)
{
    lisaui_app_t *app = (lisaui_app_t *)data;
    if (app == NULL) {
        LISAUI_LOGE(TAG, "app is NULL");
        return;
    }
    LISAUI_LOGI(TAG, "taskbar back app: %s", app->icon->title);
    // lisaui_app_show_info(app);
    // lisaui_popup_toast(app->icon->title);
    // lisaui_app_enter(app->info.id);
    // lisaui_app_exit(app->info.id);
    // lisaui_app_enter(UI_APP_ID_TASKBAR);
}
#endif

#if CONFIG_LISAUI_APP_TASKBAR_TOOLKIT_ENABLE
static void toolkit_panel_event_handler(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    LV_UNUSED(obj);

    if (event_code == LV_EVENT_GESTURE) {
        // lisaui_popup_toast("Gesture");
        LISAUI_LOGI(TAG, "Gesture");
        if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_TOP) {
            lv_indev_wait_release(lv_indev_get_act());
        }
    } else if (event_code == LV_EVENT_CLICKED) {
        // lisaui_popup_toast("Clicked");
        LISAUI_LOGI(TAG, "Clicked");
    } else if (event_code == LV_EVENT_PRESSED) {
        // lisaui_popup_toast("Pressed");
        LISAUI_LOGI(TAG, "Pressed");
    } else if (event_code == LV_EVENT_RELEASED) {
        // lisaui_popup_toast("Released");
        LISAUI_LOGI(TAG, "Released");
    }
}
#endif

lisaui_err_t app_taskbar_create(void *parent)
{
    g_app_taskbar = lv_obj_create(parent);
    lv_obj_set_size(g_app_taskbar, LV_PCT(100), LV_DPX(LISAUI_STATUS_BAR_HEIGHT));
    _lisaui_set_style_container(g_app_taskbar, lv_color_hex(0x000000), 0, lv_color_hex(0x000000), 0, 0);

#if CONFIG_LISAUI_APP_TASKBAR_TOOLKIT_ENABLE
    _lisaui_taskbar_create_back_btn(g_app_taskbar);
    if (lv_obj_get_width(g_app_taskbar) > 400) {
        _lisaui_taskbar_create_date_label(g_app_taskbar);
    }
    _lisaui_taskbar_create_time_label(g_app_taskbar);
    lv_obj_add_event_cb(g_app_taskbar, toolkit_panel_event_handler, LV_EVENT_ALL, NULL);
#else
    _lisaui_taskbar_create_operate_menu(g_app_taskbar);
    _lisaui_taskbar_create_battery(g_app_taskbar);
#endif

#if CONFIG_LISAUI_DBUS_ENABLE
    lisaui_dbus_t *m_taskbar_bus;
    if (lisaui_get_taskbar_dbus(&m_taskbar_bus) != LISAUI_ERR_OK) {
        LISAUI_LOGE(TAG, "Failed to get app manager bus");
        return LISAUI_ERR_FAIL;
    }

    lisaui_dbus_subscribe(m_taskbar_bus, LISAUI_DBUS_APP_ENTER, private_lisaui_app_enter_handler_cb);
#endif
    return LISAUI_ERR_OK;
}

lisaui_err_t lisaui_taskbar_hide(bool hide)
{
    if (g_app_taskbar == NULL) {
        return LISAUI_ERR_FAIL;
    }
    if (hide) {
        // lv_obj_set_hidden(g_app_taskbar, true);
        lv_obj_add_flag(g_app_taskbar, LV_OBJ_FLAG_HIDDEN);
    } else {
        // lv_obj_set_hidden(g_app_taskbar, false);
        lv_obj_clear_flag(g_app_taskbar, LV_OBJ_FLAG_HIDDEN);
    }
    return LISAUI_ERR_OK;
}

#if CONFIG_LISAUI_DBUS_ENABLE
static lisaui_dbus_t *_dbus_taskbar = NULL;

lisaui_err_t lisaui_get_taskbar_dbus(lisaui_dbus_t **dbus)
{
    if (dbus == NULL) {
        LISAUI_LOGE(TAG, "dbus is NULL");
        return LISAUI_ERR_INVALID_PARAM;
    }
    if (_dbus_taskbar == NULL) {
        // LISAUI_LOGE(TAG, "App manager bus is NULL(App manager not initialized)");
        // return LISAUI_ERR_FAIL;
        // _dbus_taskbar = lisaui_dbus_create();
        _dbus_taskbar = (lisaui_dbus_t *)lisaui_malloc(sizeof(struct _lisaui_dbus_t));
        if (!_dbus_taskbar) {
            LISAUI_LOGE(TAG, "Failed to create event bus");
            return LISAUI_ERR_FAIL;
        }
        if (lisaui_dbus_create(_dbus_taskbar) != LISAUI_ERR_OK) {
            LISAUI_LOGE(TAG, "Failed to create event bus");
            return LISAUI_ERR_FAIL;
        }
        LISAUI_LOGI(TAG, "Event bus created");
    }
    *dbus = _dbus_taskbar;
    return LISAUI_ERR_OK;
}
#endif

lisaui_err_t app_taskbar_destroy(void)
{
    LISAUI_LOGI(TAG, "[%d:%s] destroy", __LINE__, __func__);
    LVGL_OBJ_SAFE_DEL(g_app_taskbar);
    return LISAUI_ERR_OK;
}

lisaui_err_t app_taskbar_enter(void)
{
    LISAUI_LOGI(TAG, "[%d:%s] enter", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

lisaui_err_t app_taskbar_exit(void)
{
    LISAUI_LOGI(TAG, "[%d:%s] exit", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

void *app_taskbar_get_view(void)
{
    return g_app_taskbar;
}

static struct app_icon_t app_icon_res = {
#if CONFIG_LISAUI_FONT_LANGUAGE_ZH_CN_ENABLE
    .title = "任务栏",
#else
    .title = "taskbar",
#endif
    // .icon_width = LV_SIZE_CONTENT,
    // .icon_height = LV_SIZE_CONTENT,
    .icon = NULL, //&ui_img_icon_taskbar_png,
    .zoom = APP_ICON_ZOOM(0),
};

lisaui_app_t app_taskbar = {
    .create = app_taskbar_create,
    .destroy = app_taskbar_destroy,
    .enter = app_taskbar_enter,
    .exit = app_taskbar_exit,

    .get_app_view = app_taskbar_get_view,
    .info =
        {
            .name = "taskbar",
            .package_name = "com.listenai.lisaui.taskbar",
            .id = UI_APP_ID_TASKBAR,
            .type = LISAUI_APP_TYPE_SYSTEM,
        },
    .icon = &app_icon_res,
    .hidden_icon = true,
};

lisaui_err_t app_taskbar_init(void)
{
    lisaui_app_register(&app_taskbar);
    app_taskbar_create(lv_layer_sys());
    lisaui_app_common_set_enter_app_hook(lisaui_app_common_app_enter_common_handler);
    return LISAUI_ERR_OK;
}

LISAUI_REGISTER_APP(taskbar, &app_taskbar, app_taskbar_init);
