/**
 * @file lv_top_taskbar.c
 * @brief 顶部任务栏视图组件实现
 * @note 该文件实现了MVC架构中的View层组件
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include "lvgl.h"
#include "lisaui_common.h"
#include "lv_top_taskbar.h"
#include "lisaui_log.h"

#define TAG "lv.top.taskbar"

/**
 * ========================
 * 内部数据结构定义
 * ========================
 */

/**
 * 任务栏组件对象结构
 * 符合LVGL对象类机制的自定义组件
 */
typedef struct {
    lv_obj_t obj;              // 基类对象，必须放在第一位
    lv_obj_t *btn_back;        // 返回按钮
    lv_obj_t *btn_close;       // 关闭按钮
    lv_obj_t *btn_home;        // 主页按钮
    lv_obj_t *lbl_time;        // 时间标签
    lv_obj_t *battery_icon;    // 电池图标
    
    // 不存储模型数据，视图仅负责UI渲染
} lv_taskbar_t;

static void taskbar_btn_event_handler(lv_event_t *e);
/**
 * ========================
 * 组件类方法实现
 * ========================
 */

/**
 * @brief 获取任务栏对象指针
 * @param obj LVGL对象
 * @return 任务栏对象指针
 */
static inline lv_taskbar_t * lv_taskbar_get_instance(lv_obj_t *obj) {
    return (lv_taskbar_t *)obj;
}

/**
 * @brief 任务栏组件构造函数
 * @param class_p 组件类
 * @param obj LVGL对象
 */
