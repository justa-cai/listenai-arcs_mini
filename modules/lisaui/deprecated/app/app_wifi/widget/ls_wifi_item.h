#ifndef __LS_WIFI_LIST_ITEM_H__
#define __LS_WIFI_LIST_ITEM_H__

#include "lvgl.h"

typedef struct {
    lv_obj_t obj;
    lv_obj_t *ssid_lbl;
    lv_obj_t *img;
    lv_obj_t *sta_lbl;
} lisa_ui_wifi_item_t;

lv_obj_t *ls_ui_wifi_item_create(lv_obj_t *parent);
void ls_ui_wifi_item_set_status(lisa_ui_wifi_item_t *wifi_item, const char *status);
void ls_ui_wifi_item_set_ssid(lisa_ui_wifi_item_t *wifi_item, const char *ssid);

#endif
