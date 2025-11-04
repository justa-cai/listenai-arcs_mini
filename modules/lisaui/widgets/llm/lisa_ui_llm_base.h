/**
 * @file lisa_ui_llm_base.h
 * @brief LLM UI基础组件头文件
 * 
 * 提供LLM聊天界面的基础UI组件，包含任务栏和内容容器的布局管理。
 * 该组件基于LVGL图形库，提供了完整的创建、管理和样式设置API。
 * 
 * @version 1.0.0
 * @date 2025
 * @author Lisa UI Team
 */

#ifndef __LISA_UI_LLM_BASE_H__
#define __LISA_UI_LLM_BASE_H__

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================
 * 包含文件
 *==========================================*/
#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>

/** 默认配置参数 */
#define LISA_UI_LLM_BASE_DEFAULT_BAR_HEIGHT     36      /*!< 默认任务栏高度 */
#define LISA_UI_LLM_BASE_DEFAULT_BAR_COLOR      0x000000  /*!< 默认任务栏颜色 */
#define LISA_UI_LLM_BASE_DEFAULT_BG_COLOR       0x000000  /*!< 默认背景颜色 */
#define LISA_UI_LLM_BASE_MAX_TITLE_LENGTH       16      /*!< 最大标题长度 */

/** 类型检查宏 */
#define LISA_UI_LLM_BASE_CLASS_CHECK(obj) \
    (lv_obj_has_class(obj, &lisa_ui_llm_base_class))

/**
 * @brief LLM UI基础组件结构体
 * 
 * 该结构体定义了LLM UI基础组件的数据成员，包含LVGL对象、
 * 任务栏、内容容器和标题信息。
 */
struct lisa_ui_llm_base {
    lv_obj_t obj;                                           /*!< LVGL基础对象 */
    lv_obj_t *bar;                                         /*!< 任务栏对象 */
    lv_obj_t *container;                                   /*!< 页面容器对象 */
    char title[LISA_UI_LLM_BASE_MAX_TITLE_LENGTH];         /*!< 页面标题 */
};

/** LLM UI基础组件类型定义 */
typedef struct lisa_ui_llm_base lisa_ui_llm_base_t;

/*===========================================
 * 全局变量声明
 *==========================================*/

/** LLM UI基础组件类定义 */
extern const lv_obj_class_t lisa_ui_llm_base_class;

/*===========================================
 * 函数声明
 *==========================================*/

/**
 * @brief 获取任务栏对象
 * 
 * @param obj LLM UI基础组件对象
 * @return lv_obj_t* 任务栏对象，失败返回NULL
 * 
 * @note 返回的对象可用于添加标题标签、按钮等UI元素
 */
lv_obj_t *lisa_ui_llm_base_bar_get(lv_obj_t *obj);

/**
 * @brief 获取内容容器对象
 * 
 * @param obj LLM UI基础组件对象  
 * @return lv_obj_t* 内容容器对象，失败返回NULL
 * 
 * @note 返回的容器对象用于放置聊天消息、输入框等内容
 */
lv_obj_t *lisa_ui_llm_base_container_get(lv_obj_t *obj);

/**
 * @brief 设置页面标题
 * 
 * @param obj LLM UI基础组件对象
 * @param title 标题字符串，如果为NULL则清空标题
 * 
 * @note 标题长度限制为LISA_UI_LLM_BASE_MAX_TITLE_LENGTH-1个字符
 */
void lisa_ui_llm_base_set_title(lv_obj_t *obj, const char *title);

/**
 * @brief 获取页面标题
 * 
 * @param obj LLM UI基础组件对象
 * @return const char* 标题字符串，失败返回NULL
 */
const char *lisa_ui_llm_base_get_title(lv_obj_t *obj);

/**
 * @brief 设置任务栏背景颜色
 * 
 * @param obj LLM UI基础组件对象
 * @param color 颜色值
 * 
 * @code
 * // 设置任务栏为绿色
 * lisa_ui_llm_base_set_bar_color(llm_ui, lv_color_hex(0x4CAF50));
 * @endcode
 */
void lisa_ui_llm_base_set_bar_color(lv_obj_t *obj, lv_color_t color);

/**
 * @brief 设置任务栏高度
 * 
 * @param obj LLM UI基础组件对象
 * @param height 高度值(像素)
 * 
 * @note 高度改变会影响整体布局，建议在组件创建后立即设置
 */
void lisa_ui_llm_base_set_bar_height(lv_obj_t *obj, lv_coord_t height);

/**
 * @brief 清空内容容器
 * 
 * @param obj LLM UI基础组件对象
 * 
 * @note 该函数会删除容器中的所有子对象
 */
void lisa_ui_llm_base_clear_container(lv_obj_t *obj);

/*===========================================
 * 内联函数
 *==========================================*/

/**
 * @brief 检查对象是否为LLM UI基础组件类型
 * 
 * @param obj 要检查的对象
 * @return true 是LLM UI基础组件类型
 * @return false 不是LLM UI基础组件类型
 */
static inline bool lisa_ui_llm_base_is_valid(lv_obj_t *obj)
{
    if (obj == NULL) {
        return false;
    }
    
    // 检查是否为基类或其派生类
    return LISA_UI_LLM_BASE_CLASS_CHECK(obj);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __LISA_UI_LLM_BASE_H__ */
