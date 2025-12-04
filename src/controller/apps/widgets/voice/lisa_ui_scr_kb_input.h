#ifndef __LISA_UI_SCR_KB_INPUT_H__
#define __LISA_UI_SCR_KB_INPUT_H__

#include "lvgl.h"
#include "lisa_ui.h"

lv_obj_t *lisa_ui_scr_kb_input_create();
void lisa_ui_scr_kb_input_set_title(lv_obj_t *obj, const char *title);
const char * lisa_ui_scr_kb_input_get_title(lv_obj_t *obj);
void lisa_ui_scr_kb_input_set_tips(lv_obj_t *obj, const char *tips);
const char * lisa_ui_scr_kb_input_get_text(lv_obj_t *obj);

#endif /*__LISA_UI_SCR_KB_INPUT_H__*/
