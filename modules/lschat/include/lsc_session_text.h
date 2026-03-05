/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include "lsc.h"
#include "lsc_sessions_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup session_text 文本会话
 *
 * @{
 */

/**
 * @brief 定义会话事件类型
 *
 *
 */
typedef enum {
	/** 语音文本结果 */
	SESSION_TEXT_REPLY_URL = BIT(0),
	/** 语义TTS结果 */
	SESSION_TEXT_TTS_URL = BIT(1),
	SESSION_TEXT_MAX = BIT(31),
} session_text_event_e;

/**
 * @typedef session_text_event_cb_t
 * 定义回调函数类型，不同事件类型对应的data数据类型不同。
 * 事件类型和对应的data数据类型如下：
 *
 * 事件类型                   | data类型
 * --------------------------|-----------------
 * SESSION_TEXT_TTS_URL          | char*
 * SESSION_TEXT_REPLY_URL    | char*
 *
 * @param evt 事件类型
 * @param data 事件数据
 * @param size 事件数据长度
 * @param usr 用户参数
 */
typedef void (*session_text_event_cb_t)(session_text_event_e evt, void *data, uint32_t size, void *usr);

/**
 * @brief 定义初始化参数类型
 *
 */
typedef struct {
	/** 会话超时时间 */
	int session_timeout_ms;
} session_text_config_t;

/**
 * @brief 会话初始化
 *
 * @retval 0： 成功
 */
int session_text_init(void);

/**
 * @brief 会话逆初始化
 *
 * @retval 0： 成功
 */
int session_text_deinit(void);

/**
 * @brief 参数设置
 *
 * @param cfg 配置参数
 *
 * @retval 0： 设置成功
 */
int session_text_set_config(session_text_config_t *cfg);

/**
 * @brief 获取设置参数
 *
 * @retval cfg： 配置参数
 */
int session_text_get_config(session_text_config_t *cfg);

/**
 * @brief 添加事件回调函数
 * 支持多次添加回调函数
 *
 * @param cb 回调函数
 * @param evt 监听的事件，参考@see session_text_event_e
 * @param usr 用户参数
 *
 * @retval 0： 成功
 */
int session_text_add_evt_callback(session_text_event_cb_t cb, session_text_event_e evt, void *usr);

/**
 * @brief 移除已注册的事件回调函数
 *
 * @param cb 已注册的回调函数
 *
 * @retval 0： 成功
 */
int session_text_remove_evt_callback(session_text_event_cb_t cb);

/**
 * @brief 中止会话
 *
 * @retval 0： 成功
 */
int session_text_cancel(void);

/**
 * @brief 发起文本会话
 * 该会话用于通过文本和云端进行交互，云端进行语义识别后，通过回调函数返回结果。
 *
 * @note 该接口是同步接口
 *
 * @param txt 文本数据
 *
 * @retval 0： 成功
 */
int session_text_send(char *txt);

/**
 * @brief 发起在线语音合成
 *
 * @note 该接口是同步接口
 *
 * @param txt 文本数据
 *
 * @retval 0： 成功
 */
int session_text_tts_synth(const char *txt);
/**
 * @}
 */

#ifdef __cplusplus
}
#endif