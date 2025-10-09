/*
 * Copyright (c) 2024, LISTENAI
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

// Define any NV3030B specific commands if needed
// Example:
// #define NV3030B_CMD_SOME_CMD 0xAB

extern const struct display_device display_nv3030b;

#ifdef __cplusplus
}
#endif
