#ifndef __LISA_UI_OTA_VIEW_H__
#define __LISA_UI_OTA_VIEW_H__

#include "lvgl.h"
#include "ota_manager.h"

lv_obj_t *lisa_ui_ota_view_create(lv_obj_t *parent);
void lisa_ui_ota_view_update(lv_obj_t *obj, const ota_state_t *state);

#endif
