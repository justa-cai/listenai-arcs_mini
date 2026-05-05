/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "lisa_modem_module.h"

#ifdef __cplusplus
extern "C" {
#endif

lisa_modem_t *sample_modem_open(const char *uart_dev);
void sample_modem_close(lisa_modem_t *modem);
const char *sample_modem_backend_name(void);

#ifdef __cplusplus
}
#endif
