/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include "lsc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup session_request 服务请求会话
 * @{
 */

/**
 * @brief 会话初始化
 *
 * @return 0： 成功
 */
int session_request_init(void);

/**
 * @brief 会话逆初始化
 *
 * @return 0： 成功
 */
int session_request_deinit(void);

/**
 * @brief 中止请求会话
 * @note 该接口是异步接口，不会等待当前请求完成，立即返回。
 *
 * @retval 0： 成功
 */
int session_request_cancel(void);

/**
 * @brief 请求语音合成
 *
 * 用于请求云端语音合成服务，将文本数据转换为语音，并下发到指定的url地址。
 *
 * @note 该接口是同步接口，当获取结果或者超时时返回。
 *
 * @param txt 需合成的文本
 * @param timeout_ms  请求超时时间
 * @param[out] url 返回合成的语音url
 *
 * @retval 0： 成功
 */
int session_request_xtts(char *txt, char url[256], uint32_t timeout_ms);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif