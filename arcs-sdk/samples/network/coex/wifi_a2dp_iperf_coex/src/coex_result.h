/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COEX_MODE_UPLINK = 0,
    COEX_MODE_DOWNLINK,
    COEX_MODE_BIDIRECTIONAL,
} coex_mode_t;

typedef enum {
    COEX_STOP_REASON_NONE = 0,
    COEX_STOP_REASON_COMPLETE,
    COEX_STOP_REASON_NETWORK_LOST,
    COEX_STOP_REASON_CONNECT_FAIL,
    COEX_STOP_REASON_SEND_FAIL,
    COEX_STOP_REASON_RECV_FAIL,
    COEX_STOP_REASON_USER_STOP,
} coex_stop_reason_t;

typedef struct {
    coex_mode_t mode;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t duration_ms;
    uint32_t error_count;
    coex_stop_reason_t stop_reason;
} coex_round_result_t;

const char *coex_mode_str(coex_mode_t mode);
int coex_mode_parse(const char *text, coex_mode_t *mode);
const char *coex_stop_reason_str(coex_stop_reason_t reason);
uint64_t coex_result_throughput_bps(const coex_round_result_t *result);
uint64_t coex_result_tx_throughput_bps(const coex_round_result_t *result);
uint64_t coex_result_rx_throughput_bps(const coex_round_result_t *result);

#ifdef __cplusplus
}
#endif
