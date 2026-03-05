/**
 * @file setting_common_view.h
 * @brief Common settings view (Volume, Brightness)
 */

#ifndef __SETTING_COMMON_VIEW_H__
#define __SETTING_COMMON_VIEW_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "lisa_ui_llm_base.h"

/** Common settings view class definition */
extern const lv_obj_class_t lisa_ui_setting_common_view_class;

/**
 * @brief Back button click callback
 * @param user_data User data
 */
typedef void (*lisa_ui_setting_common_back_cb_t)(void *user_data);

/**
 * @brief Volume changed callback
 * @param volume New volume value (0-100)
 * @param user_data User data
 */
typedef void (*lisa_ui_setting_common_volume_changed_cb_t)(uint8_t volume, void *user_data);

/**
 * @brief Brightness changed callback
 * @param brightness New brightness value (0-100)
 * @param user_data User data
 */
typedef void (*lisa_ui_setting_common_brightness_changed_cb_t)(uint8_t brightness, void *user_data);

/**
 * @brief Create common settings view (Volume/Brightness sliders)
 * @param parent Parent object
 * @return lv_obj_t* Created view object
 */
lv_obj_t *lisa_ui_setting_common_view_create(lv_obj_t *parent);

/**
 * @brief Set volume value and update display
 * @param obj View object
 * @param volume Volume level (0-100)
 */
void lisa_ui_setting_common_view_set_volume(lv_obj_t *obj, uint8_t volume);

/**
 * @brief Set brightness value and update display
 * @param obj View object
 * @param brightness Brightness level (0-100)
 */
void lisa_ui_setting_common_view_set_brightness(lv_obj_t *obj, uint8_t brightness);

/**
 * @brief Set back button click callback
 * @param obj View object
 * @param cb Callback function
 * @param user_data User data passed to callback
 */
void lisa_ui_setting_common_view_set_back_cb(lv_obj_t *obj, lisa_ui_setting_common_back_cb_t cb, void *user_data);

/**
 * @brief Set volume changed callback
 * @param obj View object
 * @param cb Callback function
 * @param user_data User data passed to callback
 */
void lisa_ui_setting_common_view_set_volume_changed_cb(lv_obj_t *obj, lisa_ui_setting_common_volume_changed_cb_t cb, void *user_data);

/**
 * @brief Set brightness changed callback
 * @param obj View object
 * @param cb Callback function
 * @param user_data User data passed to callback
 */
void lisa_ui_setting_common_view_set_brightness_changed_cb(lv_obj_t *obj, lisa_ui_setting_common_brightness_changed_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* __SETTING_COMMON_VIEW_H__ */
