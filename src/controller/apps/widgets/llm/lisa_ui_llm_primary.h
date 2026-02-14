/**
 * @file lisa_ui_llm_primary.h
 * @brief LLM UI主要组件头文件
 * 
 * 继承自LLM UI基础组件，提供具体的任务栏元素实现，
 * 包括WiFi图标、状态文本和电量图标，以及容器中的emoji动画和文本显示。
 * 
 * @version 1.0.0
 * @date 2024
 * @author Lisa UI Team
 */

#ifndef __LISA_UI_LLM_PRIMARY_H__
#define __LISA_UI_LLM_PRIMARY_H__

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================
 * 包含文件
 *==========================================*/
#include "lisa_ui_llm_base.h"
#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>

/*===========================================
 * 宏定义
 *==========================================*/
/** 状态文本最大长度 */
#define LISA_UI_LLM_PRIMARY_MAX_STATUS_LENGTH    32

/** 内容文本最大长度 */
#define LISA_UI_LLM_PRIMARY_MAX_CONTENT_LENGTH   256

/** 类型检查宏 */
#define LISA_UI_LLM_PRIMARY_CLASS_CHECK(obj) \
    (lv_obj_has_class(obj, &lisa_ui_llm_primary_class))

/** 根据每帧时长计算总动画时长的辅助宏 */
#define LISA_UI_LLM_DURATION_BY_FRAME(frame_duration_ms, frame_count) \
    ((frame_duration_ms) * (frame_count))

/*===========================================
 * 数据结构
 *==========================================*/

/**
 * @brief emoji 动画类型枚举
 */
typedef enum {
    LISA_UI_EMOJI_UNKNOW = 0,
    LISA_UI_EMOJI_LOVE,
    LISA_UI_EMOJI_SAD,
    LISA_UI_EMOJI_LAUGH,
    LISA_UI_EMOJI_SQUINT,
    LISA_UI_EMOJI_ANGRY,
    LISA_UI_EMOJI_EYE,
    LISA_UI_EMOJI_BLINK,
    LISA_UI_EMOJI_HUG,
    LISA_UI_EMOJI_PUZZLED,
    LISA_UI_EMOJI_WAKEUP,
    LISA_UI_EMOJI_SLEEPY,
    LISA_UI_EMOJI_WAIT,
    LISA_UI_EMOJI_BATTERY,
    LISA_UI_EMOJI_CHARGING,
    LISA_UI_EMOJI_START_BATTERY_CHANGE,
    LISA_UI_EMOJI_HAPPY,
    LISA_UI_EMOJI_CUTE,
    LISA_UI_EMOJI_MAX,
} lisa_ui_emoji_type_e;

/**
 * @brief LLM UI主要组件结构体
 * 
 * 继承自LLM UI基础组件，添加具体的任务栏元素和容器内容
 */
struct lisa_ui_llm_primary {
    lisa_ui_llm_base_t base_obj;                           /*!< 基础对象 */
    
    /* 任务栏元素 */
    lv_obj_t *wifi_icon;                                   /*!< WiFi图标 */
    lv_obj_t *interactive_mode_icon;                       /*!< 交互模式图标 */
    lv_obj_t *music_icon;                                  /*!< 音乐播放图标 */
    lv_obj_t *status_label;                                /*!< 状态文本标签 */
    lv_obj_t *alarm_icon;                                  /*!< 闹钟图标 */
    lv_obj_t *battery_icon;                                /*!< 电量图标 */
    
    /* 容器内容元素 */
    lv_obj_t *emoji_container;                             /*!< emoji 动画容器 */
    lv_obj_t *emoji_img;                                   /*!< emoji 动画图片 */
    lv_obj_t *camera_img;                                  /*!< 拍照图片显示（覆盖在emoji上层） */
    lv_obj_t *net_img;                                     /*!< 网络图片显示（与拍照图片分离，避免属性互相影响） */
    lv_obj_t *image_hint_label;                            /*!< 图片提示文本标签 */
    lv_obj_t *content_container;                           /*!< 内容文本容器 */
    lv_obj_t *content_label;                               /*!< 内容文本标签 */
    
    /* 动画相关 */
    const lv_img_dsc_t *emoji_images;                      /*!< emoji 图片数组 */
    uint32_t emoji_images_count;                           /*!< emoji 图片数量 */
    uint32_t current_emoji_frame;                          /*!< 当前动画帧 */
    uint32_t first_frame_delay;                            /*!< 第一帧延迟时间(毫秒) */
    uint32_t frame_duration;                               /*!< 每帧时长(毫秒) */
    lv_timer_t *emoji_timer;                               /*!< emoji动画定时器 */
    bool is_first_frame_delayed;                           /*!< 是否正在第一帧延迟中 */
    lv_timer_t *net_img_timer;                             /*!< 网络图片自动隐藏定时器 */
    
