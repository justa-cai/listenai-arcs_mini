/**
 * @file app_taskbar.h
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-02-25
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LISAUI_APP_TASKBAR_H__
#define __LISAUI_APP_TASKBAR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lisaui_app_common.h"

#define CONFIG_LISAUI_APP_TASKBAR_TOOLKIT_ENABLE 1

lisaui_err_t app_taskbar_init(void);

lisaui_err_t lisaui_taskbar_hide(bool hide);
lisaui_err_t lisaui_get_taskbar_dbus(lisaui_dbus_t **dbus);
lisaui_err_t lisaui_taskbar_set_battery_percent(int percent);
lisaui_err_t lisaui_taskbar_set_battery_charging(bool is_charging);

typedef enum _lisaui_taskbar_event_e {
    LISAUI_TASKBAR_EVENT_BACK,
    LISAUI_TASKBAR_EVENT_CLOSE,
    LISAUI_TASKBAR_EVENT_HOME,

    LISAUI_TASKBAR_EVENT_ENTER_SETTING,         // 进入设置页面
    LISAUI_TASKBAR_EVENT_ENTER_ALARM,
    LISAUI_TASKBAR_EVENT_ENTER_AUDIO_PLAYER,
    LISAUI_TASKBAR_EVENT_ENTER_WEATHER,
    LISAUI_TASKBAR_EVENT_ENTER_STANDBY,
    LISAUI_TASKBAR_EVENT_ENTER_WIFI,

    LISAUI_TASKBAR_EVENT_GESTURE_UP,
    LISAUI_TASKBAR_EVENT_GESTURE_DOWN,
    LISAUI_TASKBAR_EVENT_GESTURE_LEFT,
    LISAUI_TASKBAR_EVENT_GESTURE_RIGHT,
    LISAUI_TASKBAR_EVENT_GESTURE_CLICK,
    LISAUI_TASKBAR_EVENT_GESTURE_LONG_PRESS,
} lisaui_taskbar_event_t;

typedef void (*lisaui_taskbar_event_handler_t)(lisaui_taskbar_event_t event, void *param);

lisaui_err_t lisaui_taskbar_register_event_handler(lisaui_taskbar_event_handler_t handler);
lisaui_err_t lisaui_taskbar_unregister_event_handler(void);

#define LISAUI_TASKBAR_EVENT_HANDLER(handler, event, param) \
    do { \
        if (handler) { \
            handler(event, param); \
        } \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* __LISAUI_APP_TASKBAR_H__ */
