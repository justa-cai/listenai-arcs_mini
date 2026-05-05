/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "iperf"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lisa_shell.h"

#include "nettest_app.h"
#include "nettest_config.h"
#include "nettest_iperf3.h"
#include "nettest_wifi.h"

static volatile bool g_nettest_enabled = true;
static volatile nettest_app_state_t g_nettest_state = NETTEST_APP_STATE_INIT;
static volatile nettest_mode_t g_nettest_mode = NETTEST_DEFAULT_MODE;
static volatile nettest_mode_t g_pending_nettest_mode = NETTEST_DEFAULT_MODE;
static volatile uint32_t g_round_id;
static nettest_round_result_t g_last_result;
static bool g_stop_hint_logged;

static void nettest_app_set_state_locked(nettest_app_state_t state)
{
    taskENTER_CRITICAL();
    g_nettest_state = state;
    taskEXIT_CRITICAL();
}

static void nettest_app_store_last_result(const nettest_round_result_t *result)
{
    taskENTER_CRITICAL();
    memcpy(&g_last_result, result, sizeof(g_last_result));
    taskEXIT_CRITICAL();
}

static uint32_t nettest_app_next_round_id(void)
{
    uint32_t round_id;

    taskENTER_CRITICAL();
    g_round_id++;
    round_id = g_round_id;
    taskEXIT_CRITICAL();

    return round_id;
}

static void log_placeholder_warning(void)
{
    if (strcmp(TARGET_WIFI_SSID, NETTEST_PLACEHOLDER_WIFI_SSID) == 0 ||
        strcmp(TARGET_WIFI_PWD, NETTEST_PLACEHOLDER_WIFI_PWD) == 0) {
        LOGE("please update TARGET_WIFI_SSID/TARGET_WIFI_PWD in src/nettest_config.h before running");
    }
    if (strcmp(NETTEST_SERVER_IP, NETTEST_PLACEHOLDER_SERVER_IP) == 0) {
        LOGI("NETTEST_SERVER_IP is still the default example value: %s", NETTEST_SERVER_IP);
    }
}

static void log_round_summary(uint32_t round_id, const nettest_round_result_t *result)
{
    uint64_t tx_bps = nettest_result_tx_throughput_bps(result);
    uint64_t rx_bps = nettest_result_rx_throughput_bps(result);

    LOGI("round %u summary: mode=%s duration=%u ms tx_bytes=%llu tx_throughput=%llu bps "
         "rx_bytes=%llu rx_throughput=%llu bps errors=%u reason=%s",
         round_id,
         nettest_mode_str(result->mode),
         result->duration_ms,
         result->bytes_sent,
         tx_bps,
         result->bytes_received,
         rx_bps,
         result->error_count,
         nettest_stop_reason_str(result->stop_reason));
}

void nettest_app_set_enabled(bool enabled)
{
    taskENTER_CRITICAL();
    g_nettest_enabled = enabled;
    g_nettest_state = enabled ? NETTEST_APP_STATE_WAIT_NETWORK : NETTEST_APP_STATE_STOPPED;
    taskEXIT_CRITICAL();

    if (!enabled) {
        nettest_runner_iperf3_abort();
    }
}

bool nettest_app_is_enabled(void)
{
    return g_nettest_enabled;
}

void nettest_app_set_mode(nettest_mode_t mode)
{
    taskENTER_CRITICAL();
    g_pending_nettest_mode = mode;
    taskEXIT_CRITICAL();
}

nettest_mode_t nettest_app_get_mode(void)
{
    return g_pending_nettest_mode;
}

void nettest_app_get_status(nettest_app_status_t *status)
{
    if (status == NULL) {
        return;
    }

    taskENTER_CRITICAL();
    status->enabled = g_nettest_enabled;
    status->state = g_nettest_state;
    status->current_mode = g_nettest_mode;
    status->pending_mode = g_pending_nettest_mode;
    status->round_id = g_round_id;
    memcpy(&status->last_result, &g_last_result, sizeof(g_last_result));
    taskEXIT_CRITICAL();
}

