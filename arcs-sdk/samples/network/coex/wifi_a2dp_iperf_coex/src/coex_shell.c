/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lisa_mem.h"
#include "shell.h"

#include "ls_wf_coex.h"

#include "coex_app.h"
#include "coex_bt.h"
#include "coex_bt_audio.h"
#include "coex_iperf.h"
#include "coex_shell.h"
#include "coex_wifi.h"

static void coex_shell_print_ip(Shell *shell, uint32_t ip)
{
    shellPrint(shell, "%u.%u.%u.%u",
               ip & 0xff,
               (ip >> 8) & 0xff,
               (ip >> 16) & 0xff,
               (ip >> 24) & 0xff);
}

static void coex_shell_print_wifi_status(Shell *shell)
{
    coex_wifi_status_t status;

    coex_wifi_get_status(&status);
    shellPrint(shell, "initialized: %s\n", status.initialized ? "yes" : "no");
    shellPrint(shell, "stack_ready: %s\n", status.stack_ready ? "yes" : "no");
    shellPrint(shell, "connecting: %s\n", status.connecting ? "yes" : "no");
    shellPrint(shell, "connected: %s\n", status.connected ? "yes" : "no");
    shellPrint(shell, "dhcp_ready: %s\n", status.ready ? "yes" : "no");
    shellPrint(shell, "ip: ");
    coex_shell_print_ip(shell, status.ip_addr);
    shellPrint(shell, "\n");
}

static void coex_shell_print_iperf_status(Shell *shell)
{
    coex_iperf_status_t status;
    char server_ip[16];
    int server_port;
    uint64_t tx_bps;
    uint64_t rx_bps;

    coex_iperf_get_status(&status);
    coex_iperf_get_server(server_ip, sizeof(server_ip), &server_port);
    tx_bps = coex_result_tx_throughput_bps(&status.last_result);
    rx_bps = coex_result_rx_throughput_bps(&status.last_result);

    shellPrint(shell, "initialized: %s\n", status.initialized ? "yes" : "no");
    shellPrint(shell, "enabled: %s\n", status.enabled ? "yes" : "no");
    shellPrint(shell, "state: %s\n", coex_iperf_state_str(status.state));
    shellPrint(shell, "current_test_mode: %s\n", coex_mode_str(status.current_mode));
    shellPrint(shell, "pending_test_mode: %s\n", coex_mode_str(status.pending_mode));
    shellPrint(shell, "server: %s:%d\n", server_ip, server_port);
    shellPrint(shell, "round_seconds: %d\n", COEX_ROUND_SECONDS);
    shellPrint(shell, "send_block_size: %d\n", COEX_SEND_BLOCK_SIZE);
    shellPrint(shell, "round_id: %u\n", status.round_id);
    shellPrint(shell, "last_bytes_sent: %llu\n", status.last_result.bytes_sent);
    shellPrint(shell, "last_bytes_received: %llu\n", status.last_result.bytes_received);
    shellPrint(shell, "last_duration_ms: %u\n", status.last_result.duration_ms);
    shellPrint(shell, "last_error_count: %u\n", status.last_result.error_count);
    shellPrint(shell, "last_stop_reason: %s\n", coex_stop_reason_str(status.last_result.stop_reason));
    shellPrint(shell, "last_tx_throughput_bps: %llu\n", tx_bps);
    shellPrint(shell, "last_rx_throughput_bps: %llu\n", rx_bps);
}

static void coex_shell_print_bt_status(Shell *shell)
{
    coex_bt_status_t bt_status;
    coex_bt_audio_status_t audio_status;

    coex_bt_get_status(&bt_status);
    coex_bt_audio_get_status(&audio_status);

    shellPrint(shell, "initialized: %s\n", bt_status.initialized ? "yes" : "no");
    shellPrint(shell, "opened: %s\n", bt_status.opened ? "yes" : "no");
    shellPrint(shell, "discovery_registered: %s\n", bt_status.discovery_registered ? "yes" : "no");
    shellPrint(shell, "discovered_devices: %u\n", bt_status.discovered_count);
    shellPrint(shell, "acl_connected: %s\n", bt_status.connected ? "yes" : "no");
    shellPrint(shell, "a2dp_connected: %s\n", bt_status.a2dp_connected ? "yes" : "no");
    shellPrint(shell, "peer_addr: %s\n", bt_status.peer_addr[0] ? bt_status.peer_addr : "n/a");
    shellPrint(shell, "audio_initialized: %s\n", audio_status.initialized ? "yes" : "no");
    shellPrint(shell, "audio_profile_open: %s\n", audio_status.profile_open ? "yes" : "no");
    shellPrint(shell, "audio_streaming: %s\n", audio_status.streaming ? "yes" : "no");
    shellPrint(shell, "audio_volume: %u\n", audio_status.volume);
}

static int coex_shell_parse_uint(const char *arg, unsigned int *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (arg == NULL || value == NULL) {
        return -1;
    }

    errno = 0;
    parsed = strtoul(arg, &end, 0);
    if (errno != 0 || end == arg || *end != '\0' || parsed > UINT_MAX) {
        return -1;
    }

    *value = (unsigned int)parsed;
    return 0;
}

