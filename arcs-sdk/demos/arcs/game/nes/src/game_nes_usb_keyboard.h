/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#include "autoconf.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_GAME_NES_USB_HOST_KEYBOARD
void game_nes_usb_keyboard_init(void);
uint16_t game_nes_usb_keyboard_get_state(void);
#else
static inline void game_nes_usb_keyboard_init(void)
{
}

static inline uint16_t game_nes_usb_keyboard_get_state(void)
{
    return 0;
}
#endif

#ifdef __cplusplus
}
#endif
