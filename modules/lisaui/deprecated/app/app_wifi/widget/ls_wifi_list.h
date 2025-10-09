/**
 * @file ls_wifi_list.h
 * @author TrekMax (QinYUN575@Foxmail.com)
 * @brief 
 * @version 0.1
 * @date 2025-04-20
 * 
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 * 
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LISAUI_WIDGET_LS_WIFI_LIST_H__
#define __LISAUI_WIDGET_LS_WIFI_LIST_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __ZEPHYR__
#include <zephyr/kernel.h>
#include "lvgl.h"
#elif CONFIG_LISAUI_ENV_ARCS_SDK
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

LV_FONT_DECLARE(lv_font_notosans_cs_medium_14);
LV_FONT_DECLARE(lv_font_chinese_18);

#include "../wifi_type.h"

#include "ls_wifi_item.h"

lv_obj_t *ls_wifi_list_create(lv_obj_t *parent);
lv_obj_t *ls_wifi_list_add_text(lv_obj_t *list, const char *txt);

lv_obj_t *ls_wifi_list_add_btn(lv_obj_t *list, wifi_metadata_t *wifi, lv_event_cb_t event_cb,
                             void *user_data) ;

const char *ls_wifi_list_get_btn_text(lv_obj_t *list, lv_obj_t *btn);

void lisa_ui_wifi_item_update(lisa_ui_wifi_item_t *wifi_item, wifi_metadata_t *wifi);
lv_obj_t *lisa_ui_wifi_item_create(lv_obj_t *parent);

lisa_ui_wifi_item_t * ls_wifi_list_item_get_by_ssid(lv_obj_t *list, const char *ssid);
lisa_ui_wifi_item_t *ls_wifi_list_add_item(lv_obj_t *list, const char *ssid, const char *status);
lisa_ui_wifi_item_t *ls_wifi_list_item_get_by_status(lv_obj_t *list, const char *sta);
lisa_ui_wifi_item_t *ls_wifi_list_item_get_by_index(lv_obj_t *list, int i);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*__LISAUI_WIDGET_LS_WIFI_LIST_H__*/
