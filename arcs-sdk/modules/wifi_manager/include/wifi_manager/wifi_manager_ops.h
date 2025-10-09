/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "wifi_manager/wifi_manager_mem_ops.h"
#include "wifi_manager/wifi_manager_os_ops.h"
#include "wifi_manager/wifi_manager_wifi_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    wifi_manager_os_ops_t *os_ops;
    wifi_manager_mem_ops_t *mem_ops;
    wifi_manager_wifi_ops_t *wifi_ops;
} wifi_mgr_ops_t;

wifi_mgr_ops_t* wifi_mgr_ops_get(void);

#ifdef __cplusplus
}
#endif
