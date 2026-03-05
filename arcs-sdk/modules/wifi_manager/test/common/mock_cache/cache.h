/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_DCACHE_CFG_LINE_SIZE 32

DECLARE_FAKE_VOID_FUNC(HAL_InvalidateDCache_by_Addr, uint32_t *, uint32_t);

#ifdef __cplusplus
}
#endif
