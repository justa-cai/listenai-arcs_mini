/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nettest_wifi.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "nettest_config.h"
#include "user_fs.h"

#include "lisa_kv.h"
#include "lisa_wifi.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "net_al.h"
#include "net_def.h"
#include "wifi_manager/wifi_manager.h"

#define LOG_TAG "iperf.wifi"
#include "lisa_log.h"

#define NETTEST_AUTOCONN_INTERVAL_MS 2000
#define NETTEST_WIFI_SEARCH_AP_BUFFER_SIZE 10

static mac_manager_t *g_mac_manager;
static volatile bool g_wifi_connected;
static volatile bool g_wifi_ready;
static volatile bool g_wifi_connecting;
static bool g_wifi_inited;
static volatile bool g_wifi_stack_ready;
static TickType_t g_last_connect_attempt_tick;
static wifi_mgr_sta_config_t g_saved_ap_list[NETTEST_WIFI_SEARCH_AP_BUFFER_SIZE];

static int8_t custom_get_wifi_mac(uint8_t mac_addr[6])
{
    if (mac_addr == NULL) {
        return -1;
    }

    int8_t ret = mac_manager_get(g_mac_manager, mac_addr, 6);
    LOGI("wifi mac: %02X:%02X:%02X:%02X:%02X:%02X",
         mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    return ret;
}

static void dhcp_status_callback(int vif_idx, bool success, uint32_t ip_addr, uint32_t netmask, uint32_t gateway, void *arg)
{
    (void)netmask;
    (void)gateway;
    (void)arg;

    if (success) {
        LOGI("DHCP success on VIF-%d: IP=%u.%u.%u.%u",
             vif_idx,
             ip_addr & 0xff, (ip_addr >> 8) & 0xff, (ip_addr >> 16) & 0xff, (ip_addr >> 24) & 0xff);
        g_wifi_ready = true;
    } else {
        LOGI("DHCP failed on VIF-%d", vif_idx);
        g_wifi_ready = false;
    }
}

static void wifi_mgr_connection_status_cb(wifi_mgr_connection_info_t *connection_info, void *arg)
{
    net_if_t *net_if;

    (void)arg;
    if (connection_info == NULL) {
        return;
    }

    LOGI("wifi connection status: %d", connection_info->status);

    switch (connection_info->status) {
    case WIFI_MGR_STA_CONNECTED:
        g_wifi_connecting = false;
        g_wifi_connected = true;
        g_wifi_ready = false;
        net_if = net_if_get(WIFI_VIF_STA_IDX);
        if (net_if != NULL) {
            net_if_up(net_if);
            if (!net_if->static_ip) {
                ls_dhcpc_start(WIFI_VIF_STA_IDX);
            }
        }
        LOGI("wifi connected to AP");
        break;
    case WIFI_MGR_STA_DISCONNECTED:
        g_wifi_connecting = false;
        g_wifi_connected = false;
        g_wifi_ready = false;
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        net_if = net_if_get(WIFI_VIF_STA_IDX);
        if (net_if != NULL) {
            net_if_down(net_if);
        }
        LOGI("wifi disconnected from AP");
        break;
    case WIFI_MGR_STA_CONNECT_FAILED:
        g_wifi_connecting = false;
        g_wifi_connected = false;
        g_wifi_ready = false;
        LOGI("wifi connect failed, reason=%d", connection_info->reason);
        break;
    case WIFI_MGR_STA_CONNECTING:
        g_wifi_connecting = true;
        LOGI("wifi connecting...");
        break;
    default:
        break;
    }
}

static void user_mac_manager_init(void)
{
    mac_manager_config_t config = {
        .random_mac_if_mac_invalid = false,
    };

    g_mac_manager = mac_manager_init(mac_manager_ops_get()->mem_ops, mac_manager_ops_get()->content_ops, &config);
    assert(g_mac_manager != NULL);
}

static void user_wifi_manager_init(void)
{
    wifi_mgr_sta_config_t cfg = {0};
    wifi_mgr_autoconn_config_t autoconn_cfg = {
        .interval_ms = NETTEST_AUTOCONN_INTERVAL_MS,
    };

    wifi_mgr_init(wifi_mgr_ops_get());
    wifi_mgr_sta_enable();
    wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_status_cb, NULL);

    strncpy(cfg.ssid, TARGET_WIFI_SSID, sizeof(cfg.ssid) - 1);
    strncpy(cfg.pwd, TARGET_WIFI_PWD, sizeof(cfg.pwd) - 1);

    int count = wifi_mgr_storage_search_ap(g_saved_ap_list, NETTEST_WIFI_SEARCH_AP_BUFFER_SIZE, SEARCH_ALL, NULL);
    for (int i = 0; i < count; i++) {
        LOGI("deleting saved AP: ssid=%s", g_saved_ap_list[i].ssid);
        wifi_mgr_storage_delete_ap(&g_saved_ap_list[i]);
    }

    int ret = wifi_mgr_storage_save_ap(&cfg);
    LOGI("wifi storage save ret=%d ssid=%s", ret, cfg.ssid);
    ret = wifi_mgr_auto_connect_start(&autoconn_cfg);
    LOGI("wifi auto connect start ret=%d", ret);
}

static void cb_lisa_wifi_init_done(void)
{
    LOGI("lisa_wifi_init_done");
    user_fs_init();
    lisa_kv_init();
    user_wifi_manager_init();
    g_wifi_stack_ready = true;
}

int nettest_wifi_init(void)
{
    if (g_wifi_inited) {
        return 0;
    }

    user_mac_manager_init();
    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    lisa_wifi_ops_t ops = {
        .init_done = cb_lisa_wifi_init_done,
        .custom_mac = custom_get_wifi_mac,
    };

    lisa_wifi_init(&ops);

    g_wifi_inited = true;
    return 0;
}

bool nettest_wifi_is_connected(void)
{
    return g_wifi_connected;
}

bool nettest_wifi_is_ready(void)
{
    return g_wifi_connected && g_wifi_ready;
}

int nettest_wifi_request_connect(void)
{
    if (!g_wifi_stack_ready || g_wifi_connected || g_wifi_connecting) {
        return 0;
    }

    TickType_t now = xTaskGetTickCount();
    if ((now - g_last_connect_attempt_tick) < pdMS_TO_TICKS(3000)) {
        return 0;
    }

    g_last_connect_attempt_tick = now;
    g_wifi_connecting = true;

    wifi_mgr_autoconn_config_t autoconn_cfg = {
        .interval_ms = NETTEST_AUTOCONN_INTERVAL_MS,
    };
    int ret = wifi_mgr_auto_connect_start(&autoconn_cfg);
    if (ret != 0) {
        g_wifi_connecting = false;
    }
    LOGI("wifi auto connect retrigger ret=%d", ret);
    return ret;
}

void nettest_wifi_reset_state(void)
{
    g_wifi_connecting = false;
    g_wifi_connected = false;
    g_wifi_ready = false;
}
