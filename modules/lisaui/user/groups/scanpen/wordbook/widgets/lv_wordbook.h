/**
 * @file lv_wordbook.h
 * @brief wordbook widget for wordbook functionality
 */

#ifndef LV_WORDBOOK_H
#define LV_WORDBOOK_H

#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 * MACROS
 *********************/

/*********************
 * TYPEDEFS
 *********************/

/*********************
 * PUBLIC API
 *********************/



/**
 * Create a wordbook widget
 * @param parent Parent object
 * @return Pointer to the created wordbook
 */
lv_obj_t *lv_wordbook_create(lv_obj_t *parent);

/**
 * Set the background color of the wordbook
 * @param obj wordbook object
 * @param color Background color
 */
void lv_wordbook_set_bg_color(lv_obj_t *obj, lv_color_t color);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_WORDBOOK_H */