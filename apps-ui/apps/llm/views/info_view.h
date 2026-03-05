/**
 * @file info_view.h
 * @brief Info/QR Code page view header
 * 
 * Provides view interface for info/qrcode display page.
 */

#ifndef __LISA_UI_INFO_VIEW_H__
#define __LISA_UI_INFO_VIEW_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "lisa_ui_llm_base.h"

/** Info view class definition */
extern const lv_obj_class_t lisa_ui_info_view_class;

/**
 * @brief Create info view object
 * 
 * @param parent Parent object, NULL for current screen
 * @return lv_obj_t* Created info view object, NULL on failure
 */
lv_obj_t *lisa_ui_info_view_create(lv_obj_t *parent);

/**
 * @brief Set top instruction text
 * 
 * @param obj Info view object
 * @param text Text to display, NULL for default text
 */
void lisa_ui_info_view_set_top_text(lv_obj_t *obj, const char *text);

/**
 * @brief Set bottom information text
 * 
 * @param obj Info view object
 * @param text Text to display, NULL to clear
 */
void lisa_ui_info_view_set_bottom_text(lv_obj_t *obj, const char *text);

/**
 * @brief Set QR code image source
 * 
 * @param obj Info view object
 * @param src Image source (file path, C array, or symbol)
 */
void lisa_ui_info_view_set_qr_image(lv_obj_t *obj, const void *src);

/**
 * @brief Get QR code image object
 * 
 * @param obj Info view object
 * @return lv_obj_t* QR image object for advanced operations
 */
lv_obj_t *lisa_ui_info_view_get_qr_image(lv_obj_t *obj);

/**
 * @brief Set QR code image from RGB565 data
 * 
 * @param obj Info view object
 * @param data RGB565 image data pointer
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param data_size Total data size in bytes
 */
void lisa_ui_info_view_set_qr_rgb565(lv_obj_t *obj, const uint16_t *data, uint16_t width, uint16_t height, uint32_t data_size);

#ifdef __cplusplus
}
#endif

#endif /* __LISA_UI_INFO_VIEW_H__ */
