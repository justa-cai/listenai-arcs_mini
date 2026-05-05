/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nettest_result.h"

#include <stddef.h>
#include <string.h>

const char *nettest_mode_str(nettest_mode_t mode)
{
    switch (mode) {
    case NETTEST_MODE_UPLINK:
        return "uplink";
    case NETTEST_MODE_DOWNLINK:
        return "downlink";
    case NETTEST_MODE_BIDIRECTIONAL:
        return "bidirectional";
    default:
        return "unknown";
    }
}

int nettest_mode_parse(const char *text, nettest_mode_t *mode)
{
    if (text == NULL || mode == NULL) {
        return -1;
    }

    if (strcmp(text, "uplink") == 0) {
        *mode = NETTEST_MODE_UPLINK;
        return 0;
    }
    if (strcmp(text, "downlink") == 0) {
        *mode = NETTEST_MODE_DOWNLINK;
        return 0;
    }
    if (strcmp(text, "bidirectional") == 0) {
        *mode = NETTEST_MODE_BIDIRECTIONAL;
        return 0;
    }

    return -1;
}

const char *nettest_stop_reason_str(nettest_stop_reason_t reason)
{
    switch (reason) {
    case NETTEST_STOP_REASON_NONE:
        return "none";
    case NETTEST_STOP_REASON_COMPLETE:
        return "complete";
    case NETTEST_STOP_REASON_NETWORK_LOST:
        return "network_lost";
    case NETTEST_STOP_REASON_CONNECT_FAIL:
        return "connect_fail";
    case NETTEST_STOP_REASON_SEND_FAIL:
        return "send_fail";
    case NETTEST_STOP_REASON_RECV_FAIL:
        return "recv_fail";
    case NETTEST_STOP_REASON_USER_STOP:
        return "user_stop";
    default:
        return "unknown";
    }
}

uint64_t nettest_result_throughput_bps(const nettest_round_result_t *result)
{
    return nettest_result_tx_throughput_bps(result);
}

uint64_t nettest_result_tx_throughput_bps(const nettest_round_result_t *result)
{
    if (result == NULL || result->duration_ms == 0) {
        return 0;
    }

    return (result->bytes_sent * 8ULL * 1000ULL) / result->duration_ms;
}

uint64_t nettest_result_rx_throughput_bps(const nettest_round_result_t *result)
{
    if (result == NULL || result->duration_ms == 0) {
        return 0;
    }

    return (result->bytes_received * 8ULL * 1000ULL) / result->duration_ms;
}
