#ifndef __LS_WIFI_CONNECT_H__
#define __LS_WIFI_CONNECT_H__

#include "lvgl.h"


typedef void (*ls_wifi_connect_cancel_evt_cb_t)(lv_obj_t *);
typedef void (*ls_wifi_connect_connect_evt_cb_t)(lv_obj_t *, const char *, const char *);

typedef struct {
    lv_obj_t obj;
    lv_obj_t *tips_lbl;
    lv_obj_t *kb;
    lv_obj_t *pwd_ta;
    lv_obj_t *cancel_btn;
    lv_obj_t *apply_btn;
    ls_wifi_connect_cancel_evt_cb_t cancel_cb;
    ls_wifi_connect_connect_evt_cb_t connect_cb;
} ls_ui_wifi_connect_t;

lv_obj_t *ls_wifi_connect_create(lv_obj_t *parent);
void ls_wifi_connect_set_tips(ls_ui_wifi_connect_t *obj, const char *tips);
void ls_wifi_connect_set_cancel_cb(ls_ui_wifi_connect_t *obj, ls_wifi_connect_cancel_evt_cb_t cb);
void ls_wifi_connect_set_connect_cb(ls_ui_wifi_connect_t *obj, ls_wifi_connect_connect_evt_cb_t cb);

#endif
