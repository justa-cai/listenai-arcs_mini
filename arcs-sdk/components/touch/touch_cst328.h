
/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "lisa_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CST328_TOUCH_INFO_REG    (0xD000)
#define CST328_I2C_SLAVE_ADDRESS (0x1D)
#define CST328_STATUS_PRESSED    (0x06)

extern const struct touch_device touch_cst328;

#ifdef __cplusplus
}
#endif