    /* 循环播放控制 */
    uint32_t loop_count;                                   /*!< 当前循环次数 */
    uint32_t target_loops;                                 /*!< 目标循环次数，0表示无限循环 */
    bool loop_mode_enabled;                                /*!< 是否启用循环模式 */
};

/** LLM UI主要组件类型定义 */
typedef struct lisa_ui_llm_primary lisa_ui_llm_primary_t;

/*===========================================
 * 全局变量声明
 *==========================================*/

/** LLM UI主要组件类定义 */
extern const lv_obj_class_t lisa_ui_llm_primary_class;

/*===========================================
 * 函数声明
 *==========================================*/

/**
 * @brief 创建LLM UI主要组件
 * 
 * @param parent 父对象，如果为NULL则使用当前活动屏幕
 * @return lv_obj_t* 创建的LLM UI主要组件对象，失败返回NULL
 * 
 * @note 该函数会自动创建任务栏的所有元素和容器内容
 * 
 * @code
 * // 创建LLM UI主要组件
 * lv_obj_t *llm_ui = lisa_ui_llm_primary_create(lv_scr_act());
 * if (llm_ui != NULL) {
 *     // 组件创建成功，包含完整的任务栏和内容区域
 * }
 * @endcode
 */
lv_obj_t *lisa_ui_llm_primary_create(lv_obj_t *parent);

/**
 * @brief 获取WiFi图标对象
 * 
 * @param obj LLM UI主要组件对象
 * @return lv_obj_t* WiFi图标对象，失败返回NULL
 */
lv_obj_t *lisa_ui_llm_primary_wifi_icon_get(lv_obj_t *obj);

/**
 * @brief 获取状态文本标签对象
 * 
 * @param obj LLM UI主要组件对象
 * @return lv_obj_t* 状态文本标签对象，失败返回NULL
 */
lv_obj_t *lisa_ui_llm_primary_status_label_get(lv_obj_t *obj);

/**
 * @brief 获取电量图标对象
 * 
 * @param obj LLM UI主要组件对象
 * @return lv_obj_t* 电量图标对象，失败返回NULL
 */
lv_obj_t *lisa_ui_llm_primary_battery_icon_get(lv_obj_t *obj);

/**
 * @brief 获取emoji动画图片对象
 * 
 * @param obj LLM UI主要组件对象
 * @return lv_obj_t* emoji动画图片对象，失败返回NULL
 */
lv_obj_t *lisa_ui_llm_primary_emoji_img_get(lv_obj_t *obj);

/**
 * @brief 获取内容文本标签对象
 * 
 * @param obj LLM UI主要组件对象
 * @return lv_obj_t* 内容文本标签对象，失败返回NULL
 */
lv_obj_t *lisa_ui_llm_primary_content_label_get(lv_obj_t *obj);

/**
 * @brief 设置状态文本
 * 
 * @param obj LLM UI主要组件对象
 * @param status 状态文本，如果为NULL则清空状态
 *
 */
void lisa_ui_llm_primary_set_status_text(lv_obj_t *obj, const char *status);

/**
 * @brief 设置内容文本
 * 
 * @param obj LLM UI主要组件对象
 * @param content 内容文本，如果为NULL则清空内容
 * 
 */
void lisa_ui_llm_primary_set_content_text(lv_obj_t *obj, const char *content);

/**
 * @brief 设置内容文本(追加)
 * 
 * @param obj LLM UI主要组件对象
 * @param content 内容文本，如果为NULL则清空内容
 *
 */
void lisa_ui_llm_primary_add_content_text(lv_obj_t *obj, const char *content);

/**
 * @brief 设置WiFi图标显示状态
 * 
 * @param obj LLM UI主要组件对象
 * @param img_path 图片路径，如果为NULL则不显示WiFi图标
 */
void lisa_ui_llm_primary_set_wifi_img(lv_obj_t *obj, const void *img_path);

/**
 * @brief 设置电量图标显示
 * 
 * @param obj LLM UI主要组件对象
 * @param img_path 图片路径，如果为NULL则不显示电量图标
 */
void lisa_ui_llm_primary_set_battery_img(lv_obj_t *obj, const void *img_path);

/**
 * @brief 隐藏电池图标
 *
 * 此函数用于隐藏LLM主界面的电池图标。
 *
 * @param obj LLM UI主要组件对象指针
 */
void lisa_ui_llm_primary_battery_icon_hide(lv_obj_t *obj);


/**
 * @brief 设置闹钟图标图片
 *
 * 为任务栏闹钟图标绑定要显示的图片资源。
 *
 * @param obj LLM UI主要组件对象指针
 * @param img_path 指向图片资源的指针（可为打包的 PNG 资源）
 */
void lisa_ui_llm_primary_set_alarm_img(lv_obj_t *obj, const void *img_path);

