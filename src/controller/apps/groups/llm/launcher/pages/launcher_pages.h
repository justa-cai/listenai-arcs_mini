/**
 * 
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LISAUI_LAUNCHER_PAGES_H__
#define __LISAUI_LAUNCHER_PAGES_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum{
    LAUNCHER_PAGE_MAIN_UPDATE_ADD_ICON = 0,
}launcher_page_update_event_t;

typedef struct{
    launcher_page_update_event_t event;
    union{
        struct {
            void* group;
        }update_icon;

    };

}launcher_page_update_parameter_t;

/**
 * @brief 从云端设置待机文本
 * 
 * 此函数用于设置云端下发的待机文本配置。当云端下发新的待机文本时，
 * 调用此函数来更新配置，主页面将自动使用新的文本进行轮换显示。
 * 
 * @param texts 文本数组指针，每个元素为一个待机文本字符串
 * @param count 文本数量，最大支持10个文本
 * @param interval_ms 轮换间隔（毫秒），建议3000-10000毫秒
 * @return 0表示成功，负数表示失败
 * 
 * 使用示例：
 * @code
 * const char *cloud_texts[] = {
 *     "欢迎使用智能助手",
 *     "我在这里等待您的指令",
 *     "有什么可以帮助您的吗？"
 * };
 * lisaui_primary_page_set_cloud_standby_texts(cloud_texts, 3, 5000);
 * @endcode
 */
int lisaui_primary_page_set_cloud_standby_texts(const char **texts, uint32_t count, uint32_t interval_ms);

/**
 * @brief 测试云端待机文本功能
 * 
 * 此函数提供一个测试接口，用于验证云端待机文本功能是否正常工作。
 * 调用此函数会设置一组测试文本并启用轮换功能。
 */
void lisaui_primary_page_test_cloud_standby_texts(void);

/**
 * @brief Check if primary page is currently active
 * @return true if primary page is active, false otherwise
 */
bool is_primary_page_active(void);

/**
 * @brief Get primary page UI object
 * @return UI object pointer if page is active, NULL otherwise
 */
struct _lv_obj_t *page_primary_get_ui_object(void);

/**
 * @brief 停止当前正在运行的分阶段表情动画
 * @return 0表示成功，-1表示失败（主页面视图不存在）
 * @note 如果主页面视图不存在，函数会直接返回-1
 * @note 此函数会将动画状态重置为EMOJI_ANIM_STAGE_IDLE，并清除强制退出标志
 */
int current_staged_emoji_animation_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* __LISAUI_LAUNCHER_PAGES_H__ */
