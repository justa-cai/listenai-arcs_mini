
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

#define CST816D_REG_CHIP (0xa7)
#define CST816D_CHIP_ID  (0xb6)

extern const struct touch_device touch_cst816d;

#ifdef __cplusplus
}
#endif