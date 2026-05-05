/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "shell.h"

#include "nettest_app.h"
#include "nettest_config.h"

static void shell_print_status(Shell *shell)
{
    nettest_app_status_t status;
    uint64_t tx_bps;
    uint64_t rx_bps;

    nettest_app_get_status(&status);
    tx_bps = nettest_result_tx_throughput_bps(&status.last_result);
    rx_bps = nettest_result_rx_throughput_bps(&status.last_result);

    shellPrint(shell, "enabled: %s\n", status.enabled ? "yes" : "no");
    shellPrint(shell, "state: %s\n", nettest_app_state_str(status.state));
    shellPrint(shell, "mode: iperf3\n");
    shellPrint(shell, "current_test_mode: %s\n", nettest_mode_str(status.current_mode));
    shellPrint(shell, "pending_test_mode: %s\n", nettest_mode_str(status.pending_mode));
    shellPrint(shell, "server: %s:%d\n", NETTEST_SERVER_IP, NETTEST_SERVER_PORT);
    shellPrint(shell, "round_seconds: %d\n", NETTEST_ROUND_SECONDS);
    shellPrint(shell, "send_block_size: %d\n", NETTEST_SEND_BLOCK_SIZE);
    shellPrint(shell, "round_id: %u\n", status.round_id);
    shellPrint(shell, "last_bytes_sent: %llu\n", status.last_result.bytes_sent);
    shellPrint(shell, "last_bytes_received: %llu\n", status.last_result.bytes_received);
    shellPrint(shell, "last_duration_ms: %u\n", status.last_result.duration_ms);
    shellPrint(shell, "last_error_count: %u\n", status.last_result.error_count);
    shellPrint(shell, "last_stop_reason: %s\n", nettest_stop_reason_str(status.last_result.stop_reason));
    shellPrint(shell, "last_tx_throughput_bps: %llu\n", tx_bps);
    shellPrint(shell, "last_rx_throughput_bps: %llu\n", rx_bps);
}

static int cmd_iperf(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();
    const char *subcmd;
    int arg_base = 0;

    if (argc < 1) {
        shellPrint(shell, "Usage: iperf <start|stop|status|mode|help>\n");
        return -1;
    }

    subcmd = argv[0];
    if (strcmp(subcmd, "iperf") == 0) {
        if (argc < 2) {
            shellPrint(shell, "Usage: iperf <start|stop|status|mode|help>\n");
            return -1;
        }
        subcmd = argv[1];
        arg_base = 1;
    }

    if (strcmp(subcmd, "start") == 0) {
        nettest_app_set_enabled(true);
        shellPrint(shell, "iperf loop enabled\n");
        return 0;
    }

    if (strcmp(subcmd, "stop") == 0) {
        nettest_app_set_enabled(false);
        shellPrint(shell, "iperf loop disabled\n");
        return 0;
    }

    if (strcmp(subcmd, "status") == 0) {
        shell_print_status(shell);
        return 0;
    }

    if (strcmp(subcmd, "mode") == 0) {
        nettest_mode_t mode;
        int mode_index = arg_base + 1;

        if (argc <= mode_index) {
            shellPrint(shell, "current mode: %s\n", nettest_mode_str(nettest_app_get_mode()));
            shellPrint(shell, "set mode: iperf mode uplink|downlink|bidirectional\n");
            return 0;
        }

        const char *mode_arg = argv[mode_index];
        if (nettest_mode_parse(mode_arg, &mode) != 0) {
            shellPrint(shell, "Unknown mode: %s\n", mode_arg);
            shellPrint(shell, "Usage: iperf mode uplink|downlink|bidirectional\n");
            return -1;
        }

        nettest_app_set_mode(mode);
        shellPrint(shell, "iperf mode %s will apply on next round\n", nettest_mode_str(mode));
        return 0;
    }

    if (strcmp(subcmd, "help") == 0) {
        shellPrint(shell, "iperf start  - enable auto loop\n");
        shellPrint(shell, "iperf stop   - stop current round and disable auto loop\n");
        shellPrint(shell, "iperf status - show current status\n");
        shellPrint(shell, "iperf mode uplink|downlink|bidirectional - set next round mode\n");
        shellPrint(shell, "iperf help   - show this message\n");
        return 0;
    }

    shellPrint(shell, "Unknown subcommand: %s\n", subcmd);
    shellPrint(shell, "Usage: iperf <start|stop|status|mode|help>\n");
    return -1;
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    iperf,
    cmd_iperf,
    iperf-like network throughput controls
);
