/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define TAG "samples"
#include "lisa_log.h"

#include "lisa_kv.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "wifi_manager/wifi_manager.h"
#include "user_fs.h"
#include "lisa_wifi.h"
#include "ls_wifi_type.h"
#include "net_al.h"
#include "net_def.h"

#define TARGET_WIFI_SSID "listenai"
#define TARGET_WIFI_PWD "listenai"

#define WIFI_MGR_SEARCH_AP_BUFFER_SIZE 10
#define WIFI_MGR_AUTO_CONNECT_INTERVAL_MS 2000
#define NET_CONNECT_TIMEOUT_MS 30000  // 30秒超时

static mac_manager_t *m_mac_manager;
static wifi_mgr_sta_config_t list[WIFI_MGR_SEARCH_AP_BUFFER_SIZE] = { 0 };
static SemaphoreHandle_t net_connect_sem = NULL;
static bool net_connect_success = false;

static void user_wifi_manager_init(void);

static void cb_lisa_wifi_init_done(void)
{
    LOGI("lisa_wifi_init_done");

    user_fs_init();
    lisa_kv_init();
    user_wifi_manager_init();
}

static int8_t custom_get_wifi_mac(uint8_t mac_addr[6])
{
    int8_t ret = 0;

    if (!mac_addr)
        return -1;

    ret = mac_manager_get(m_mac_manager, mac_addr, 6);
    LOGI("custom_get_wifi_mac: %d, %02X:%02X:%02X:%02X:%02X:%02X", ret, mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    return ret;
}

static void dhcp_status_callback(int vif_idx, bool success, uint32_t ip_addr, uint32_t netmask, uint32_t gateway, void *arg)
{
    if (success) {
        LOGI("DHCP Success on VIF-%d: IP=%d.%d.%d.%d, Mask=%d.%d.%d.%d, GW=%d.%d.%d.%d",
             vif_idx,
             ip_addr & 0xff, (ip_addr >> 8) & 0xff, (ip_addr >> 16) & 0xff, (ip_addr >> 24) & 0xff,
             netmask & 0xff, (netmask >> 8) & 0xff, (netmask >> 16) & 0xff, (netmask >> 24) & 0xff,
             gateway & 0xff, (gateway >> 8) & 0xff, (gateway >> 16) & 0xff, (gateway >> 24) & 0xff);

        // 设置连接成功标志并释放信号量
        net_connect_success = true;
        if (net_connect_sem != NULL) {
            xSemaphoreGive(net_connect_sem);
        }
    } else {
        LOGI("DHCP Failed on VIF-%d", vif_idx);

        // DHCP失败也需要释放信号量
        net_connect_success = false;
        if (net_connect_sem != NULL) {
            xSemaphoreGive(net_connect_sem);
        }
    }
}

static void wifi_mgr_connection_cb(wifi_mgr_connection_info_t *connection_info, void *arg)
{
    net_if_t *net_if;

    LOGI("mgr connection: %d", connection_info->status);

    switch (connection_info->status)
    {
        case WIFI_MGR_STA_CONNECTED:
            LOGI("WiFi connected, starting network interface");
            net_if = net_if_get(WIFI_VIF_STA_IDX);
            net_if_up(net_if);
            if (!net_if->static_ip) {
                ls_dhcpc_start(WIFI_VIF_STA_IDX);
            }
            break;

        case WIFI_MGR_STA_DISCONNECTED:
            LOGI("WiFi disconnected, stopping network interface");
            ls_dhcpc_stop(WIFI_VIF_STA_IDX);
            net_if_down(net_if_get(WIFI_VIF_STA_IDX));
            break;

        case WIFI_MGR_STA_CONNECTING:
            LOGI("WiFi connecting...");
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

    m_mac_manager = mac_manager_init(mac_manager_ops_get()->mem_ops, mac_manager_ops_get()->content_ops, &config);
    if (m_mac_manager == NULL) {
        LOGI("[user_wifi]mac_manager_init failed\n");
        assert(0);
    }
}

static void user_wifi_manager_init(void)
{
    wifi_mgr_sta_config_t cfg = {
        .ssid = TARGET_WIFI_SSID,
        .pwd = TARGET_WIFI_PWD,
    };

    wifi_mgr_autoconn_config_t autoconn_cfg = {
        .interval_ms = WIFI_MGR_AUTO_CONNECT_INTERVAL_MS,
    };
    
    wifi_mgr_init(wifi_mgr_ops_get());
    wifi_mgr_sta_enable();

    wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_cb, NULL);

    /* Search and delete all saved APs */
    int count = wifi_mgr_storage_search_ap(list, 10, SEARCH_ALL, NULL);
    for (int i = 0; i < count; i++) {
        LOGI("Deleting saved AP: ssid=%s", list[i].ssid);
        wifi_mgr_storage_delete_ap(&list[i]);
    }

    wifi_mgr_storage_save_ap(&cfg);
    wifi_mgr_auto_connect_start(&autoconn_cfg);
}

int net_connect(void)
{
    BaseType_t ret;

    // 创建信号量
    if (net_connect_sem == NULL) {
        net_connect_sem = xSemaphoreCreateBinary();
        if (net_connect_sem == NULL) {
            LOGI("Failed to create net_connect semaphore");
            return -1;
        }
    }

    // 重置状态标志
    net_connect_success = false;

    user_mac_manager_init();

    // 注册 DHCP 状态回调
    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    lisa_wifi_ops_t ops = {
        .init_done = cb_lisa_wifi_init_done,
        .custom_mac = custom_get_wifi_mac,
    };
    lisa_wifi_init(&ops);

    // 等待获取IP地址或超时
    LOGI("Waiting for network connection and IP address...");
    ret = xSemaphoreTake(net_connect_sem, pdMS_TO_TICKS(NET_CONNECT_TIMEOUT_MS));

    if (ret == pdTRUE) {
        if (net_connect_success) {
            LOGI("Network connected successfully with IP address");
            return 0;
        } else {
            LOGI("Network connection failed: DHCP failed");
            return -2;
        }
    } else {
        LOGI("Network connection timeout after %d ms", NET_CONNECT_TIMEOUT_MS);
        return -3;
    }
}
