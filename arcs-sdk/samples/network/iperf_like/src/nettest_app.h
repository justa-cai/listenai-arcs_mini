/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>

#include "nettest_result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NETTEST_APP_STATE_INIT = 0,
    NETTEST_APP_STATE_WAIT_NETWORK,
    NETTEST_APP_STATE_RUNNING,
    NETTEST_APP_STATE_STOPPED,
} nettest_app_state_t;

typedef struct {
    bool enabled;
    nettest_app_state_t state;
    nettest_mode_t current_mode;
    nettest_mode_t pending_mode;
    uint32_t round_id;
    nettest_round_result_t last_result;
} nettest_app_status_t;

void nettest_app_set_enabled(bool enabled);
bool nettest_app_is_enabled(void);
void nettest_app_set_mode(nettest_mode_t mode);
nettest_mode_t nettest_app_get_mode(void);
void nettest_app_get_status(nettest_app_status_t *status);
const char *nettest_app_state_str(nettest_app_state_t state);

#ifdef __cplusplus
}
#endif
