/**
 * @file app_setting.h
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-01-22
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __APP_SETTING_H__
#define __APP_SETTING_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lisaui_app_common.h"

#if CONFIG_LVGL_ENV_SIMULATOR
#define LISAUI_APP_SETTING_UI_RES_PERFIX_PATH(path) "app/llm/lisaui/app/app_setting/" path
#else
// #define LISAUI_APP_SETTING_UI_RES_PERFIX_PATH(path) "src/ui/lisaui/app/app_setting/" path
// #define LISAUI_APP_SETTING_UI_RES_PERFIX_PATH(path) "src/ui/lisaui/app/app_setting/" path
#define LISAUI_APP_SETTING_UI_RES_PERFIX_PATH(path) "source/view/lisaui/app/app_setting/" path
#endif

#define LISAUI_APP_SETTING_ITEM_ID_REGULAR      0
#define LISAUI_APP_SETTING_ITEM_ID_ALARM        1
#define LISAUI_APP_SETTING_ITEM_ID_WAKE         2
#define LISAUI_APP_SETTING_ITEM_ID_WIFI         3
#define LISAUI_APP_SETTING_ITEM_ID_WEATHER      4
#define LISAUI_APP_SETTING_ITEM_ID_AUDIO_PLAYER 5

#define LISAUI_APP_SETTING_ITEM_BACKLIGHT_BRIGHTNESS_MAX_VALUE (100)
#define LISAUI_APP_SETTING_ITEM_BACKLIGHT_BRIGHTNESS_MIN_VALUE (0)
#define LISAUI_APP_SETTING_ITEM_VOLUME_MAX_VALUE               (10)
#define LISAUI_APP_SETTING_ITEM_VOLUME_MIN_VALUE               (0)

enum lisaui_app_audio_player_state {
    LISAUI_APP_AUDIO_PLAY_STATE_IDLE,
    LISAUI_APP_AUDIO_PLAY_STATE_PLAYING,
    LISAUI_APP_AUDIO_PLAY_STATE_PAUSED,
};

enum lisaui_app_audio_player_action {
    LISAUI_APP_AUDIO_PLAY_ACTION_PLAY,
    LISAUI_APP_AUDIO_PLAY_ACTION_PAUSE,
    LISAUI_APP_AUDIO_PLAY_ACTION_RESUME,
    LISAUI_APP_AUDIO_PLAY_ACTION_STOP,
};

lisaui_err_t app_setting_init(void);
lisaui_err_t app_setting_del_item_page(void);

#ifdef __cplusplus
}
#endif

#endif // __APP_SETTING_H__
