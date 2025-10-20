/**
 * @file button.h
 * @brief
 * @version 0.1
 * @date 2025-04-16
 *
 * @copyright Copyright (C) 2025 ANHUI LISTENAI Co., Ltd. All Rights Reserved.
 */

#ifndef __BUTTON_H__
#define __BUTTON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    LISA_BTN_PRESS_DOWN = 0,
    LISA_BTN_PRESS_CLICK,
    LISA_BTN_PRESS_DOUBLE_CLICK,
    LISA_BTN_PRESS_TRIPLE_CLICK,
    LISA_BTN_PRESS_QUADRUPLE_CLICK,
    LISA_BTN_PRESS_QUINTUPLE_CLICK,
    LISA_BTN_PRESS_REPEAT_CLICK,
    LISA_BTN_PRESS_SHORT_START,
    LISA_BTN_PRESS_SHORT_UP,
    LISA_BTN_PRESS_LONG_START,
    LISA_BTN_PRESS_LONG_UP,
    LISA_BTN_PRESS_LONG_HOLD,
    LISA_BTN_PRESS_LONG_HOLD_UP,
    LISA_BTN_PRESS_MAX,
    LISA_BTN_PRESS_NONE,
} lisa_btn_event_t;

typedef enum {
    LISA_BTN_ID_POWER = 0,
    LISA_BTN_ID_MAX,
} lisa_btn_id_t;

typedef void (*lisa_btn_cb_t)(lisa_btn_event_t evt, void *user);

void lisa_btn_init(lisa_btn_cb_t cb, void *user);

#ifdef __cplusplus
}
#endif

#endif