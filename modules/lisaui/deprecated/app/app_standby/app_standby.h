/**
 * @file app_standby.h
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-01-22
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __APP_STANDBY_H__
#define __APP_STANDBY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "../app_common.h"
#include "lisaui_app_common.h"

/**
 * @brief 交互表情状态
 * 
 */
 typedef enum _lisaui_app_standby_emoji_type_e {
    LISAUI_APP_STANDBY_EMOJI_TYPE_STANDBY = 0,      // 待机
    LISAUI_APP_STANDBY_EMOJI_TYPE_LISTENING,        // 聆听中
    LISAUI_APP_STANDBY_EMOJI_TYPE_RECOGNITION,      // 识别
    LISAUI_APP_STANDBY_EMOJI_TYPE_RESPONSE,         // 响应
    LISAUI_APP_STANDBY_EMOJI_TYPE_NONE,
} lisaui_app_standby_emoji_type_e;


lisaui_err_t app_standby_init(void);
lisaui_err_t lisaui_app_standby_set_emoji(lisaui_app_standby_emoji_type_e type);

#ifdef __cplusplus
}
#endif

#endif // __APP_STANDBY_H__
