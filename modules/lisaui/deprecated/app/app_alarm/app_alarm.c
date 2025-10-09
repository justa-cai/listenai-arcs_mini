/**
 * @file app_alarm.c
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-01-22
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_alarm.h"
#include "lisaui_app_common.h"
#include "common_widgets.h"

#define LISAUI_ALARM_MAX_NUMBER  (32)
static const char *TAG = "app_alarm";
static lv_obj_t *g_app_alarm = NULL;
static lv_obj_t *g_app_panel = NULL;

static lv_obj_t *g_obj_alarm_list = NULL;
static lv_obj_t *g_delete_alarm_menu = NULL;

static lv_obj_t *g_apply_btn = NULL;
static lv_obj_t *g_cancel_btn = NULL;

static lisaui_app_alarm_handler_t g_app_alarm_op_handler = NULL;
lisaui_err_t lisaui_app_alarm_register_handler(lisaui_app_alarm_handler_t handler)
{
    if (handler == NULL) {
        return LISAUI_ERR_FAIL;
    }
    g_app_alarm_op_handler = handler;
    return LISAUI_ERR_OK;
}

static void app_alarm_delete_menu_destroy(lv_obj_t *menu)
{
    if (menu == NULL) {
        return;
    }
    lv_obj_del(menu);
    menu = NULL;
}

static void msgbox_event_handler(lv_event_t *event)
{
    lv_obj_t *obj = lv_event_get_target(event);
    lv_event_code_t event_code = lv_event_get_code(event);
    LISAUI_LOGI(TAG, "button clicked");

    if (event_code == LV_EVENT_CLICKED) {
        if (g_apply_btn && g_apply_btn == obj) {
            LISAUI_LOGI(TAG, "button apply clicked");
            lisaui_alarm_clock_item_t *item = (lisaui_alarm_clock_item_t *)lv_event_get_user_data(event);
            if (item == NULL) {
                LISAUI_LOGE(TAG, "item is NULL");
                return;
            }
            g_app_alarm_op_handler(LISAUI_ALARM_ITEM_CLOCK, LISAUI_ALARM_OP_DELETE, (void *)&item->timestamp);
            if (item) {
                lisaui_free(item);
                item = NULL;
            }
            app_alarm_delete_menu_destroy(g_delete_alarm_menu);
            // FIXME: 删除 alarm item
        }
        if (g_cancel_btn && g_cancel_btn == obj) {
            LISAUI_LOGI(TAG, "button cancel clicked");
            app_alarm_delete_menu_destroy(g_delete_alarm_menu);
        }
    }
}

static lv_obj_t *app_alarm_delete_menu_create(lv_obj_t *parent, void *data)
{
    lv_obj_t *menu = lv_obj_create(parent);
    lv_obj_set_size(menu, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(menu, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(menu, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(menu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(menu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(menu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *tips_label = lv_label_create(menu);
    lv_label_set_text(tips_label, "是否删除该闹钟");
    lv_obj_align(tips_label, LV_ALIGN_CENTER, 0, -LV_DPX(60));
    lv_obj_set_style_text_color(tips_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(tips_label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(tips_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_apply_btn = lv_btn_create(menu);
    lv_obj_align(g_apply_btn, LV_ALIGN_BOTTOM_MID, LV_DPX(80), -LV_DPX(30));
    lv_obj_add_event_cb(g_apply_btn, msgbox_event_handler, LV_EVENT_CLICKED, data);
    lv_obj_set_size(g_apply_btn, LV_DPX(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(g_apply_btn, lv_color_hex(0x24242d), 0);
    lv_obj_set_style_radius(g_apply_btn, 8, 0);
    lv_obj_set_style_shadow_width(g_apply_btn, 0, 0);
    lv_obj_set_style_border_width(g_apply_btn, 0, 0);
    lv_obj_set_style_outline_width(g_apply_btn, 0, 0);

    lv_obj_t *label;
    label = lv_label_create(g_apply_btn);
    lv_label_set_text(label, "删除");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(label);

    g_cancel_btn = lv_btn_create(menu);
    lv_obj_align(g_cancel_btn, LV_ALIGN_BOTTOM_MID, -LV_DPX(80), -LV_DPX(30));
    lv_obj_set_size(g_cancel_btn, LV_DPX(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(g_cancel_btn, lv_color_hex(0x24242d), 0);
    lv_obj_set_style_radius(g_cancel_btn, 8, 0);
    lv_obj_set_style_shadow_width(g_cancel_btn, 0, 0);
    lv_obj_set_style_border_width(g_cancel_btn, 0, 0);
    lv_obj_set_style_outline_width(g_cancel_btn, 0, 0);
    lv_obj_add_event_cb(g_cancel_btn, msgbox_event_handler, LV_EVENT_CLICKED, data);

    label = lv_label_create(g_cancel_btn);
    lv_label_set_text(label, "取消");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(label);

    return menu;
}

static void event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    // setting_item_t *item = (setting_item_t *)lv_event_get_user_data(e);
    if (code == LV_EVENT_CLICKED) {
        LISAUI_LOGI(TAG, "Clicked");
        g_delete_alarm_menu = app_alarm_delete_menu_create(lv_layer_sys(), lv_event_get_user_data(e));
    }
}

void app_alarm_create_alarm_list(lv_obj_t *parent)
{
    g_obj_alarm_list = ls_lv_list_create(parent);
    lv_obj_set_size(g_obj_alarm_list, LV_PCT(100), LV_PCT(100));
    // // ls_lv_list_add_text(g_obj_alarm_list, "闹钟列表");
    // ls_lv_list_add_btn(g_obj_alarm_list, "07:00", "Alarm1", event_handler, NULL);
    // ls_lv_list_add_btn(g_obj_alarm_list, "08:00", "Alarm1", event_handler, NULL);
    // ls_lv_list_add_btn(g_obj_alarm_list, "09:00", "Alarm1", event_handler, NULL);
    // ls_lv_list_add_btn(g_obj_alarm_list, "09:00", "Alarm1", event_handler, NULL);
    // // ls_lv_list_add_text(g_obj_alarm_list, "闹钟列表");
    // ls_lv_list_add_btn(g_obj_alarm_list, "09:00", "Alarm1", event_handler, NULL);
    // ls_lv_list_add_btn(g_obj_alarm_list, "09:00", "Alarm1", event_handler, NULL);
    // ls_lv_list_add_btn(g_obj_alarm_list, "09:00", "Alarm1", event_handler, NULL);

    if (g_app_alarm_op_handler) {
        lisaui_alarm_clock_list_t*  alarm_clock_list = 
            lisaui_malloc(sizeof(lisaui_alarm_clock_list_t) + sizeof(lisaui_alarm_clock_item_t)*LISAUI_ALARM_MAX_NUMBER);
        if(alarm_clock_list == NULL){
            LISAUI_LOGE(TAG,"[%s %d]no memory!",__FUNCTION__,__LINE__);
            return;
        }
        alarm_clock_list->count = LISAUI_ALARM_MAX_NUMBER;
        g_app_alarm_op_handler(LISAUI_ALARM_ITEM_CLOCK_LIST, LISAUI_ALARM_OP_GET_LIST, alarm_clock_list);
        for (int i = 0; i < alarm_clock_list->count; i++) {
            lisaui_alarm_clock_item_t *item = (lisaui_alarm_clock_item_t *)lisaui_malloc(sizeof(lisaui_alarm_clock_item_t));
            if (item == NULL) {
                LISAUI_LOGE(TAG, "malloc failed");
                return;
            }
            LISAUI_LOGI(TAG, "Add alarm clock item %d: %s", i, alarm_clock_list->list[i].time_text);
            strncpy(item->time_text, alarm_clock_list->list[i].time_text, LISAUI_ALARM_TIME_TEXT_MAX_LEN);
            strncpy(item->date_text, alarm_clock_list->list[i].date_text, LISAUI_ALARM_DATE_TEXT_MAX_LEN);
            item->timestamp = alarm_clock_list->list[i].timestamp;
            ls_lv_list_add_btn(g_obj_alarm_list, alarm_clock_list->list[i].time_text,
                                alarm_clock_list->list[i].date_text, event_handler, (void *)item);
        }
        lisaui_free(alarm_clock_list);
    }
}

lisaui_err_t lisaui_app_alarm_op_alarm_clear(void)
{
    LVGL_UI_LOCK();
    // app_alarm_delete_menu_destroy(g_delete_alarm_menu);
    if (g_obj_alarm_list != NULL) {
        // lv_obj_del(g_delete_alarm_menu);
        // g_delete_alarm_menu = NULL;
        int cnt = lv_obj_get_child_cnt(g_obj_alarm_list);
        for (int i = 0; i < cnt; i++) {
            lv_obj_t *child = lv_obj_get_child(g_obj_alarm_list, 0);
            if (child) {
                lisaui_alarm_clock_item_t *item = (lisaui_alarm_clock_item_t *)lv_obj_get_user_data(child);
                lisaui_free(item);
            }
        }
        lv_obj_clean(g_obj_alarm_list);
    }
    LVGL_UI_UNLOCK();
    return LISAUI_ERR_OK;
}

lisaui_err_t app_alarm_create(void *parent)
{
    if (g_app_alarm != NULL) {
        return LISAUI_ERR_OK;
    }
    g_app_alarm = lv_obj_create(parent);
    _lisaui_set_style_container(g_app_alarm, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);

    g_app_panel = lv_obj_create(g_app_alarm);
    _lisaui_set_style_container(g_app_panel, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);

    LISAUI_COMMON_SET_APP_VIEW_PANEL_SIZE(g_app_alarm, g_app_panel, LISAUI_ERR_INVALID_PARAM);

    app_alarm_create_alarm_list(g_app_panel);

    LISAUI_LOGI(TAG, "[%d:%s] create\n", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

lisaui_err_t app_alarm_destroy(void)
{
    LVGL_OBJ_SAFE_DEL(g_app_alarm);

    LISAUI_LOGI(TAG, "[%d:%s] destroy\n", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

lisaui_err_t app_alarm_enter(void)
{
    LISAUI_LOGI(TAG, "[%d:%s] enter\n", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

lisaui_err_t app_alarm_exit(void)
{
    LISAUI_LOGI(TAG, "[%d:%s] exit\n", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

void *app_alarm_get_view(void)
{
    return g_app_alarm;
}

static struct app_icon_t app_icon_res_alarm = {
    .title = "闹钟",
    // .icon_width = LV_SIZE_CONTENT,
    // .icon_height = LV_SIZE_CONTENT,
    .icon = NULL, //&icon_img_app_store_png,
    .zoom = APP_ICON_ZOOM(0),
};

lisaui_app_t app_alarm = {
    .create = app_alarm_create,
    .destroy = app_alarm_destroy,
    .enter = app_alarm_enter,
    .exit = app_alarm_exit,

    .get_app_view = app_alarm_get_view,
    .info =
        {
            .name = "Alarm",
            .package_name = "com.listenai.lisaui.alarm",
            .id = UI_APP_ID_ALARM,
        },
    .icon = &app_icon_res_alarm,
};

lisaui_err_t app_alarm_init(void)
{
    lisaui_app_register(&app_alarm);
    return LISAUI_ERR_OK;
}

LISAUI_REGISTER_APP(alarm, &app_alarm, app_alarm_init);