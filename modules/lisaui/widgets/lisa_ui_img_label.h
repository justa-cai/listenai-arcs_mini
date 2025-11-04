#ifndef LISA_UI_IMG_LABEL_H
#define LISA_UI_IMG_LABEL_H

#include "lvgl.h"

lv_obj_t *lisa_ui_img_label_create(lv_obj_t *parent, const void *img_path, const char *label);
int lisa_ui_img_label_img_set(lv_obj_t *obj, const void *img_path);
int lisa_ui_img_label_label_set(lv_obj_t *obj, const char *label);

#endif