#ifndef LISA_UI_SCR_CAMERA_H
#define LISA_UI_SCR_CAMERA_H

#include "lvgl.h"
#include "lisa_ui.h"

typedef void (*lisa_ui_scr_camera_btn_shut_click_event_cb_t)(void *user_data);
typedef void (*lisa_ui_scr_camera_btn_back_click_event_cb_t)(void *user_data);

/**
 * 创建相机界面
 */
lv_obj_t *lisa_ui_scr_camera_create(lv_obj_t *parent);

/**
 * 设置相机界面的图片
 */
void lisa_ui_scr_camera_img_set(lv_obj_t *obj, const void *img_data);

/**
 * 设置相机界面的取景框图片
 */
void lisa_ui_scr_camera_rec_img_set(lv_obj_t *obj, const void *img_data);

/**
 * 设置相机界面的拍照按钮点击事件回调
 */
void lisa_ui_scr_camera_btn_shut_click_event_cb_set(lv_obj_t *obj, lisa_ui_scr_camera_btn_shut_click_event_cb_t cb,
                                                    void *user_data);

/**
 * 设置相机界面的返回按钮点击事件回调
 */
void lisa_ui_scr_camera_btn_back_click_event_cb_set(lv_obj_t *obj, lisa_ui_scr_camera_btn_back_click_event_cb_t cb,
                                                    void *user_data);

/**
 * 获取相机界面的图片
 */
const void* lisa_ui_scr_camera_img_get(lv_obj_t *obj);

/**
 * 设置相机界面的图片旋转角度
 */
void lisa_ui_scr_camera_img_rotate(lv_obj_t *obj, int angle);

#endif