static void lv_taskbar_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj) {
    LV_UNUSED(class_p);
    lv_taskbar_t *taskbar = lv_taskbar_get_instance(obj);
    
    // 设置基本样式和属性
    lv_obj_set_size(obj, LV_PCT(100), LV_DPX(LISAUI_STATUS_BAR_HEIGHT));
    lisaui_common_set_style_container(obj, lv_color_hex(0x000000), 0, 
                                    lv_color_hex(0x000000), 0, 0);
    
    // 不再初始化模型数据，视图不存储状态
    
    // 创建返回按钮
    taskbar->btn_back = lv_btn_create(obj);
    lv_obj_set_size(taskbar->btn_back, LV_DPX(50), LV_DPX(LISAUI_STATUS_BAR_HEIGHT - 10));
    lv_obj_align(taskbar->btn_back, LV_ALIGN_LEFT_MID, LV_DPX(4), LV_DPX(0));
    lv_obj_set_style_radius(taskbar->btn_back, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    lv_obj_t *label = lv_label_create(taskbar->btn_back);
    lv_label_set_text(label, LV_SYMBOL_LEFT);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    
    // 创建关闭按钮
    taskbar->btn_close = lv_btn_create(obj);
    lv_obj_set_size(taskbar->btn_close, LV_DPX(50), LV_DPX(LISAUI_STATUS_BAR_HEIGHT - 10));
    lv_obj_align_to(taskbar->btn_close, taskbar->btn_back, LV_ALIGN_OUT_RIGHT_MID, LV_DPX(2), 0);
    lv_obj_set_style_radius(taskbar->btn_close, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    lv_obj_t *label_close = lv_label_create(taskbar->btn_close);
    lv_label_set_text(label_close, LV_SYMBOL_CLOSE);
    lv_obj_align(label_close, LV_ALIGN_CENTER, 0, 0);
    
    // 创建主页按钮
    taskbar->btn_home = lv_btn_create(obj);
    lv_obj_set_size(taskbar->btn_home, LV_DPX(50), LV_DPX(LISAUI_STATUS_BAR_HEIGHT - 10));
    lv_obj_align_to(taskbar->btn_home, taskbar->btn_close, LV_ALIGN_OUT_RIGHT_MID, LV_DPX(2), 0);
    lv_obj_set_style_radius(taskbar->btn_home, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    lv_obj_t *label_home = lv_label_create(taskbar->btn_home);
    lv_label_set_text(label_home, LV_SYMBOL_HOME);
    lv_obj_align(label_home, LV_ALIGN_CENTER, 0, 0);
    
    // 创建时间标签
    taskbar->lbl_time = lv_label_create(obj);
    lv_label_set_text(taskbar->lbl_time, "--:--");
    lv_obj_align(taskbar->lbl_time, LV_ALIGN_RIGHT_MID, -LV_DPX(10), 0);
    lv_obj_set_style_text_color(taskbar->lbl_time, lv_color_white(), 0);
    
    // 创建电池图标
    taskbar->battery_icon = lv_label_create(obj);
    lv_label_set_text(taskbar->battery_icon, LV_SYMBOL_BATTERY_FULL);
    lv_obj_align_to(taskbar->battery_icon, taskbar->lbl_time, LV_ALIGN_OUT_LEFT_MID, -LV_DPX(10), 0);
    lv_obj_set_style_text_color(taskbar->battery_icon, lv_color_white(), 0);
    
    // 直接在构造函数中注册按钮事件处理函数
    LISAUI_LOGI(TAG, "注册按钮事件处理函数");
    lv_obj_add_event_cb(taskbar->btn_back, taskbar_btn_event_handler, LV_EVENT_CLICKED, obj);
    lv_obj_add_event_cb(taskbar->btn_close, taskbar_btn_event_handler, LV_EVENT_CLICKED, obj);
    lv_obj_add_event_cb(taskbar->btn_home, taskbar_btn_event_handler, LV_EVENT_CLICKED, obj);
}

/**
 * @brief 任务栏组件析构函数
 * @param class_p 组件类
 * @param obj LVGL对象
 */
static void lv_taskbar_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj) {
    LV_UNUSED(class_p);
    lv_taskbar_t *taskbar = lv_taskbar_get_instance(obj);
    
    // 在LVGL中，子对象会自动删除，所以不需要手动删除子对象
    // 如果有其他需要清理的资源，可以在这里处理
}

/**
 * @brief 按钮事件处理函数
 * @param e 事件对象
 */
static void taskbar_btn_event_handler(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *taskbar_obj = lv_event_get_user_data(e);
    lv_taskbar_t *taskbar = lv_taskbar_get_instance(taskbar_obj);
    
    // 获取点击的按钮类型
    lv_taskbar_btn_type_t btn_type = LV_TASKBAR_BTN_NONE;
    
    if (btn == taskbar->btn_back) {
        btn_type = LV_TASKBAR_BTN_BACK;
    } else if (btn == taskbar->btn_close) {
        btn_type = LV_TASKBAR_BTN_CLOSE;
    } else if (btn == taskbar->btn_home) {
        btn_type = LV_TASKBAR_BTN_HOME;
    }
    
    // 执行回调函数
    if (event_code == LV_EVENT_CLICKED && btn_type != LV_TASKBAR_BTN_NONE) {
        lv_taskbar_event_data_t event_data;
        event_data.btn_type = btn_type;
        
        // 回调注册的事件处理函数
        lv_event_send(taskbar_obj, LV_TASKBAR_EVENT_BUTTON_CLICKED, &event_data);
    }
}

/**
 * @brief 任务栏组件事件处理函数
 * @param class_p 组件类
 * @param e 事件对象
 */
static void lv_taskbar_event(const lv_obj_class_t *class_p, lv_event_t *e) {
    LV_UNUSED(class_p);
    
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    lv_taskbar_t *taskbar = lv_taskbar_get_instance(obj);

    // 不再依赖LV_EVENT_READY事件，直接在构造函数中注册按钮事件
    if (code == LV_EVENT_GESTURE) {
        if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_TOP) {
            lv_indev_wait_release(lv_indev_get_act());
            lv_event_send(obj, LV_TASKBAR_EVENT_SWIPE, NULL);
        }
    }
}

/**
 * 任务栏组件类定义
 */
const lv_obj_class_t lv_taskbar_class = {
    .constructor_cb = lv_taskbar_constructor,
    .destructor_cb = lv_taskbar_destructor,
    .event_cb = lv_taskbar_event,
    .instance_size = sizeof(lv_taskbar_t),
    .base_class = &lv_obj_class
};

/**
 * ========================
 * 公共API函数实现
 * ========================
 */

/**
 * @brief 创建任务栏组件
 * @param parent 父对象
 * @return 任务栏对象
 */
lv_obj_t *lv_taskbar_create(lv_obj_t *parent) {
    lv_obj_t *obj = lv_obj_class_create_obj(&lv_taskbar_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

/**
 * @brief 设置任务栏按钮点击事件回调
 * @param taskbar 任务栏对象
 * @param event_cb 事件回调函数
 * @param user_data 用户数据
 */
void lv_taskbar_set_event_cb(lv_obj_t *taskbar, lv_event_cb_t event_cb, void *user_data) {
    if (!taskbar) return;
    lv_obj_add_event_cb(taskbar, event_cb, LV_TASKBAR_EVENT_BUTTON_CLICKED, user_data);
    lv_obj_add_event_cb(taskbar, event_cb, LV_TASKBAR_EVENT_SWIPE, user_data);
}

/**
 * @brief 更新任务栏时间
 * @param taskbar 任务栏对象
 * @param time_str 时间字符串
 */
void lv_taskbar_set_time(lv_obj_t *taskbar, const char *time_str) {
    if (!taskbar || !time_str) return;
    
    lv_taskbar_t *taskbar_obj = lv_taskbar_get_instance(taskbar);
    
    // 直接更新UI，不存储数据
    
    // 更新视图
    lv_label_set_text(taskbar_obj->lbl_time, time_str);
}

/**
 * @brief 更新任务栏电池状态
 * @param taskbar 任务栏对象
 * @param level 电池电量（0-100）
 * @param is_charging 是否正在充电
 */
void lv_taskbar_set_battery(lv_obj_t *taskbar, uint8_t level, bool is_charging) {
    if (!taskbar) return;
    
    lv_taskbar_t *taskbar_obj = lv_taskbar_get_instance(taskbar);
    
    // 直接更新UI，不存储数据
    
    // 更新视图
    const char *battery_symbol;
    if (is_charging) {
        battery_symbol = LV_SYMBOL_CHARGE;
    } else if (level > 75) {
        battery_symbol = LV_SYMBOL_BATTERY_FULL;
    } else if (level > 50) {
        battery_symbol = LV_SYMBOL_BATTERY_3;
    } else if (level > 25) {
        battery_symbol = LV_SYMBOL_BATTERY_2;
    } else {
        battery_symbol = LV_SYMBOL_BATTERY_1;
    }
    
    lv_label_set_text(taskbar_obj->battery_icon, battery_symbol);
}

/**
 * @brief 显示或隐藏任务栏
 * @param taskbar 任务栏对象
 * @param visible 是否可见
 */
void lv_taskbar_set_visible(lv_obj_t *taskbar, bool visible) {
    if (!taskbar) return;
    
    lv_taskbar_t *taskbar_obj = lv_taskbar_get_instance(taskbar);
    
    // 直接更新UI，不存储数据
    
    // 更新视图
    if (visible) {
        lv_obj_clear_flag(taskbar, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(taskbar, LV_OBJ_FLAG_HIDDEN);
    }
}