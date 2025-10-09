/**
 * @file lv_scanner.h
 * @brief Scanner widget for OCR functionality
 */

#ifndef LV_SCANNER_H
#define LV_SCANNER_H

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
 * Create a scanner widget
 * @param parent Parent object
 * @return Pointer to the created scanner
 */
lv_obj_t *lv_scanner_create(lv_obj_t *parent);

/**
 * Set the background color of the scanner
 * @param obj Scanner object
 * @param color Background color
 */
void lv_scanner_set_bg_color(lv_obj_t *obj, lv_color_t color);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_SCANNER_H */