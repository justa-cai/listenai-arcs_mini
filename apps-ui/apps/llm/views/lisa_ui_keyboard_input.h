/**
 * @file lisa_ui_keyboard_input.h
 * @brief 通用键盘输入组件
 * @description 提供一个全屏的键盘输入界面,支持密码输入模式
 */

#ifndef __LISA_UI_KEYBOARD_INPUT_H__
#define __LISA_UI_KEYBOARD_INPUT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * @brief 键盘输入确认回调函数类型
 * @param text 输入的文本内容
 * @param user_data 用户数据
 */
typedef void (*lisa_ui_keyboard_input_confirm_cb_t)(const char *text, void *user_data);

/**
 * @brief 键盘输入取消回调函数类型
 * @param user_data 用户数据
 */
typedef void (*lisa_ui_keyboard_input_cancel_cb_t)(void *user_data);

/**
 * @brief 创建键盘输入组件
 * @param parent 父对象
 * @return lv_obj_t* 创建的键盘输入对象
 */
lv_obj_t *lisa_ui_keyboard_input_create(lv_obj_t *parent);

/**
 * @brief 设置标题文本
 * @param obj 键盘输入对象
 * @param title 标题文本
 */
void lisa_ui_keyboard_input_set_title(lv_obj_t *obj, const char *title);

/**
 * @brief 设置输入框提示文本
 * @param obj 键盘输入对象
 * @param placeholder 提示文本
 */
void lisa_ui_keyboard_input_set_placeholder(lv_obj_t *obj, const char *placeholder);

/**
 * @brief 设置输入框标签文本 (如"密码:"、"用户名:"等)
 * @param obj 键盘输入对象
 * @param label 标签文本
 */
void lisa_ui_keyboard_input_set_label(lv_obj_t *obj, const char *label);

/**
 * @brief 设置密码模式
 * @param obj 键盘输入对象
 * @param enable true=密码模式, false=普通模式
 */
void lisa_ui_keyboard_input_set_password_mode(lv_obj_t *obj, bool enable);

/**
 * @brief 设置确认回调
 * @param obj 键盘输入对象
 * @param confirm_cb 确认回调函数
 * @param user_data 用户数据
 */
void lisa_ui_keyboard_input_set_confirm_cb(lv_obj_t *obj,
                                           lisa_ui_keyboard_input_confirm_cb_t confirm_cb,
                                           void *user_data);

/**
 * @brief 设置取消回调
 * @param obj 键盘输入对象
 * @param cancel_cb 取消回调函数
 * @param user_data 用户数据
 */
void lisa_ui_keyboard_input_set_cancel_cb(lv_obj_t *obj,
                                          lisa_ui_keyboard_input_cancel_cb_t cancel_cb,
                                          void *user_data);

/**
 * @brief 显示键盘输入界面
 * @param obj 键盘输入对象
 */
void lisa_ui_keyboard_input_show(lv_obj_t *obj);

/**
 * @brief 隐藏键盘输入界面
 * @param obj 键盘输入对象
 */
void lisa_ui_keyboard_input_hide(lv_obj_t *obj);

/**
 * @brief 获取输入的文本
 * @param obj 键盘输入对象
 * @return const char* 输入的文本
 */
const char *lisa_ui_keyboard_input_get_text(lv_obj_t *obj);

/**
 * @brief 清空输入框
 * @param obj 键盘输入对象
 */
void lisa_ui_keyboard_input_clear(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif /* __LISA_UI_KEYBOARD_INPUT_H__ */
