/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int net_init(void);

int net_up(void);

int net_down(void);

#ifdef __cplusplus
}
#endif