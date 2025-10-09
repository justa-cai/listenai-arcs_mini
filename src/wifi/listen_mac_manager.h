/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if CONFIG_MAC_MANAGER
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int listen_mac_manager_init(void);

uint8_t* listen_mac_manager_get(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MAC_MANAGER
