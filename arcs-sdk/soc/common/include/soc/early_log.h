/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

void soc_early_log_init(void);
int soc_early_log_is_ready(void);
void soc_early_log_write(const char *msg);
void soc_early_log_vprintf(const char *fmt, va_list ap);

#ifdef __cplusplus
}
#endif
