/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t crc32_sw(uint32_t val, const uint8_t *buf, size_t len);


#ifdef __cplusplus
}
#endif