const char *nettest_app_state_str(nettest_app_state_t state)
{
    switch (state) {
    case NETTEST_APP_STATE_INIT:
        return "init";
    case NETTEST_APP_STATE_WAIT_NETWORK:
        return "wait_network";
    case NETTEST_APP_STATE_RUNNING:
        return "running";
    case NETTEST_APP_STATE_STOPPED:
        return "stopped";
    default:
        return "unknown";
    }
}

int main(int argc, char **argv)
{
    int ret;

    (void)argc;
    (void)argv;

    LOGI("ARCS SDK iperf-like sample starting");
    LOGI("mode: iperf3 protocol");
    LOGI("default test mode: %s", nettest_mode_str(NETTEST_DEFAULT_MODE));
    LOGI("target server: %s:%d", NETTEST_SERVER_IP, NETTEST_SERVER_PORT);
    LOGI("round_seconds=%d block_size=%d interval_ms=%d",
         NETTEST_ROUND_SECONDS, NETTEST_SEND_BLOCK_SIZE, NETTEST_ROUND_INTERVAL_MS);
    LOGI("default mode: auto start after wifi is ready");
    LOGI("shell override: use 'iperf stop' to pause, 'iperf start' to resume");

    log_placeholder_warning();

    ret = lisa_shell_init();
    if (ret != 0) {
        LOGI("shell init failed: %d", ret);
    }

    ret = nettest_wifi_init();
    if (ret != 0) {
        LOGE("wifi init failed: %d", ret);
        return ret;
    }

    nettest_app_set_state_locked(NETTEST_APP_STATE_WAIT_NETWORK);

    while (1) {
        if (!g_nettest_enabled) {
            if (g_nettest_state != NETTEST_APP_STATE_STOPPED) {
                LOGI("nettest stopped by shell");
            }
            nettest_app_set_state_locked(NETTEST_APP_STATE_STOPPED);
            if (!g_stop_hint_logged) {
                LOGI("auto test paused, waiting for shell command 'iperf start'");
                g_stop_hint_logged = true;
            }
            if (!nettest_wifi_is_ready()) {
                (void)nettest_wifi_request_connect();
            }
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        if (!nettest_wifi_is_ready()) {
            if (g_nettest_state != NETTEST_APP_STATE_WAIT_NETWORK) {
                LOGI("waiting for wifi connection and IP");
            }
            nettest_app_set_state_locked(NETTEST_APP_STATE_WAIT_NETWORK);
            g_stop_hint_logged = false;
            (void)nettest_wifi_request_connect();
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        g_stop_hint_logged = false;
        nettest_app_set_state_locked(NETTEST_APP_STATE_RUNNING);
        taskENTER_CRITICAL();
        g_nettest_mode = g_pending_nettest_mode;
        taskEXIT_CRITICAL();
        uint32_t round_id = nettest_app_next_round_id();
        LOGI("round %u start, mode=%s", round_id, nettest_mode_str(g_nettest_mode));

        nettest_round_result_t round_result;
        memset(&round_result, 0, sizeof(round_result));
        round_result.mode = g_nettest_mode;
        ret = nettest_runner_iperf3_run(g_nettest_mode, &g_nettest_enabled, &round_result);
        nettest_app_store_last_result(&round_result);
        if (ret != 0) {
            LOGI("round %u ended with runner return=%d", round_id, ret);
        }
        log_round_summary(round_id, &round_result);

        if (!g_nettest_enabled) {
            nettest_app_set_state_locked(NETTEST_APP_STATE_STOPPED);
            continue;
        }

        if (!nettest_wifi_is_ready()) {
            nettest_app_set_state_locked(NETTEST_APP_STATE_WAIT_NETWORK);
            LOGI("network lost, pause until reconnect");
            continue;
        }

        /* iperf3 服务器单客户端，失败后需等待服务器清理残留连接 */
        if (ret != 0) {
            LOGI("iperf3 failed, waiting 5s before retry...");
            vTaskDelay(pdMS_TO_TICKS(5000));
        } else {
            vTaskDelay(pdMS_TO_TICKS(NETTEST_ROUND_INTERVAL_MS));
        }
    }

    return 0;
}
