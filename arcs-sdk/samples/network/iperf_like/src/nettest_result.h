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
    NETTEST_MODE_UPLINK = 0,
    NETTEST_MODE_DOWNLINK,
    NETTEST_MODE_BIDIRECTIONAL,
} nettest_mode_t;

typedef enum {
    NETTEST_STOP_REASON_NONE = 0,
    NETTEST_STOP_REASON_COMPLETE,
    NETTEST_STOP_REASON_NETWORK_LOST,
    NETTEST_STOP_REASON_CONNECT_FAIL,
    NETTEST_STOP_REASON_SEND_FAIL,
    NETTEST_STOP_REASON_RECV_FAIL,
    NETTEST_STOP_REASON_USER_STOP,
} nettest_stop_reason_t;

typedef struct {
    nettest_mode_t mode;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t duration_ms;
    uint32_t error_count;
    nettest_stop_reason_t stop_reason;
} nettest_round_result_t;

const char *nettest_mode_str(nettest_mode_t mode);
int nettest_mode_parse(const char *text, nettest_mode_t *mode);
const char *nettest_stop_reason_str(nettest_stop_reason_t reason);
uint64_t nettest_result_throughput_bps(const nettest_round_result_t *result);
uint64_t nettest_result_tx_throughput_bps(const nettest_round_result_t *result);
uint64_t nettest_result_rx_throughput_bps(const nettest_round_result_t *result);

#ifdef __cplusplus
}
#endif
