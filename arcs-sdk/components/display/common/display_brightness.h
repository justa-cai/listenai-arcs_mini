
/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "lisa_display.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define DISP_PWM_DEV()      GPT0_PWM()
// #define DISP_PWM_CHANNEL    GPT_CHANNEL5
// #define DISP_GPIO_PWM_PIN        13
#define DRV_PWM_CLK_HZ           1000
void disp_comm_brightness_init(struct blacklight_config *config);

/**
 * @brief Set display brightness
 *
 * @param value Brightness value (0-100)
 */
void disp_comm_brightness_set(uint8_t value);
 
#ifdef __cplusplus
}
#endif