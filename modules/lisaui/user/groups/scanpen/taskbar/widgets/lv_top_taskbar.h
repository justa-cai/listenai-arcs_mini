/**
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LV_TOP_TASKBAR_H
#define LV_TOP_TASKBAR_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ========================
 * 常量和枚举定义
 * ========================
 */

/**
 * 自定义任务栏事件类型
 */
#define LV_TASKBAR_EVENT_BUTTON_CLICKED   (LV_EVENT_READY + 1)  /**< 按钮点击事件 */
#define LV_TASKBAR_EVENT_SWIPE            (LV_EVENT_READY + 2)  /**< 滑动手势事件 */

/**
 * 任务栏按钮类型枚举
 */
typedef enum {
    LV_TASKBAR_BTN_NONE  = 0,  /**< 无按钮 */
    LV_TASKBAR_BTN_BACK  = 1,  /**< 返回按钮 */
    LV_TASKBAR_BTN_CLOSE = 2,  /**< 关闭按钮 */
    LV_TASKBAR_BTN_HOME  = 3,  /**< 主页按钮 */
} lv_taskbar_btn_type_t;

/**
 * 任务栏事件数据结构
 */
typedef struct {
    lv_taskbar_btn_type_t btn_type;  /**< 按钮类型 */
} lv_taskbar_event_data_t;

/* 视图组件不存储模型数据，仅负责UI渲染 */

/**
 * ========================
 * 组件API函数声明
 * ========================
 */

/**
 * @brief 创建任务栏组件
 * @param parent 父对象
 * @return 任务栏对象
 */
lv_obj_t *lv_taskbar_create(lv_obj_t *parent);

/**
 * @brief 设置任务栏按钮点击事件回调
 * @param taskbar 任务栏对象
 * @param event_cb 事件回调函数
 * @param user_data 用户数据
 */
void lv_taskbar_set_event_cb(lv_obj_t *taskbar, lv_event_cb_t event_cb, void *user_data);

/**
 * @brief 更新任务栏时间
 * @param taskbar 任务栏对象
 * @param time_str 时间字符串
 */
void lv_taskbar_set_time(lv_obj_t *taskbar, const char *time_str);

/**
 * @brief 更新任务栏电池状态
 * @param taskbar 任务栏对象
 * @param level 电池电量（0-100）
 * @param is_charging 是否正在充电
 */
void lv_taskbar_set_battery(lv_obj_t *taskbar, uint8_t level, bool is_charging);

/**
 * @brief 显示或隐藏任务栏
 * @param taskbar 任务栏对象
 * @param visible 是否可见
 */
void lv_taskbar_set_visible(lv_obj_t *taskbar, bool visible);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_TOP_TASKBAR_H */