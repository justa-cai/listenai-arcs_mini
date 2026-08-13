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

lisa_modem_t *modem_usb_at_open(void);
void modem_usb_at_close(lisa_modem_t *modem);

#ifdef __cplusplus
}
#endif
