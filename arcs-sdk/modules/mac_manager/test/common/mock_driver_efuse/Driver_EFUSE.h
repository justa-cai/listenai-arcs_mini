/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void efuse_init(void);

uint64_t efuse_read_uuid(void);

#ifdef __cplusplus
}
#endif