/**
 * @file bt_audio_adapter.h
 * @brief 蓝牙音频适配层
 * 
 * Copyright (C) ListenAI 2025
 */

#ifndef BT_AUDIO_ADAPTER_H_
#define BT_AUDIO_ADAPTER_H_

#include <stdint.h>
#include "aud_os_task.h"
#include "aud_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 蓝牙Profile类型
 * ======================================================================== */
typedef enum {
    BT_PROFILE_NONE = 0,
    BT_PROFILE_A2DP,                    /* Advanced Audio Distribution Profile */
    BT_PROFILE_HFP,                     /* Hands-Free Profile */
    BT_PROFILE_LEA,                     /* LE Audio */
} bt_audio_profile_e;

/**
 * @brief 蓝牙事件处理结构体
 */
typedef struct {
    void (*bt_event_start)(aud_codec_info_t codec_info);
    void (*bt_event_stop)(uint8_t conidx, uint8_t status);
    void (*bt_event_rcv_data)(const uint8_t *data, size_t size);
    void (*bt_event_pause)(void);
    void (*bt_event_resume)(aud_codec_info_t codec_info);
    void (*bt_audio_send_complete)(void);
} bt_audio_adapter_event_ops_t;

/**
 * @brief 获取os_task_cb_t回调接口
 * 
 * 返回的回调结构体用于注册到aud_os_task，替代aud_if.c
 * 
 * @return os_task_cb_t指针
 */
os_task_cb_t *bt_audio_adapter_get_os_task_cb(void);

/**
 * @brief 音频发送接口
 * @param conidx 连接索引
 * @param data 要发送的数据
 * @param frame_count 帧数
 * @param total_bytes 总字节数
 * 
 * @note 所有蓝牙发送操作都通过此函数，便于统一管理和统计
 */
int bt_audio_adapter_send_frames(uint8_t conidx, const uint8_t *data, 
                                    size_t frame_count, size_t total_bytes);

/**
 * @brief 获取当前蓝牙音频Profile类型
 *
 * 该函数用于获取当前激活的蓝牙音频Profile（如A2DP、HFP等），
 * 便于上层根据Profile类型做不同处理。
 *
 * @return 当前Profile类型，详见bt_audio_profile_e
 */
bt_audio_profile_e bt_audio_adapter_get_profile(void);

/**
 * @brief 启动指定蓝牙音频Profile的音频流
 *
 * 该函数用于启动A2DP/HFP等Profile的音频流，
 * 通常由上层在open时调用，启动后会异步触发bt_event_start回调。
 *
 * @param profile 目标Profile类型（如BT_PROFILE_A2DP/BT_PROFILE_HFP）
 * @return 0 启动成功，非0 启动失败
 */
int bt_audio_adapter_start_audio_stream(bt_audio_profile_e profile);

/**
 * @brief 停止指定蓝牙音频Profile的音频流
 *
 * 该函数用于停止A2DP/HFP等Profile的音频流，
 * 通常由上层在close时调用，停止后会异步触发bt_event_stop回调。
 *
 * @param profile 目标Profile类型（如BT_PROFILE_A2DP/BT_PROFILE_HFP）
 * @return 0 停止成功，非0 停止失败
 */
int bt_audio_adapter_stop_audio_stream(bt_audio_profile_e profile);

/**
 * @brief 注册蓝牙音频事件回调接口
 *
 * 该函数用于注册蓝牙音频适配层的事件回调，
 * 通过传入的ops结构体实现事件通知。
 *
 * @param ops 事件回调结构体指针，不能为空
 * @return 0 注册成功，-1 参数无效
 */
int bt_audio_adapter_registeer(bt_audio_adapter_event_ops_t *ops);

#ifdef __cplusplus
}
#endif

#endif /* BT_AUDIO_ADAPTER_H_ */
