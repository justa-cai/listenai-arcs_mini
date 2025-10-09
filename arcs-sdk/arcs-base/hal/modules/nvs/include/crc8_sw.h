/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t crc8_ccitt(uint8_t val, const void *buf, size_t cnt);


#ifdef __cplusplus
}
#endif

