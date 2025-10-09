/**
 * @file app_standby.c
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-01-22
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_standby.h"
#include "lisaui_app_common.h"
#include "assets/assets_res.h"

#include "app_alarm/app_alarm.h"

static const char *TAG = "app_standby";
static lv_obj_t *g_app_standby = NULL;
static lv_obj_t *g_app_panel = NULL;
static lv_obj_t *g_icon_emoji = NULL;
static lv_obj_t *g_label_wakeup_tip = NULL;

lv_timer_t *timer_listening_timeout = NULL;

static void timer_listening_timeout_handler(lv_timer_t *timer)
{
    LISAUI_LOGI(TAG, "[%d:%s] timer_listening_timeout_handler", __LINE__, __func__);
    // 超时恢复待机表情
    lisaui_app_standby_set_emoji(LISAUI_APP_STANDBY_EMOJI_TYPE_STANDBY);
    // lv_timer_del(timer);
    // timer_listening_timeout = NULL;
}

lisaui_err_t lisaui_app_standby_set_emoji(lisaui_app_standby_emoji_type_e type)
{
    if (g_icon_emoji == NULL) {
        LISAUI_LOGW(TAG, "[%d:%s] g_icon_emoji is NULL", __LINE__, __func__);
        return LISAUI_ERR_INVALID_PARAM;
    }
    LVGL_UI_LOCK();
    switch (type) {
    case LISAUI_APP_STANDBY_EMOJI_TYPE_STANDBY:
        lv_gif_set_src(g_icon_emoji, &anim_standby);
        break;
    case LISAUI_APP_STANDBY_EMOJI_TYPE_LISTENING:
        lv_gif_set_src(g_icon_emoji, &anim_wakeup);
        break;
    case LISAUI_APP_STANDBY_EMOJI_TYPE_RECOGNITION:
        break;
    case LISAUI_APP_STANDBY_EMOJI_TYPE_RESPONSE:
        lv_gif_set_src(g_icon_emoji, &anim_speaking2_15fps);
        break;
    default:
        break;
    }
    LVGL_UI_UNLOCK();
    return LISAUI_ERR_OK;
}

lisaui_err_t app_standby_create(void *parent)
{
    if (g_app_standby != NULL) {
        return LISAUI_ERR_OK;
    }
    g_app_standby = lv_obj_create(parent);
    _lisaui_set_style_container(g_app_standby, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);

    g_app_panel = lv_obj_create(g_app_standby);
    _lisaui_set_style_container(g_app_panel, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);

    LISAUI_COMMON_SET_APP_VIEW_PANEL_SIZE(g_app_standby, g_app_panel, LISAUI_ERR_INVALID_PARAM);

    g_icon_emoji = lv_gif_create(g_app_panel);
    lv_obj_align(g_icon_emoji, LV_ALIGN_CENTER, 0, -(LV_DPX(30)));
    lisaui_app_standby_set_emoji(LISAUI_APP_STANDBY_EMOJI_TYPE_STANDBY);

    g_label_wakeup_tip = lv_label_create(g_app_standby);
    lv_label_set_text(g_label_wakeup_tip, "请使用“小聆小聆”唤醒我");
    lv_obj_align(g_label_wakeup_tip, LV_ALIGN_BOTTOM_MID, 0, -(LV_DPX(20)));
    lv_obj_set_style_text_font(g_label_wakeup_tip, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_label_wakeup_tip, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_color(g_label_wakeup_tip, lv_color_black(), 0);

    LISAUI_LOGI(TAG, "[%d:%s] create", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

lisaui_err_t app_standby_destroy(void)
{
    LVGL_OBJ_SAFE_DEL(g_icon_emoji);
    LVGL_OBJ_SAFE_DEL(g_label_wakeup_tip);

    LVGL_OBJ_SAFE_DEL(g_app_standby);
    LISAUI_LOGI(TAG, "[%d:%s] destroy", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

lisaui_err_t app_standby_enter(void)
{
    LISAUI_LOGI(TAG, "[%d:%s] enter", __LINE__, __func__);
    extern lisaui_err_t lisaui_app_alarm_close_skill_alarm_view(void);
    lisaui_app_alarm_close_skill_alarm_view();
    return LISAUI_ERR_OK;
}

lisaui_err_t app_standby_exit(void)
{
    LISAUI_LOGI(TAG, "[%d:%s] exit", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

void *app_standby_get_view(void)
{
    return g_app_standby;
}

static struct app_icon_t app_icon_res_standby = {
    .title = "待机",
    // .icon_width = LV_SIZE_CONTENT,
    // .icon_height = LV_SIZE_CONTENT,
    .icon = NULL,
    .zoom = APP_ICON_ZOOM(0),
};

lisaui_app_t app_standby = {
    .create = app_standby_create,
    .destroy = app_standby_destroy,
    .enter = app_standby_enter,
    .exit = app_standby_exit,

    .get_app_view = app_standby_get_view,
    .info =
        {
            .name = "Standby",
            .package_name = "com.listenai.lisaui.standby",
            .id = UI_APP_ID_STANDBY,
            .type = LISAUI_APP_TYPE_LAUNCHER,
        },
    .icon = &app_icon_res_standby,
};

lisaui_err_t app_standby_init(void)
{
    lisaui_app_register(&app_standby);
    // lisaui_app_enter(UI_APP_ID_STANDBY);
    return LISAUI_ERR_OK;
}

LISAUI_REGISTER_APP(standby, &app_standby, app_standby_init);
