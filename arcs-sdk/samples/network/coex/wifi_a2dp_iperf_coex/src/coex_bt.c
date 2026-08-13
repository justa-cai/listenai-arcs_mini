/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#define LOG_TAG "coex.bt"
#include <lisa_log.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lisa_bluetooth.h"

#include "coex_bt.h"

static coex_bt_status_t g_coex_bt_status;

static void coex_bt_format_addr(const gap_bdaddr_t *peer_addr, char out[COEX_BT_ADDR_STR_LEN])
{
    if (peer_addr == NULL || out == NULL) {
        return;
    }

    (void)snprintf(out, COEX_BT_ADDR_STR_LEN,
                   "%02X:%02X:%02X:%02X:%02X:%02X",
                   peer_addr->addr[0], peer_addr->addr[1], peer_addr->addr[2],
                   peer_addr->addr[3], peer_addr->addr[4], peer_addr->addr[5]);
}

static void coex_bt_discovery_ind(const lisa_bt_discovery_info_t *info)
{
    const lisa_bt_discovery_info_t *list;
    uint8_t count = 0;

    if (info != NULL) {
        LOGI("discovered device: %02X:%02X:%02X:%02X:%02X:%02X RSSI=%d Name=%.*s",
             info->addr.addr[0], info->addr.addr[1], info->addr.addr[2],
             info->addr.addr[3], info->addr.addr[4], info->addr.addr[5],
             info->rssi,
             info->name_len, info->name);
    }

    if (lisa_bluetooth_get_discovered_devices(&list, &count) == 0) {
        (void)list;
        g_coex_bt_status.discovered_count = count;
    }
}

static void coex_bt_conn_cb(uint8_t conidx, uint16_t conhdl, const gap_bdaddr_t *peer_addr)
{
    (void)conidx;
    (void)conhdl;

    g_coex_bt_status.connected = true;
    coex_bt_format_addr(peer_addr, g_coex_bt_status.peer_addr);
    LOGI("bt classic connected: %s", g_coex_bt_status.peer_addr);
}

static void coex_bt_disc_cb(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    (void)conidx;
    (void)conhdl;

    g_coex_bt_status.connected = false;
    g_coex_bt_status.a2dp_connected = false;
    g_coex_bt_status.peer_addr[0] = '\0';
    LOGI("bt classic disconnected, reason=%u", reason);
}

static void coex_bt_profile_cb(uint8_t conidx, int profile, bool connected)
{
    (void)conidx;

    if (profile == LISA_BT_PROFILE_A2DP) {
        g_coex_bt_status.a2dp_connected = connected;
        LOGI("a2dp profile %s", connected ? "connected" : "disconnected");
    }
}

int coex_bt_init(void)
{
    int ret;

    if (g_coex_bt_status.initialized) {
        return 0;
    }

    ret = lisa_bluetooth_init(NULL);
    if (ret != 0) {
        LOGE("lisa_bluetooth_init failed: %d", ret);
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
    lisa_bluetooth_register_discovery_callback(coex_bt_discovery_ind);
    lisa_bt_classic_register_conn_cb(coex_bt_conn_cb);
    lisa_bt_classic_register_disc_cb(coex_bt_disc_cb);
    lisa_bt_classic_register_profile_cb(coex_bt_profile_cb);

    g_coex_bt_status.initialized = true;
    g_coex_bt_status.discovery_registered = true;
    LOGI("bt classic init done");
    return 0;
}

int coex_bt_open(void)
{
    int ret;

    if (!g_coex_bt_status.initialized) {
        ret = coex_bt_init();
        if (ret != 0) {
            return ret;
        }
    }

    ret = lisa_bluetooth_open();
    g_coex_bt_status.opened = lisa_bluetooth_is_opened();
    LOGI("bt open ret=%d opened=%d", ret, g_coex_bt_status.opened);
    return ret;
}

int coex_bt_close(void)
{
    int ret;

    g_coex_bt_status.discovered_count = 0;
    g_coex_bt_status.connected = false;
    g_coex_bt_status.a2dp_connected = false;
    g_coex_bt_status.peer_addr[0] = '\0';

    ret = lisa_bluetooth_close();
    g_coex_bt_status.opened = lisa_bluetooth_is_opened();
    LOGI("bt close ret=%d opened=%d", ret, g_coex_bt_status.opened);
    return ret;
}

int coex_bt_inquiry(void)
{
    lisa_bluetooth_clear_discovered_devices();
    g_coex_bt_status.discovered_count = 0;
    return lisa_bluetooth_inquiry_start(GAPM_DISC_TYPE_GEN_DISC, MAX_DISCOVERED_DEVICES);
}

int coex_bt_connect_by_name(const char *name)
{
    if (name == NULL) {
        return -1;
    }
    return lisa_bluetooth_connect_by_name(name);
}

int coex_bt_connect_by_index(uint8_t index)
{
    return lisa_bluetooth_connect_by_index(index);
}

bool coex_bt_is_connected(void)
{
    return g_coex_bt_status.connected;
}

bool coex_bt_is_a2dp_connected(void)
{
    return g_coex_bt_status.a2dp_connected;
}

void coex_bt_get_status(coex_bt_status_t *status)
{
    const lisa_bt_discovery_info_t *list;
    uint8_t count = 0;

    if (status == NULL) {
        return;
    }

    if (lisa_bluetooth_get_discovered_devices(&list, &count) == 0) {
        (void)list;
        g_coex_bt_status.discovered_count = count;
    }

    g_coex_bt_status.opened = lisa_bluetooth_is_opened();

    *status = g_coex_bt_status;
}
