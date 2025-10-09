#ifndef __LISA_UI_SCR_BASE_H__
#define __LISA_UI_SCR_BASE_H__

#include "lvgl.h"

struct lisa_ui_scr_base {
    lv_obj_t obj;
    lv_obj_t *bar;       /** 任务栏 */
    lv_obj_t *container; /** 页面容器, 任务栏下方, 页面内容在此容器中布局 */
    char title[16];      /** 页面标题 */
};

typedef struct lisa_ui_scr_base lisa_ui_scr_base_t;
/**
 * @brief 获取页面容器
 */
lv_obj_t *lisa_ui_scr_base_container_get(lv_obj_t *obj);
/**
 * @brief 隐藏页面任务栏
 */
void lisa_ui_scr_base_bar_hide(lv_obj_t *obj);
/**
 * @brief 显示页面任务栏
 */
void lisa_ui_scr_base_bar_show(lv_obj_t *obj);
/**
 * @brief 设置页面标题
 */
void lisa_ui_scr_base_title_set(lv_obj_t *obj, const char *title);
/**
 * @brief 删除页面
 */
void lisa_ui_scr_base_del(lv_obj_t *obj);
/**
 * @brief 显示页面
 */
void lisa_ui_scr_base_show(lv_obj_t *obj);
/**
 * @brief 隐藏页面
 */
void lisa_ui_scr_base_hide(lv_obj_t *obj);
/**
 * @brief 隐藏页面标题
 */
void lisa_ui_scr_base_title_hide(lv_obj_t *obj);
/**
 * @brief 显示页面标题
 */
void lisa_ui_scr_base_title_show(lv_obj_t *obj);

#endif
