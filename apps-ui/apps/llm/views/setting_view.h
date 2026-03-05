/**
 * @file setting_view.h
 * @brief Setting main page view header
 */

#ifndef __LISA_UI_SETTING_VIEW_H__
#define __LISA_UI_SETTING_VIEW_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "lisa_ui_llm_base.h"

/** Setting view class definition */
extern const lv_obj_class_t lisa_ui_setting_view_class;

/**
 * @brief Setting item click callback
 * @param name Item name that was clicked
 * @param user_data User data
 */
typedef void (*lisa_ui_setting_item_click_cb_t)(const char *name, void *user_data);

/**
 * @brief Back button click callback
 * @param user_data User data
 */
typedef void (*lisa_ui_setting_back_cb_t)(void *user_data);

/**
 * @brief Create setting view object
 * @param parent Parent object, NULL for current screen
 * @return lv_obj_t* Created setting view object, NULL on failure
 */
lv_obj_t *lisa_ui_setting_view_create(lv_obj_t *parent);

/**
 * @brief Add setting item card to 2x2 grid
 * @param obj Setting view object
 * @param label Item label text
 * @param icon_src LVGL image descriptor for icon
 * @param col Grid column (0 or 1)
 * @param row Grid row (0 or 1)
 */
void lisa_ui_setting_view_item_add(lv_obj_t *obj, const char *label, const void *icon_src, int col, int row);

/**
 * @brief Set item click callback
 * @param obj Setting view object
 * @param cb Callback function
 * @param user_data User data passed to callback
 */
void lisa_ui_setting_view_set_click_cb(lv_obj_t *obj, lisa_ui_setting_item_click_cb_t cb, void *user_data);

/**
 * @brief Clear all setting items
 * @param obj Setting view object
 */
void lisa_ui_setting_view_clear(lv_obj_t *obj);

/**
 * @brief Set back button click callback
 * @param obj Setting view object
 * @param cb Callback function
 * @param user_data User data passed to callback
 */
void lisa_ui_setting_view_set_back_cb(lv_obj_t *obj, lisa_ui_setting_back_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* __LISA_UI_SETTING_VIEW_H__ */
