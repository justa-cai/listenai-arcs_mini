#ifndef __LISA_UI_SCR_AI_HUMAN_MAIN_H__
#define __LISA_UI_SCR_AI_HUMAN_MAIN_H__

#include "lvgl.h"

/**
 * @brief AI人像主界面图片点击事件回调
 *
 * @param user_data 用户数据
 * @param img_name 图片名称
 */
typedef void (*lisa_ui_scr_ai_human_main_imgs_click_event_cb_t)(void *user_data, const char *img_name);

typedef void (*lisa_ui_scr_ai_human_main_btn_click_event_cb_t)(void *user_data);

/**
 * @brief 创建AI人像主界面
 *
 * @param parent 父对象
 * @return lv_obj_t* AI人像主界面对象
 */
lv_obj_t *lisa_ui_scr_ai_human_main_create(lv_obj_t *parent);

/**
 * @brief 清空AI人像主界面的图片
 *
 * @param obj AI人像主界面对象
 */
void lisa_ui_scr_ai_human_main_imgs_clear(lv_obj_t *obj);

/**
 * @brief 向AI人像主界面添加图片
 *
 * @param obj
 * @param img_path 图片路径
 * @param name 图片名称
 */
void lisa_ui_scr_ai_human_main_imgs_add(lv_obj_t *obj, const void *img_path, const char *name);

/**
 * @brief 添加AI人像主界面图片点击事件回调
 *
 * @param obj
 * @param cb    事件回调
 * @param user_data   用户数据
 */
void lisa_ui_scr_ai_human_main_imgs_click_event_cb_set(lv_obj_t *obj,
                                                       lisa_ui_scr_ai_human_main_imgs_click_event_cb_t cb,
                                                       void *user_data);

/**
 * @brief 移除AI人像主界面图片点击事件回调
 *
 * @param obj
 */
void lisa_ui_scr_ai_human_main_imgs_click_event_cb_remove(lv_obj_t *obj);

/**
 * @brief AI人像主界面图片前一张
 *
 * @param obj
 */
void lisa_ui_scr_ai_human_main_imgs_prev_show(lv_obj_t *obj);

/**
 * @brief AI人像主界面图片后一张
 *
 * @param obj
 */
void lisa_ui_scr_ai_human_main_imgs_next_show(lv_obj_t *obj);

/**
 * @brief 显示按钮
 *
 * @param obj
 */
void lisa_ui_scr_ai_human_main_show_btn(lv_obj_t *obj, const char *btn_text);

/**
 * @brief 隐藏按钮
 *
 * @param obj
 */
void lisa_ui_scr_ai_human_main_hide_btn(lv_obj_t *obj);

/**
 * @brief 显示提示
 *
 * @param obj
 * @param tips
 */
void lisa_ui_scr_ai_human_main_show_tips(lv_obj_t *obj, const char *tips);

/**
 * @brief 隐藏提示
 *
 * @param obj
 */
void lisa_ui_scr_ai_human_main_hide_tips(lv_obj_t *obj);

#endif
