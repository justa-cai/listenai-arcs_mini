/**
 * @file app_setting_view_wakeup_config.c
 * @author TrekMax (QinYUN575@Foxmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-15
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_setting_view_wakeup_config.h"

#include "app_setting.h"
#include "assets/assets_res.h"
#include "lisaui_app_common.h"
#include "lv_img_utils.h"
#include "common_widgets.h"

static const char *TAG = "app_setting_view_wakeup_config";

typedef struct _lisaui_app_setting_wakeup_options_t {
    const char *title;
    const char *tips;
    const int id;
    int mode;
    lisaui_app_setting_set_wakeup_mode_cb_t event_cb;
    void *user_data;
} lisaui_app_setting_wakeup_options_t;

static lisaui_app_setting_set_wakeup_mode_cb_t g_wakeup_mode_handler = NULL;

lisaui_err_t lisaui_app_setting_register_set_wakeup_mode_handler(lisaui_app_setting_set_wakeup_mode_cb_t handler)
{
    if (handler == NULL) {
        LISAUI_LOGE(TAG, "wakeup mode handler is NULL");
        return LISAUI_ERR_INVALID_PARAM;
    }
    g_wakeup_mode_handler = handler;
    return LISAUI_ERR_OK;
}
void lisaui_app_radio_select_handler(lv_event_t *event)
{
    lv_event_code_t event_code = lv_event_get_code(event);
    if (LV_EVENT_CLICKED == event_code) {
        uint16_t radio_id = *(uint16_t *)lv_event_get_user_data(event);
        LISAUI_LOGI(TAG, "radio_id = %d", radio_id);
        if (g_wakeup_mode_handler) {
            lisaui_app_setting_wakeup_options_t *p_wakeup_option =
                (lisaui_app_setting_wakeup_options_t *)lv_event_get_user_data(event);
            p_wakeup_option->mode = radio_id;
            g_wakeup_mode_handler(LISAUI_SETTING_WAKEUP_OP_SET, (void *)&p_wakeup_option->mode);
        }
    }
}

static lisaui_app_setting_wakeup_options_t g_wakeup_options[] = {
    {"按键唤醒", "通过开发板 VOICE 按键进行唤醒", 0, LISAUI_APP_SETTING_WAKEUP_MODE_KEY, NULL, NULL},
    {"语音唤醒（单轮对话）", "通过唤醒词“小美小美”唤醒，一次唤醒一轮对话", 1,
     LISAUI_APP_SETTING_WAKEUP_MODE_VOICE_SINGLE, NULL, NULL},
    {"语音唤醒（多轮对话）", "通过唤醒词“小美小美”唤醒，支持多轮对话", 2, LISAUI_APP_SETTING_WAKEUP_MODE_VOICE_MULTI,
     NULL, NULL},
};

lv_obj_t *private_lisaui_app_view_create_wakeup_config(lv_obj_t *parent)
{
    lv_obj_t *setting_home = lv_obj_create(parent);
    lv_obj_set_size(setting_home, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(setting_home, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(setting_home, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(setting_home, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(setting_home, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(setting_home, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(setting_home, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_add_flag(setting_home, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t *page = ls_lv_radio_group_create(parent, NULL, NULL);
    // ls_lv_radio_group_add(page, "按键唤醒", "通过开发板 K3 按键进行唤醒", radio_event_handler, NULL);
    // ls_lv_radio_group_add(page, "语音唤醒（单轮对话）", "通过唤醒词“小美小美”唤醒，一次唤醒一轮对话",
    //                       radio_event_handler, NULL);
    // ls_lv_radio_group_add(page, "语音唤醒（多轮对话）", "通过唤醒词“小美小美”唤醒，支持多轮对话",
    // radio_event_handler,
    //                       NULL);
    uint8_t active = 0;
    for (int i = 0; i < sizeof(g_wakeup_options) / sizeof(g_wakeup_options[0]); i++) {
        ls_lv_radio_group_add(page, g_wakeup_options[i].title, g_wakeup_options[i].tips,
                              lisaui_app_radio_select_handler, (void *)&g_wakeup_options[i]);
        // if (g_wakeup_options[i].id == active) {
        //     active = i;
        // }
    }
    if (g_wakeup_mode_handler) {
        lisaui_app_setting_wakeup_options_t wakeup_option = {0};
        g_wakeup_mode_handler(LISAUI_SETTING_WAKEUP_OP_GET, (void *)&wakeup_option.id);
        lv_lv_radio_group_set_active_id(page, wakeup_option.id);
    }
    return setting_home;
}
