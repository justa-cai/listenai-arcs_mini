/**
 * @file bt_sink.h
 * @brief 蓝牙 Sink 场景适配器头文件
 * 
 * Copyright (C) ListenAI 2025
 */

#ifndef BT_SINK_H_
#define BT_SINK_H_

#include "bt_audio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 bt_sink 场景适配器
 * 
 * 初始化完整的 bt_sink 适配层，包括：
 * - session manager
 * - 蓝牙适配器事件回调注册
 * - 音频接口注册
 * 
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_sink_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BT_SINK_H_ */
