/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int nettest_wifi_init(void);
bool nettest_wifi_is_connected(void);
bool nettest_wifi_is_ready(void);
int nettest_wifi_request_connect(void);
void nettest_wifi_reset_state(void);

#ifdef __cplusplus
}
#endif
