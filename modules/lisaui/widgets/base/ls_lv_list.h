/**
 * @file lv_win.h
 *
 */

#ifndef LS_LV_LIST_H
#define LS_LV_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#ifdef __ZEPHYR__
#include <zephyr/kernel.h>
#include "lvgl.h"
#elif CONFIG_LVGL_ENV_SIMULATOR
#include "lvgl/lvgl.h"
#else
#include "lvgl.h"
#endif

// #if LV_USE_LS_LIST

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

LV_FONT_DECLARE(lv_font_notosans_cs_medium_14);
LV_FONT_DECLARE(lv_font_chinese_18);

lv_obj_t *ls_lv_list_create(lv_obj_t *parent);

lv_obj_t *ls_lv_list_add_text(lv_obj_t *list, const char *txt);

lv_obj_t *ls_lv_list_add_btn(lv_obj_t *list, const char *title_text, const char *tips_text, lv_event_cb_t event_cb,
                             void *user_data);
const char *ls_lv_list_get_btn_text(lv_obj_t *list, lv_obj_t *btn);

/**********************
 *      MACROS
 **********************/

// #endif /*LV_USE_LIST*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LS_LV_LIST_H*/
