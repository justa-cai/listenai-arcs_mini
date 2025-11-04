#ifndef __LISA_UI_ICON_BTN_H__
#define __LISA_UI_ICON_BTN_H__

#include "lvgl.h"

lv_obj_t *lisa_ui_icon_btn_create(lv_obj_t *parent, const void *icon_path, const char *label_text);
int lisa_ui_icon_btn_set_icon(lv_obj_t *obj, const void *icon_path);
int lisa_ui_icon_btn_set_label(lv_obj_t *obj, const char *label_text);

#endif /* __LISA_UI_ICON_BTN_H__ */