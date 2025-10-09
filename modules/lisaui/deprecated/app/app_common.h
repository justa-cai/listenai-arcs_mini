/**
 * @file app_common.h
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-02-24
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __APP_COMMON_H__
#define __APP_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lisaui_app_common.h"

enum lisaui_app_id_t {
    UI_APP_ID_NONE = LISAUI_APP_ID_NONE,
    UI_APP_ID_STANDBY,

    UI_APP_ID_ALARM,
    UI_APP_ID_WEATHER,
    UI_APP_ID_WIFI,
    UI_APP_ID_DICTIONARY,
    UI_APP_ID_AUDIO_PLAYER,

    UI_APP_ID_TEMPLATE,
    UI_APP_ID_LAUNCHER,

    UI_APP_ID_TASKBAR,
    UI_APP_ID_SETTING,

    UI_APP_ID_MAX,
};

#ifdef __cplusplus
}
#endif

#endif /* __APP_COMMON_H__ */