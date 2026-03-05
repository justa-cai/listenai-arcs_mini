/**
 * @file setting_wakeup_view.h
 * @brief Wakeup settings view
 */

#ifndef __SETTING_WAKEUP_VIEW_H__
#define __SETTING_WAKEUP_VIEW_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "lisa_ui_llm_base.h"

/** Wakeup settings view class definition */
extern const lv_obj_class_t lisa_ui_setting_wakeup_view_class;

/**
 * @brief 返回按钮点击回调函数类型
 * @param user_data 用户数据
 */
typedef void (*lisa_ui_setting_wakeup_back_cb_t)(void *user_data);
typedef void (*lisa_ui_setting_wakeup_mode_changed_cb_t)(uint16_t mode, void *user_data);

/**
 * @brief Create wakeup settings view
 * @param parent Parent object
 * @return lv_obj_t* Created view object
 */
lv_obj_t *lisa_ui_setting_wakeup_view_create(lv_obj_t *parent);

/**
 * @brief 设置返回按钮点击回调
 * @param obj Wakeup view对象
 * @param cb 回调函数
 * @param user_data 传递给回调的用户数据
 */
void lisa_ui_setting_wakeup_view_set_back_cb(lv_obj_t *obj, lisa_ui_setting_wakeup_back_cb_t cb, void *user_data);
void lisa_ui_setting_wakeup_view_set_mode_changed_cb(lv_obj_t *obj, lisa_ui_setting_wakeup_mode_changed_cb_t cb, void *user_data);
void lisa_ui_setting_wakeup_view_set_mode(lv_obj_t *obj, uint16_t mode);

#ifdef __cplusplus
}
#endif

#endif /* __SETTING_WAKEUP_VIEW_H__ */
