#ifndef __LISA_UI_LOG_UPLOAD_VIEW_H__
#define __LISA_UI_LOG_UPLOAD_VIEW_H__

#include "log_upload.h"
#include "lvgl.h"

lv_obj_t *lisa_ui_log_upload_view_create(lv_obj_t *parent);
void lisa_ui_log_upload_view_update(lv_obj_t *obj, const log_upload_state_t *state);

#endif