static int cmd_wifi(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();

    if (argc < 2) {
        shellPrint(shell, "Usage: wifi <connect [ssid pwd]|disconnect|status|help>\n");
        return -1;
    }

    if (strcmp(argv[1], "connect") == 0) {
        int ret;

        if (argc == 2) {
            ret = coex_wifi_connect();
        } else if (argc == 4) {
            ret = coex_wifi_connect_to(argv[2], argv[3]);
        } else {
            shellPrint(shell, "Usage: wifi connect [ssid pwd]\n");
            return -1;
        }

        shellPrint(shell, "wifi connect ret=%d\n", ret);
        return ret;
    }

    if (strcmp(argv[1], "disconnect") == 0) {
        int ret = coex_wifi_disconnect();
        shellPrint(shell, "wifi disconnect ret=%d\n", ret);
        return ret;
    }

    if (strcmp(argv[1], "status") == 0) {
        coex_shell_print_wifi_status(shell);
        return 0;
    }

    shellPrint(shell, "Usage: wifi <connect [ssid pwd]|disconnect|status|help>\n");
    return -1;
}

static int cmd_iperf(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();
    const char *subcmd;
    int arg_base = 0;

    if (argc < 1) {
        shellPrint(shell, "Usage: iperf <start|stop|status|mode|server|help>\n");
        return -1;
    }

    subcmd = argv[0];
    if (strcmp(subcmd, "iperf") == 0) {
        if (argc < 2) {
            shellPrint(shell, "Usage: iperf <start|stop|status|mode|server|help>\n");
            return -1;
        }
        subcmd = argv[1];
        arg_base = 1;
    }

    if (strcmp(subcmd, "start") == 0) {
        coex_iperf_set_enabled(true);
        shellPrint(shell, "iperf loop enabled\n");
        if (!coex_wifi_is_ready()) {
            shellPrint(shell, "wifi not ready, run 'wifi connect' first\n");
        }
        return 0;
    }

    if (strcmp(subcmd, "stop") == 0) {
        coex_iperf_set_enabled(false);
        shellPrint(shell, "iperf loop disabled\n");
        return 0;
    }

    if (strcmp(subcmd, "status") == 0) {
        coex_shell_print_iperf_status(shell);
        return 0;
    }

    if (strcmp(subcmd, "server") == 0) {
        char server_ip[16];
        int server_port;
        int server_index = arg_base + 1;

        coex_iperf_get_server(server_ip, sizeof(server_ip), &server_port);
        if (argc <= server_index) {
            shellPrint(shell, "current server: %s:%d\n", server_ip, server_port);
            shellPrint(shell, "set server: iperf server <ip> [port]\n");
            return 0;
        }

        if (argc > server_index + 2) {
            shellPrint(shell, "Usage: iperf server <ip> [port]\n");
            return -1;
        }

        if (argc > server_index + 1) {
            unsigned int parsed_port;

            if (coex_shell_parse_uint(argv[server_index + 1], &parsed_port) != 0 ||
                parsed_port == 0 || parsed_port > 65535U) {
                shellPrint(shell, "Invalid port: %s\n", argv[server_index + 1]);
                return -1;
            }
            server_port = (int)parsed_port;
        }

        int ret = coex_iperf_set_server(argv[server_index], server_port);
        if (ret == -EBUSY) {
            shellPrint(shell, "iperf is running, run 'iperf stop' before changing server\n");
        }
        shellPrint(shell, "iperf server set ret=%d ip=%s port=%d\n", ret, argv[server_index], server_port);
        return ret;
    }

    if (strcmp(subcmd, "mode") == 0) {
        coex_mode_t mode;
        int mode_index = arg_base + 1;

        if (argc <= mode_index) {
            shellPrint(shell, "current mode: %s\n", coex_mode_str(coex_iperf_get_mode()));
            shellPrint(shell, "set mode: iperf mode uplink|downlink|bidirectional\n");
            return 0;
        }

        if (coex_mode_parse(argv[mode_index], &mode) != 0) {
            shellPrint(shell, "Unknown mode: %s\n", argv[mode_index]);
            shellPrint(shell, "Usage: iperf mode uplink|downlink|bidirectional\n");
            return -1;
        }

        coex_iperf_set_mode(mode);
        shellPrint(shell, "iperf mode %s will apply on next round\n", coex_mode_str(mode));
        return 0;
    }

    shellPrint(shell, "Usage: iperf <start|stop|status|mode|server|help>\n");
    return -1;
}

static int cmd_bt_inquiry_shell(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return coex_bt_inquiry();
}

static int cmd_bt_open_shell(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return coex_bt_open();
}

static int cmd_bt_close_shell(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    (void)coex_bt_audio_stop();
    return coex_bt_close();
}

static int cmd_bt_connect_shell(int argc, char **argv)
{
    if (argc < 2) {
        return -1;
    }
    return coex_bt_connect_by_name(argv[1]);
}

