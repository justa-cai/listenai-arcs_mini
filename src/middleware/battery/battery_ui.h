/*
 * @file battery_ui.h
 * @brief Battery UI layer - smoothing, debounce, and UI event publishing
 * @version 0.1
 * @date 2025-04-16
 *
 * @copyright Copyright (C) 2025 ANHUI LISTENAI Co., Ltd. All Rights Reserved.
 */

#ifndef __BATTERY_UI_H__
#define __BATTERY_UI_H__

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdbool.h>

#include "voice_msg_structure.h"

/**
 * @brief 初始化电池 UI 层
 *
 * 创建并启动 1s 采样定时器，定时读取原始电量/状态，
 * 经过平滑滤波后通过 VOICE_MSG_POWER_BATTERY_UPDATE 发布给 UI。
 * 必须在 battery_init() 之后调用。
 */
void battery_ui_init(void);

/**
 * @brief 获取当前 UI 显示使用的电池信息
 *
 * @param info 输出参数
 * @return true 获取成功
 * @return false 参数无效
 */
bool battery_ui_get_info(voice_msg_battery_info_t *info);

#if defined(__cplusplus)
}
#endif

#endif /* __BATTERY_UI_H__ */
