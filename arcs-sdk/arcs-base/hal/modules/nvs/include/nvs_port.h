/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "crc8_sw.h"
#include "log_print.h"

#ifdef __cplusplus
extern "C" {
#endif

#define k_mutex_init(mutex) (0)
#define k_mutex_lock(mutex, timeout) (0)
#define k_mutex_unlock(mutex) (0)


#define LOG_ERR(...)  CLOGE(__VA_ARGS__)
#define LOG_DBG(...)  CLOGD(__VA_ARGS__)
#define LOG_INF(...)  CLOGI(__VA_ARGS__)

#ifdef __cplusplus
}
#endif

