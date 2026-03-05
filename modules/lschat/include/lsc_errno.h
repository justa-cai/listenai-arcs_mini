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

typedef enum {
	LSC_OK = 0,
	LSC_ERR = -1,
	LSC_INVALID_PARAM = -2,
	LSC_INVALID_STATE = -3,
	LSC_NO_MEM = -4,
} lsc_err_e;

#ifdef __cplusplus
}
#endif