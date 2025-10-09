/**
 * @file private_taskbar.c
 * @author TrekMax (QinYUN575@Foxmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-15
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
#include "lisaui_app_manager.h"
#include "lisaui_log.h"
#include "lv_img_utils.h"

#if CONFIG_LVGL_ENV_SIMULATOR
#define LISAUI_APP_TASKBAR_UI_RES_PERFIX_PATH(path) "app/llm/lisaui/app/app_taskbar/" path
#else
// #define LISAUI_APP_TASKBAR_UI_RES_PERFIX_PATH(path) "src/ui/lisaui/app/app_taskbar/" path
#define LISAUI_APP_TASKBAR_UI_RES_PERFIX_PATH(path) "source/view/lisaui/app/app_taskbar/" path
#endif

// UI_RES_IMG_NAME(back_btn, LISAUI_APP_TASKBAR_UI_RES_PERFIX_PATH("assets/png/icon_back.png"))
UI_RES_IMG_NAME(icon_setting, LISAUI_APP_TASKBAR_UI_RES_PERFIX_PATH("assets/png/ic_launch_setting.png"))
UI_RES_IMG_NAME(ic_nav_back, LISAUI_APP_TASKBAR_UI_RES_PERFIX_PATH("assets/png/ic_nav_back.png"))

static const char *TAG = "pri_app_taskbar";

lv_obj_t *m_label_date = NULL;
lv_obj_t *m_label_time = NULL;
lv_obj_t *g_taskbar_btn_back = NULL;
lv_obj_t *g_taskbar_btn_close = NULL;
lv_obj_t *g_taskbar_btn_home = NULL;

static bool charging = true;
static int battery_level = 0;
static int battery_percent = 0;
static lv_obj_t *battery_icon = NULL;
static lv_obj_t *battery_label = NULL;

static void taskbar_label_time_update_cb(lv_timer_t *timer)
{
    static bool dot_hide_flag = false;
    dot_hide_flag = !dot_hide_flag;
#if defined(__POSIX__)
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char time_str[10];
    snprintf(time_str, sizeof(time_str), "%02d%s%02d %s", tm->tm_hour, dot_hide_flag ? ":" : " ", tm->tm_min,
             tm->tm_hour < 12 ? "AM" : "PM");
    lv_label_set_text(m_label_time, time_str);
#endif
}

lv_obj_t *_lisaui_taskbar_create_time_label(lv_obj_t *parent)
{
    m_label_time = lv_label_create(parent);
    lv_label_set_text(m_label_time, "9:00 AM");
    lv_obj_align(m_label_time, LV_ALIGN_CENTER, LV_DPX(20), 0);

    lv_obj_set_style_text_color(m_label_time, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(m_label_time, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(m_label_time, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_timer_t *timer = lv_timer_create(taskbar_label_time_update_cb, 500, m_label_time);

    return m_label_time;
}

static void taskbar_label_date_update_cb(lv_timer_t *timer)
{
#if defined(__POSIX__)
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char date_str[40];
    snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    lv_label_set_text(m_label_date, date_str);
#endif
}

lv_obj_t *_lisaui_taskbar_create_date_label(lv_obj_t *parent)
{
    m_label_date = lv_label_create(parent);
    lv_obj_set_size(m_label_date, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    // lv_obj_align(m_label_date, LV_ALIGN_LEFT_MID, LV_DPX(68), 0);
    lv_obj_align_to(m_label_date, g_taskbar_btn_home, LV_ALIGN_OUT_RIGHT_MID, LV_DPX(6), 0);

    lv_label_set_text(m_label_date, "2025/1/22");
    lv_obj_set_style_text_color(m_label_date, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(m_label_date, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(m_label_date, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_timer_t *timer = lv_timer_create(taskbar_label_date_update_cb, 900, m_label_date);

    return m_label_date;
}

// typedef void (*lisaui_taskbar_event_handler_t)(lisaui_taskbar_event_t event, void *param);

static lisaui_taskbar_event_handler_t g_taskbar_event_handler = NULL;

lisaui_err_t lisaui_taskbar_register_event_handler(lisaui_taskbar_event_handler_t handler)
{
    if (handler == NULL) {
        return LISAUI_ERR_INVALID_PARAM;
    }
    g_taskbar_event_handler = handler;
    return LISAUI_ERR_OK;
}

lisaui_err_t lisaui_taskbar_unregister_event_handler(void)
{
    g_taskbar_event_handler = NULL;
    return LISAUI_ERR_OK;
}

#if CONFIG_LISAUI_APP_TASKBAR_TOOLKIT_ENABLE
static void back_btn_event_click_handler(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);
    if (event_code == LV_EVENT_CLICKED) {
        int app_id = lisaui_app_manager_get_current_appid();
        lisaui_app_t *app = NULL;
        if (lisaui_app_manager_get_app_by_id(app_id, &app) != LISAUI_ERR_OK) {
            LISAUI_LOGE(TAG, "Failed to get app by id: %d", app_id);
            return;
        }
        if (app->info.type == LISAUI_APP_TYPE_LAUNCHER) {
            lisaui_popup_toast("Can't exit or close launcher");
            return;
        }
        if (lisaui_app_get_lock_state()) {
            lisaui_popup_toast("App view is locked");
            return;
        }
        if (target == g_taskbar_btn_back) {
            lisaui_app_exit(app_id);
        } else if (target == g_taskbar_btn_close) {
            lisaui_app_close(app_id);
        } else if (target == g_taskbar_btn_home) {
            lisaui_app_enter(UI_APP_ID_LAUNCHER);
        }
    }
}
#else
static void back_btn_event_click_handler(lv_event_t *event)
{
    lv_event_code_t event_code = lv_event_get_code(event);
    lv_obj_t *target = lv_event_get_target(event);
    if (event_code == LV_EVENT_CLICKED) {
        int app_id = lisaui_app_manager_get_current_appid();
        lisaui_app_t *app = NULL;
        if (lisaui_app_manager_get_app_by_id(app_id, &app) != LISAUI_ERR_OK) {
            LISAUI_LOGE(TAG, "Failed to get app by id: %d", app_id);
            return;
        }
        LISAUI_LOGI(TAG, "app_id: %d, app_name: %s", app->info.id, app->info.name);

        if (app->info.type == LISAUI_APP_TYPE_LAUNCHER) {
            #if CONFIG_LVGL_ENV_SIMULATOR
                lisaui_app_enter(UI_APP_ID_SETTING);
            #else
                LISAUI_TASKBAR_EVENT_HANDLER(g_taskbar_event_handler, LISAUI_TASKBAR_EVENT_ENTER_SETTING, NULL);
            #endif
            return;
        }
        
        // 判断是否为 App 页面/技能页面
        if (app->info.id == UI_APP_ID_SETTING) {
            // lisaui_app_exit(app->info.id);
            #if CONFIG_LVGL_ENV_SIMULATOR
                if (lisaui_app_enter(UI_APP_ID_SETTING) == LISAUI_ERR_APP_ALREADY_IN) {
                    LISAUI_LOGI(TAG, "app already in");
                    // 判断退出还是关闭 具体的设置菜单
                    // lisaui_app_exit(app->info.id);
                    lisaui_app_enter(UI_APP_ID_STANDBY);
                }
            #else
                LISAUI_TASKBAR_EVENT_HANDLER(g_taskbar_event_handler, LISAUI_TASKBAR_EVENT_ENTER_STANDBY, NULL);
            #endif
        } else {
            // 技能页面
            LISAUI_TASKBAR_EVENT_HANDLER(g_taskbar_event_handler, LISAUI_TASKBAR_EVENT_ENTER_STANDBY, NULL);
        }
    }
}
#endif

lv_obj_t *_lisaui_taskbar_create_back_btn(lv_obj_t *parent)
{
    g_taskbar_btn_back = lv_btn_create(parent);
    lv_obj_set_size(g_taskbar_btn_back, LV_DPX(50), LV_DPX(LISAUI_STATUS_BAR_HEIGHT - 10));
    lv_obj_align(g_taskbar_btn_back, LV_ALIGN_LEFT_MID, LV_DPX(4), LV_DPX(0));
    lv_obj_add_event_cb(g_taskbar_btn_back, back_btn_event_click_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_radius(g_taskbar_btn_back, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *label = lv_label_create(g_taskbar_btn_back);
    lv_label_set_text(label, LV_SYMBOL_LEFT);
    // _lisaui_lv_obj_set_default_style(label);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    g_taskbar_btn_close = lv_btn_create(parent);
    lv_obj_set_size(g_taskbar_btn_close, LV_DPX(50), LV_DPX(LISAUI_STATUS_BAR_HEIGHT - 10));
    lv_obj_align_to(g_taskbar_btn_close, g_taskbar_btn_back, LV_ALIGN_OUT_RIGHT_MID, LV_DPX(2), 0);
    lv_obj_add_event_cb(g_taskbar_btn_close, back_btn_event_click_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_radius(g_taskbar_btn_close, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *label_close = lv_label_create(g_taskbar_btn_close);
    lv_label_set_text(label_close, LV_SYMBOL_CLOSE);
    lv_obj_align(label_close, LV_ALIGN_CENTER, 0, 0);

    g_taskbar_btn_home = lv_btn_create(parent);
    lv_obj_set_size(g_taskbar_btn_home, LV_DPX(50), LV_DPX(LISAUI_STATUS_BAR_HEIGHT - 10));
    lv_obj_align_to(g_taskbar_btn_home, g_taskbar_btn_close, LV_ALIGN_OUT_RIGHT_MID, LV_DPX(2), 0);
    lv_obj_add_event_cb(g_taskbar_btn_home, back_btn_event_click_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_radius(g_taskbar_btn_home, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *label_home = lv_label_create(g_taskbar_btn_home);
    lv_label_set_text(label_home, LV_SYMBOL_HOME);
    lv_obj_align(label_home, LV_ALIGN_CENTER, 0, 0);

    return g_taskbar_btn_back;
}

static const lv_img_dsc_t *_lisaui_taskbar_battery_charging_icon[5] = {
    &ui_img__status_charging_1_png, &ui_img__status_charging_2_png, &ui_img__status_charging_3_png,
    &ui_img__status_charging_4_png, &ui_img__status_charging_5_png,
};

static const lv_img_dsc_t *_lisaui_taskbar_battery_icon[5] = {
    &ui_img__status_battery_1_png, &ui_img__status_battery_2_png, &ui_img__status_battery_3_png,
    &ui_img__status_battery_4_png, &ui_img__status_battery_5_png,
};

lisaui_err_t lisaui_taskbar_set_battery_percent(int percent)
{
    battery_percent = percent;
    return LISAUI_ERR_OK;
}

lisaui_err_t lisaui_taskbar_set_battery_charging(bool is_charging)
{
    charging = is_charging;
    LISAUI_LOGI(TAG, "lisaui_taskbar_set_battery_charging status:%s", is_charging ? "true" : "false");
    return LISAUI_ERR_OK;
}

void _lisaui_taskbar_battery_update_cb(lv_timer_t *timer)
{
    // charging = !charging;
    static int count = 0;
    static uint8_t battery_charging_animation = 0;
    if (charging) {
        battery_charging_animation = (battery_charging_animation + 1) % 5;
        if (count % 10 == 0) {
            // LISAUI_LOGI(TAG, "battery charging status:%s", charging ? "true" : "false");
            battery_percent = (battery_percent + 1) % 100;
            if (battery_label) {
                lv_label_set_text_fmt(battery_label, "%d%%", battery_percent);
            }
            battery_level = battery_percent / 20;
        }
    }
    lv_img_set_src(battery_icon, charging ? _lisaui_taskbar_battery_charging_icon[battery_charging_animation]
                                          : _lisaui_taskbar_battery_icon[battery_level]);
    count++;
}

lv_obj_t *_lisaui_taskbar_create_battery(lv_obj_t *parent)
{
    if (lv_obj_get_width(parent) > 400) {
        battery_label = lv_label_create(parent);
        lv_label_set_text(battery_label, "100%");
        lv_obj_align(battery_label, LV_ALIGN_RIGHT_MID, -LV_DPX(20), 0);
        // _lisaui_lv_obj_set_default_style(battery_label);
        lv_obj_set_style_text_color(battery_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(battery_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    battery_icon = lv_img_create(parent);
    lv_img_set_src(battery_icon, _lisaui_taskbar_battery_icon[0]);
    if (lv_obj_get_width(parent) > 400) {
        lv_obj_align_to(battery_icon, battery_label, LV_ALIGN_OUT_LEFT_MID, 0, 0);
    } else {
        lv_obj_align(battery_icon, LV_ALIGN_RIGHT_MID, -LV_DPX(20), 0);
    }
    // lv_img_set_zoom(battery_icon, 255*1.2);
    lv_timer_t *timer = lv_timer_create(_lisaui_taskbar_battery_update_cb, 500, battery_icon);

    return battery_icon;
}
 
lv_obj_t *_lisaui_taskbar_create_operate_menu(lv_obj_t *parent)
{
    lv_obj_t *menu_panel = lv_obj_create(parent);
    lv_obj_set_size(menu_panel, LV_DPX(100), LV_PCT(100));
    lv_obj_set_style_bg_color(menu_panel, lv_color_hex(0x0000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(menu_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(menu_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(menu_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_padding_all(menu_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(menu_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(menu_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(menu_panel, LV_ALIGN_LEFT_MID, 0, LV_DPX(0));
    lv_obj_clear_flag(menu_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_shadow_width(menu_panel, 0, 0);
    lv_obj_set_style_border_width(menu_panel, 0, 0);
    lv_obj_set_style_outline_width(menu_panel, 0, 0);
    lv_obj_add_event_cb(menu_panel, back_btn_event_click_handler, LV_EVENT_CLICKED, NULL);

    // lv_obj_t *menu_btn = lv_btn_create(menu_panel);
    // lv_obj_set_size(menu_btn, LV_DPX(40), LV_DPX(LISAUI_STATUS_BAR_HEIGHT - 10));
    // lv_obj_align(menu_btn, LV_ALIGN_LEFT_MID, LV_DPX(4), 0);
    // lv_obj_add_event_cb(menu_btn, back_btn_event_click_handler, LV_EVENT_CLICKED, NULL);
    // lv_obj_set_style_radius(menu_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_shadow_width(menu_btn, 0, 0);
    // lv_obj_set_style_border_width(menu_btn, 0, 0);
    // lv_obj_set_style_outline_width(menu_btn, 0, 0);

    // lv_obj_t *label = lv_label_create(menu_btn);
    // lv_label_set_text(label, LV_SYMBOL_SETTINGS);
    // // _lisaui_lv_obj_set_default_style(label);
    // lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    LV_OBJ_ICON(icon_setting) = lv_img_create(menu_panel);
    lv_img_png_src_init(UI_RES_IMG_PNG(icon_setting));
    lv_img_png_src_init(UI_RES_IMG_PNG(ic_nav_back));

    lv_img_set_src(LV_OBJ_ICON(icon_setting), &LV_IMG_DSC(icon_setting));
    lv_obj_set_size(LV_OBJ_ICON(icon_setting), LV_DPX(40), LV_DPX(LISAUI_STATUS_BAR_HEIGHT - 10));
    lv_obj_align(LV_OBJ_ICON(icon_setting), LV_ALIGN_LEFT_MID, LV_DPX(4), 0);
    // lv_img_set_zoom(LV_OBJ_ICON(icon_setting), 256*2);

    lv_obj_add_flag(LV_OBJ_ICON(icon_setting), LV_OBJ_FLAG_CLICKABLE); /// Flags
    lv_obj_add_event_cb(LV_OBJ_ICON(icon_setting), back_btn_event_click_handler, LV_EVENT_CLICKED, NULL);
    
    lv_obj_set_style_radius(LV_OBJ_ICON(icon_setting), 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(LV_OBJ_ICON(icon_setting), lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(LV_OBJ_ICON(icon_setting), 0, 0);
    lv_obj_set_style_border_width(LV_OBJ_ICON(icon_setting), 0, 0);
    lv_obj_set_style_outline_width(LV_OBJ_ICON(icon_setting), 0, 0);
    lv_obj_set_style_bg_opa(LV_OBJ_ICON(icon_setting), 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *label2 = lv_label_create(menu_panel);
    lv_label_set_text(label2, "");
    lv_obj_set_width(label2, LV_DPX(140));
    lv_label_set_long_mode(label2, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(label2, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align_to(label2, LV_OBJ_ICON(icon_setting), LV_ALIGN_OUT_RIGHT_MID, LV_DPX(6), 0);
    lv_obj_set_style_text_color(label2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

    return menu_panel;
}


lisaui_err_t lisaui_app_common_app_enter_common_handler(lisaui_app_t *app)
{
    if (app == NULL) {
        LISAUI_LOGE(TAG, "app is NULL");
        return LISAUI_ERR_INVALID_PARAM;
    }
    LISAUI_LOGI(TAG, "[%d:%s] app: %s", __LINE__, __func__, app->icon->title);
    if (app->info.id == UI_APP_ID_SETTING) {
        // lv_img_set_src(LV_OBJ_ICON(icon_setting), &LV_IMG_DSC(ic_nav_back));
    }
    if (app->info.type == LISAUI_APP_TYPE_LAUNCHER) {
        // lv_img_set_src(LV_OBJ_ICON(icon_setting), &LV_IMG_DSC(icon_setting));
        // app_setting_del_item_page();
    }
    return LISAUI_ERR_OK;
}
