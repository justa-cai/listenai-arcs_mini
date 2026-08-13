/**
 * @file bt_sink.h
 * @brief 蓝牙 Sink 场景适配器头文件
 * 
 * Copyright (C) ListenAI 2025
 */

#ifndef BT_SINK_H_
#define BT_SINK_H_

#include "bt_audio_types.h"
#include "interfaces/bt_audio_interface.h"

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

/**
 * @brief 设置 bt_sink 使用的音频硬件接口
 *
 * 若应用不调用该接口，bt_sink_init() 仍会使用 bt_audio_interface_get_default()
 * 获取默认接口；若调用该接口，需要在播放流空闲时设置。
 *
 * @param ops 音频接口操作表
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_sink_set_audio_interface(const bt_audio_interface_ops_t *ops);

#ifdef __cplusplus
}
#endif

#endif /* BT_SINK_H_ */
