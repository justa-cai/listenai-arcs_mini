#ifndef __LISA_UI_H__
#define __LISA_UI_H__

#include <stdint.h>
#include <stddef.h>

#include "lisa_ui_log.h"
#include "lisa_ui_nav_scr.h"

#include "lvgl.h"
#include "lv_i18n.h"

int lisa_ui_init(void);
void lisa_ui_free(void *p);
void *lisa_ui_malloc(uint32_t size);

#endif
