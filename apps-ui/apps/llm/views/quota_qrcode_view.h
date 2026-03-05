#ifndef LISA_UI_QUOTA_QRCODE_VIEW_H
#define LISA_UI_QUOTA_QRCODE_VIEW_H

#include "lvgl.h"

lv_obj_t *lisa_ui_quota_qrcode_view_create(lv_obj_t *parent);
void lisa_ui_quota_qrcode_view_set_title(lv_obj_t *view, const char *title);
void lisa_ui_quota_qrcode_view_set_message(lv_obj_t *view, const char *message);
void lisa_ui_quota_qrcode_view_set_qr_image(lv_obj_t *view, const void *img_src);

#endif