/**
 * @brief 显示闹钟图标
 *
 * 将任务栏中的闹钟图标设为可见。
 *
 * @param obj LLM UI主要组件对象指针
 */
void lisa_ui_llm_primary_alarm_icon_show(lv_obj_t *obj);

/**
 * @brief 隐藏闹钟图标
 *
 * 将任务栏中的闹钟图标隐藏。
 *
 * @param obj LLM UI主要组件对象指针
 */
void lisa_ui_llm_primary_alarm_icon_hide(lv_obj_t *obj);

/**
 * @brief 设置交互模式图标图片
 *
 * 为任务栏交互模式图标绑定要显示的图片资源。
 *
 * @param obj LLM UI主要组件对象指针
 * @param img_path 指向图片资源的指针（可为打包的 PNG 资源）
 */
void lisa_ui_llm_primary_set_interactive_mode_img(lv_obj_t *obj, const void *img_path);

/**
 * @brief 显示交互模式图标
 *
 * 将任务栏中的交互模式图标设为可见。
 *
 * @param obj LLM UI主要组件对象指针
 */
void lisa_ui_llm_primary_interactive_mode_icon_show(lv_obj_t *obj);

/**
 * @brief 隐藏交互模式图标
 *
 * 将任务栏中的交互模式图标隐藏。
 *
 * @param obj LLM UI主要组件对象指针
 */
void lisa_ui_llm_primary_interactive_mode_icon_hide(lv_obj_t *obj);

/**
 * @brief 显示音乐播放图标
 *
 * 将任务栏中的音乐播放图标设为可见并开始动画。
 *
 * @param obj LLM UI主要组件对象指针
 */
void lisa_ui_llm_primary_music_icon_show(lv_obj_t *obj);

/**
 * @brief 隐藏音乐播放图标
 *
 * 将任务栏中的音乐播放图标隐藏。
 *
 * @param obj LLM UI主要组件对象指针
 */
void lisa_ui_llm_primary_music_icon_hide(lv_obj_t *obj);

/**
 * @brief 启动emoji动画
 * 
 * @param obj LLM UI主要组件对象
 */
void lisa_ui_llm_primary_start_emoji_animation(lv_obj_t *obj);

/**
 * @brief 停止emoji动画
 * 
 * @param obj LLM UI主要组件对象
 */
void lisa_ui_llm_primary_stop_emoji_animation(lv_obj_t *obj);

/**
 * @brief 设置自定义emoji动画图片
 * 
 * @param obj LLM UI主要组件对象
 * @param images 图片数组指针
 * @param images_count 图片数量
 * @param duration 动画时长(毫秒)
 * @param first_frame_delay 第一帧停留时间(毫秒)，0表示无延迟
 */
void lisa_ui_llm_primary_set_custom_emoji_animation(lv_obj_t *obj, const lv_img_dsc_t *images, uint32_t images_count, uint32_t duration, uint32_t first_frame_delay);

/**
 * @brief 设置emoji动画循环参数
 * 
 * @param obj LLM UI主要组件对象
 * @param loop_count 循环次数，0表示无限循环
 */
void lisa_ui_llm_primary_set_loop_count(lv_obj_t *obj, uint32_t loop_count);

/**
 * @brief 显示拍照图片到表情容器
 * 
 * @param obj LLM UI主要组件对象
 * @param rgb565_data RGB565图片数据
 * @param width 图片宽度
 * @param height 图片高度
 */
void lisa_ui_llm_primary_show_camera_image(lv_obj_t *obj, const uint16_t *rgb565_data, uint32_t width, uint32_t height);

/**
 * @brief 显示网络图片到表情容器
 *
 * @param obj LLM UI主要组件对象
 * @param img_dsc 指向 lv_img_dsc_t 的图片描述符
 */
void lisa_ui_llm_primary_show_net_image(lv_obj_t *obj, const lv_img_dsc_t *img_dsc);

/**
 * @brief 隐藏拍照图片
 * 
 * @param obj LLM UI主要组件对象
 */
void lisa_ui_llm_primary_hide_camera_image(lv_obj_t *obj);

/**
 * @brief 隐藏网络图片
 * 
 * @param obj LLM UI主要组件对象
 */
void lisa_ui_llm_primary_hide_net_image(lv_obj_t *obj);

/*===========================================
 * 内联函数
 *==========================================*/

/**
 * @brief 检查对象是否为LLM UI主要组件类型
 * 
 * @param obj 要检查的对象
 * @return true 是LLM UI主要组件类型
 * @return false 不是LLM UI主要组件类型
 */
static inline bool lisa_ui_llm_primary_is_valid(lv_obj_t *obj)
{
    return (obj != NULL) && LISA_UI_LLM_PRIMARY_CLASS_CHECK(obj);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __LISA_UI_LLM_PRIMARY_H__ */
