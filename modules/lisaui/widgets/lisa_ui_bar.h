#ifndef __LISA_UI_BAR_H__
#define __LISA_UI_BAR_H__

#include "lvgl.h"

typedef void (*lisa_ui_bar_nav_btn_click_event_cb_t)(void *user_data);

/**
 * 创建一个顶部任务栏
 * @param parent 父对象
 * @return 任务栏对象
 */
lv_obj_t *lisa_ui_bar_create(lv_obj_t *parent);

/**
 * 添加一个图标
 * @param bar 任务栏对象
 * @param icon_path 图标路径
 * @return 图标索引
 */
int lisa_ui_bar_icon_add(lv_obj_t *bar, const void *icon_path);

/**
 * 设置任务栏标题
 * @param bar 任务栏对象
 * @param title 标题
 */
void lisa_ui_bar_title_set(lv_obj_t *bar, const char *title);

/**
 * 设置导航按钮图标
 * @param bar 任务栏对象
 * @param icon_path 图标路径
 */
void lisa_ui_bar_nav_btn_icon_set(lv_obj_t *bar, const void *icon_path);

/**
 * 设置导航按钮点击事件回调
 * @param bar 任务栏对象
 * @param cb 回调函数
 * @param user_data 用户数据
 */
void lisa_ui_bar_nav_btn_click_event_cb_set(lv_obj_t *bar, lisa_ui_bar_nav_btn_click_event_cb_t cb, void *user_data);

/**
 * 隐藏任务栏标题
 * @param bar 任务栏对象
 */
void lisa_ui_bar_title_hide(lv_obj_t *bar);

/**
 * 显示任务栏标题
 * @param bar 任务栏对象
 */
void lisa_ui_bar_title_show(lv_obj_t *bar);

#endif