static int cmd_bt_connect_index_shell(int argc, char **argv)
{
    if (argc < 2) {
        return -1;
    }
    return coex_bt_connect_by_index((uint8_t)atoi(argv[1]));
}

static int cmd_bt_audio_start_shell(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return coex_bt_audio_start();
}

static int cmd_bt_audio_stop_shell(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return coex_bt_audio_stop();
}

static int cmd_bt_audio_volume_shell(int argc, char **argv)
{
    if (argc < 2) {
        return -1;
    }
    return coex_bt_audio_set_volume((uint8_t)atoi(argv[1]));
}

static int cmd_coex_slot_time_shell(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();
    unsigned int coex_period_us;
    unsigned int wifi_time_us;
    int ret;

    if (argc < 3) {
        shellPrint(shell, "Usage: coex_slot_time <coex_period_us> <wifi_time_us>\n");
        return -1;
    }

    if (coex_shell_parse_uint(argv[1], &coex_period_us) != 0 ||
        coex_shell_parse_uint(argv[2], &wifi_time_us) != 0) {
        shellPrint(shell, "Invalid argument\n");
        shellPrint(shell, "Usage: coex_slot_time <coex_period_us> <wifi_time_us>\n");
        return -1;
    }

    ret = coex_win_slot_time_set(coex_period_us, wifi_time_us);
    shellPrint(shell, "coex_slot_time ret=%d, coex_period_us=%u, wifi_time_us=%u\n",
               ret, coex_period_us, wifi_time_us);
    return ret;
}

static int cmd_coex(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();

    if (argc < 2 || strcmp(argv[1], "status") != 0) {
        shellPrint(shell, "Usage: coex status\n");
        return -1;
    }

    shellPrint(shell, "[wifi]\n");
    coex_shell_print_wifi_status(shell);
    shellPrint(shell, "[iperf]\n");
    coex_shell_print_iperf_status(shell);
    shellPrint(shell, "[bt]\n");
    coex_shell_print_bt_status(shell);
    return 0;
}

static int threads_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    Shell *shell = shellGetCurrent();
    uint32_t tasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *item = lisa_mem_alloc(tasks * sizeof(TaskStatus_t));

    if (item) {
        uint32_t total = 0;

        tasks = uxTaskGetSystemState(item, tasks, &total);
        if (total > 0) {
            shellPrint(shell, "\n---------------------------------------------------------------------------------------------\n");
            shellPrint(shell, "Name                      State  Prio  Stack  MinFree    MaxUsed    Tid    Call100US      PCT\n");
            shellPrint(shell, "---------------------------------------------------------------------------------------------\n");
            for (uint32_t i = 0, pct = 0; i < tasks; i++) {
                uint32_t stack_size = (item[i].pxEndOfStack - item[i].pxStackBase + 2) * sizeof(StackType_t);
                uint32_t min_free = item[i].usStackHighWaterMark * sizeof(StackType_t);
                float max_used_pct = 100.0f - (float)min_free / (float)stack_size * 100.0f;

                if ((pct = (uint32_t)(100.0f * item[i].ulRunTimeCounter / total))) {
                    shellPrint(shell, "%-25s %-6c %-6u %-6u %-10u %-10.1f %-6u %-12u %5u%%\n",
                               item[i].pcTaskName,
                               "XRBSD"[item[i].eCurrentState],
                               item[i].uxCurrentPriority,
                               stack_size,
                               min_free,
                               max_used_pct,
                               item[i].xTaskNumber,
                               item[i].ulRunTimeCounter,
                               pct);
                } else {
                    shellPrint(shell, "%-25s %-6c %-6u %-6u %-10u %-10.1f %-6u %-12u %5s%%\n",
                               item[i].pcTaskName,
                               "XRBSD"[item[i].eCurrentState],
                               item[i].uxCurrentPriority,
                               stack_size,
                               min_free,
                               max_used_pct,
                               item[i].xTaskNumber,
                               item[i].ulRunTimeCounter,
                               "<1");
                }
            }
            shellPrint(shell, "---------------------------------------------------------------------------------------------\n\n");
        }
        lisa_mem_free(item);
    }

    return 0;
}

int coex_shell_register(void)
{
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), wifi, cmd_wifi, wifi controls);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), iperf, cmd_iperf, iperf controls);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), bt_open, cmd_bt_open_shell, open bt stack);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), bt_close, cmd_bt_close_shell, close bt stack);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), bt_inquiry, cmd_bt_inquiry_shell, bt inquiry);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), bt_connect, cmd_bt_connect_shell, bt connect by name);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), bt_connect_index, cmd_bt_connect_index_shell, bt connect by index);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), bt_audio_start, cmd_bt_audio_start_shell, start bt audio);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), bt_audio_stop, cmd_bt_audio_stop_shell, stop bt audio);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), bt_audio_volume, cmd_bt_audio_volume_shell, set bt audio volume);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), coex_slot_time, cmd_coex_slot_time_shell, set coex slot time);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), coex, cmd_coex, coexistence controls);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, threads,
                 threads_cmd, show threads info);
