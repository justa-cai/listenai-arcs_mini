#ifndef __LISA_UI_IMGS_VIEW_H__
#define __LISA_UI_IMGS_VIEW_H__

#include "lvgl.h"

lv_obj_t *lisa_ui_imgs_view_create(lv_obj_t *parent);
void lisa_ui_imgs_view_set_imgs(lv_obj_t *obj, void **srcs, char **names, uint32_t cnt);

#endif
