/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "coex_result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COEX_IPERF_STATE_INIT = 0,
    COEX_IPERF_STATE_WAIT_NETWORK,
    COEX_IPERF_STATE_RUNNING,
    COEX_IPERF_STATE_STOPPED,
} coex_iperf_state_t;

typedef struct {
    bool initialized;
    bool enabled;
    coex_iperf_state_t state;
    coex_mode_t current_mode;
    coex_mode_t pending_mode;
    uint32_t round_id;
    coex_round_result_t last_result;
} coex_iperf_status_t;

int coex_iperf_init(void);
void coex_iperf_set_enabled(bool enabled);
bool coex_iperf_is_enabled(void);
void coex_iperf_set_mode(coex_mode_t mode);
coex_mode_t coex_iperf_get_mode(void);
int coex_iperf_set_server(const char *ip, int port);
void coex_iperf_get_server(char *ip, size_t ip_len, int *port);
void coex_iperf_get_status(coex_iperf_status_t *status);
const char *coex_iperf_state_str(coex_iperf_state_t state);

int coex_runner_iperf3_run(coex_mode_t mode,
                           volatile bool *enabled,
                           coex_round_result_t *result);
void coex_runner_iperf3_abort(void);

#ifdef __cplusplus
}
#endif
