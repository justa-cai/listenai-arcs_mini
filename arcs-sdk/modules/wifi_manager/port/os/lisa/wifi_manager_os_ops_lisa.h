/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "wifi_manager/wifi_manager_os_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取 lisa 系统操作接口
 * 
 * @return wifi_manager_os_ops_t* 系统操作接口指针
 */
wifi_manager_os_ops_t* wifi_manager_lisa_os_ops_get(void);

#ifdef __cplusplus
}
#endif 