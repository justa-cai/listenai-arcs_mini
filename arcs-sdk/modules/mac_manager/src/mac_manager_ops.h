/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "mac_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    mac_manager_mem_ops_t *mem_ops;
    mac_manager_content_ops_t *content_ops;
} mac_manager_ops_t;

mac_manager_ops_t* mac_manager_ops_get(void);

#ifdef __cplusplus
}
#endif

