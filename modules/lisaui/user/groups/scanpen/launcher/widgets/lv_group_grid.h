/**
 * @file lv_group_grid.h
 * @brief 应用组网格视图组件头文件
 */
#ifndef LV_GROUP_GRID_H
#define LV_GROUP_GRID_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "lisaui_common.h"

/*********************
 * 常量和定义
 *********************/

/**
 * 组网格事件类型
 */
enum {
    LV_GROUP_GRID_EVENT_ITEM_CLICK = LV_EVENT_READY + 1,  /**< 点击项目事件 */
    LV_GROUP_GRID_EVENT_ITEM_PRESS,                       /**< 按下项目事件 */
    LV_GROUP_GRID_EVENT_ITEM_RELEASE,                     /**< 释放项目事件 */
};

/**
 * 事件回调数据结构
 */
typedef struct {
    uint32_t group_id;        /**< 组ID */
    uint32_t item_index;      /**< 项目索引 */
    void *user_data;          /**< 用户数据 */
} lv_group_grid_event_data_t;

/**
 * 网格项目数据结构
 */
typedef struct {
    const void *img_src;       /**< 图标图像源 */
    const char *title;         /**< 项目标题 */
    uint32_t group_id;               /**< 组ID */
    uint8_t zoom;              /**< 图像缩放系数 */
    void *user_data;           /**< 用户数据指针 */
} lv_group_grid_item_t;

/*********************
 * 公共API函数
 *********************/

/**
 * 创建组网格视图组件
 * @param parent 父对象
 * @return 创建的组网格对象
 */
lv_obj_t *lv_group_grid_create(lv_obj_t *parent);

/**
 * 设置组网格的行列数
 * @param obj 组网格对象
 * @param rows 行数
 * @param cols 列数
 */
void lv_group_grid_set_layout(lv_obj_t *obj, uint8_t rows, uint8_t cols);

/**
 * 添加项目到组网格
 * @param obj 组网格对象
 * @param item_data 项目数据
 * @return 成功返回true，失败返回false
 */
bool lv_group_grid_add_item(lv_obj_t *obj, const lv_group_grid_item_t *item_data);

/**
 * 清空所有图标
 * @param obj 组网格对象
 */
void lv_group_grid_clear_all(lv_obj_t *obj);

/**
 * 设置事件回调函数
 * @param obj 组网格对象
 * @param event_cb 事件回调函数
 * @param user_data 用户数据
 */
void lv_group_grid_set_event_cb(lv_obj_t *obj, lv_event_cb_t event_cb, void *user_data);

/**
 * 设置背景颜色
 * @param obj 组网格对象
 * @param color 颜色值
 */
void lv_group_grid_set_bg_color(lv_obj_t *obj, lv_color_t color);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_GROUP_GRID_H*/