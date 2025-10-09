#ifndef __LISA_UI_IMGS_GROUP_H__
#define __LISA_UI_IMGS_GROUP_H__

#include "lvgl.h"

/**
 * @brief 创建一个图片组
 *
 * @param parent
 * @return lv_obj_t*
 */
lv_obj_t *lisa_ui_imgs_group_create(lv_obj_t *parent);

/**
 * @brief 向图片组中添加一个图片
 *
 * @param obj
 * @param img_path 图片路径
 * @param name 图片名称
 * @note 图片的名称以及路径的内存不可被释放
 */
void lisa_ui_imgs_group_add_img(lv_obj_t *obj, const void *img_path, const char *name);

/**
 * @brief 向图片组中添加一个图片并显示当前图片
 *
 * @param obj
 * @param img_path
 * @param name
 */
void lisa_ui_imgs_group_add_and_show(lv_obj_t *obj, const void *img_path, const char *name);

/**
 * @brief 显示图片组
 *
 * @param obj
 */
void lisa_ui_imgs_group_show(lv_obj_t *obj);

/**
 * @brief 显示下一张图片
 *
 * @param obj
 */
void lisa_ui_imgs_group_show_next(lv_obj_t *obj);

/**
 * @brief 显示上一张图片
 *
 * @param obj
 */
void lisa_ui_imgs_group_show_prev(lv_obj_t *obj);

/**
 * @brief 清空图片组
 *
 * @param obj
 */
void lisa_ui_imgs_group_clear(lv_obj_t *obj);

/**
 * @brief 设置图片组是否循环
 *
 * @param obj
 * @param loop
 */
void lisa_ui_imgs_group_set_loop(lv_obj_t *obj, bool loop);

/**
 * @brief 获取当前图片的路径
 *
 * @param obj
 * @return void*
 */
const void *lisa_ui_imgs_group_img_get_curr_src(lv_obj_t *obj);

/**
 * @brief 获取当前图片的名称
 *
 * @param obj
 * @return const char*
 */
const char *lisa_ui_imgs_group_img_get_curr_name(lv_obj_t *obj);

/**
 * @brief 显示图片名称
 *
 * @param obj
 */
void lisa_ui_imgs_group_name_show(lv_obj_t *obj);

/**
 * @brief 隐藏图片名称
 *
 * @param obj
 */
void lisa_ui_imgs_group_name_hide(lv_obj_t *obj);

#endif
