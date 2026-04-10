/**
 * @file bt_audio_framework.h
 * @brief 蓝牙音频框架主头文件
 * 
 * Copyright (C) ListenAI 2025
 */

#ifndef BT_AUDIO_FRAMEWORK_H_
#define BT_AUDIO_FRAMEWORK_H_

#include "bt_audio_types.h"
#include "bt_audio_codec.h"
#include "bt_audio_session.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化蓝牙音频框架
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_audio_framework_init(void);

/**
 * @brief 去初始化蓝牙音频框架
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_audio_framework_deinit(void);

/**
 * @brief 获取框架版本
 * @return 版本字符串
 */
const char* bt_audio_framework_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* BT_AUDIO_FRAMEWORK_H_ */
