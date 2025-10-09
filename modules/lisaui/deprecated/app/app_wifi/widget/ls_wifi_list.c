/**
 * @file ls_wifi_list.c
 * @author TrekMax (QinYUN575@Foxmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-20
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ls_wifi_list.h"
#include <src/widgets/lv_label.h>
#include "lv_img_utils.h"
#include "incbin.h"

#include "ls_wifi_item.h"

static lv_img_dsc_t img_wifi_status;

#if CONFIG_LVGL_ENV_SIMULATOR
#define LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH(path) "app/llm/lisaui/app/app_wifi/" path
#else
#define LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH(path) "source/view/lisaui/app/app_wifi/" path
// #define LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH(path) "src/ui/lisaui/app/app_wifi/" path
// app/llm/lisaui/widgets/assets/gif/ani_talk.gif
// app/llm/lisaui/app/app_wifi/assets/png/ic_status_wifi0.png
#endif

// UI_RES_IMG_NAME(wifi_status, LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH("assets/png/ic_status_wifi4.png"))
// UI_RES_IMG_NAME(ani_talk, LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH("assets/gif/ani_talk.gif"))

#define MV_CLASS &ls_wifi_list

const lv_obj_class_t ls_wifi_list_class = {
    .base_class = &lv_obj_class, .width_def = (LV_DPI_DEF * 3) / 2, .height_def = LV_DPI_DEF * 2};

const lv_obj_class_t ls_wifi_list_btn_class = {
    .base_class = &lv_obj_class,
};

const lv_obj_class_t ls_wifi_list_text_class = {
    .base_class = &lv_label_class,
};

lv_obj_t *ls_wifi_list_create(lv_obj_t *parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t *list_panel = lv_obj_create(parent);
    lv_obj_set_size(list_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(list_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_width(list_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(list_panel, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(list_panel, 0, 0);
    // lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_bg_color(list_panel, lv_color_hex(0x0F0000), LV_PART_MAIN | LV_STATE_DEFAULT);

    return list_panel;
}

lisa_ui_wifi_item_t *ls_wifi_list_add_item(lv_obj_t *list, const char *ssid, const char *status)
{
    lv_obj_t *item = ls_ui_wifi_item_create(list);
    if (item == NULL) {
        return NULL;
    }

    lisa_ui_wifi_item_t *wifi_item = (lisa_ui_wifi_item_t *)item;
    ls_ui_wifi_item_set_ssid(wifi_item, ssid);
    ls_ui_wifi_item_set_status(wifi_item, status);

    return wifi_item;
}

lisa_ui_wifi_item_t *ls_wifi_list_item_get_by_ssid(lv_obj_t *list, const char *ssid)
{
    int i;
    for (i = 0; i < lv_obj_get_child_cnt(list); i++) {
        lisa_ui_wifi_item_t *wifi_item = (lisa_ui_wifi_item_t *)lv_obj_get_child(list, i);
        if (wifi_item == NULL) {
            return NULL;
        }

        const char *text = lv_label_get_text(wifi_item->ssid_lbl);

        if (text != NULL && strcmp(text, ssid) == 0) {
            return wifi_item;
        }
    }

    return NULL;
}

lisa_ui_wifi_item_t *ls_wifi_list_item_get_by_status(lv_obj_t *list, const char *sta)
{
    int i;
    for (i = 0; i < lv_obj_get_child_cnt(list); i++) {
        lisa_ui_wifi_item_t *wifi_item = (lisa_ui_wifi_item_t *)lv_obj_get_child(list, i);
        if (wifi_item == NULL) {
            return NULL;
        }

        const char *text = lv_label_get_text(wifi_item->sta_lbl);

        if (text != NULL && strcmp(text, sta) == 0) {
            return wifi_item;
        }
    }

    return NULL;
}

lisa_ui_wifi_item_t *ls_wifi_list_item_get_by_index(lv_obj_t *list, int i)
{
    return (lisa_ui_wifi_item_t *)lv_obj_get_child(list, i);
}
