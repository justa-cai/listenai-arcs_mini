#ifndef __LISA_UI_SYS_BAR_H__
#define __LISA_UI_SYS_BAR_H__

#include "lvgl.h"
#include "lisa_ui_bar.h"

/**
 * 获取系统任务栏
 */
lv_obj_t *lisa_ui_sys_bar_get(void);

/**
 * 设置系统任务栏标题
 */
void lisa_ui_sys_bar_title_set(const char *title);

/**
 * 设置系统任务栏导航按钮图标
 */
void lisa_ui_sys_bar_btn_nav_img_set(const void *icon_path);

/**
 * 设置系统任务栏导航按钮点击事件回调
 */
void lisa_ui_sys_bar_btn_nav_click_event_cb_set(lisa_ui_bar_nav_btn_click_event_cb_t cb, void *user_data);

/**
 * 隐藏系统任务栏标题
 */
void lisa_ui_sys_bar_title_hide(void);

/**
 * 显示系统任务栏标题
 */
void lisa_ui_sys_bar_title_show(void);

/**
 * 重置系统任务栏父对象
 */
void lisa_ui_sys_bar_parent_reset(lv_obj_t *bar);

/**
 * 获取系统任务栏父对象
 */
lv_obj_t *lisa_ui_sys_bar_parent_get(void);

#endif
