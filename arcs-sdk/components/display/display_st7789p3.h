
/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "lisa_display.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ST7789V_CMD_SLEEP_IN  0x10
#define ST7789V_CMD_SLEEP_OUT 0x11
#define ST7789V_CMD_INV_OFF   0x20
#define ST7789V_CMD_INV_ON    0x21
#define ST7789V_CMD_GAMSET    0x26
#define ST7789V_CMD_DISP_OFF  0x28
#define ST7789V_CMD_DISP_ON   0x29

#define ST7789V_CMD_CASET 0x2a
#define ST7789V_CMD_RASET 0x2b
#define ST7789V_CMD_RAMWR 0x2c

extern const struct display_device display_st7789p3;

#ifdef __cplusplus
}
#endif