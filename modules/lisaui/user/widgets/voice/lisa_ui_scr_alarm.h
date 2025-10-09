#ifndef __LISA_UI_ALARM_H__
#define __LISA_UI_ALARM_H__

#include "lvgl.h"

lv_obj_t *lisa_ui_alarm_create(lv_obj_t *parent);
void lisa_ui_alarm_item_clear(lv_obj_t *obj);
int lisa_ui_alarm_item_add(lv_obj_t *obj, const char *label, const char *sub_label);

#endif /* __LISA_UI_ALARM_H__ */
