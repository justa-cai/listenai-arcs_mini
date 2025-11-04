#ifndef __LISA_UI_SETTING_MAIN_H__
#define __LISA_UI_SETTING_MAIN_H__

#include "lvgl.h"

typedef void (*lisa_ui_setting_item_click_cb_t)(const char *name, void *user_data);

void lisa_ui_setting_item_click_cb_set(lv_obj_t *obj, lisa_ui_setting_item_click_cb_t cb, void *user_data);

lv_obj_t *lisa_ui_setting_create(lv_obj_t *parent);
int lisa_ui_setting_item_add(lv_obj_t *obj, const char *label, const void *icon);
int lisa_ui_setting_item_remove(lv_obj_t *obj, int index);

#endif /* __LISA_UI_SETTING_MAIN_H__ */