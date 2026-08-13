/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "wifi_utils.h"

#if defined(CONFIG_AVI_PLAYER_HTTP_RANGE)

#include "lisa_kv.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "wifi_manager/wifi_manager.h"
#include "wifi_manager/wifi_manager_ops.h"
#include "lisa_wifi.h"
#include "ls_wifi_type.h"
#include "net_al.h"
#include "net_def.h"
#include "lisa_thread.h"
#include <string.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "wifi_utils"
#include <lisa_log.h>

static mac_manager_t *s_mac_manager;
static volatile bool s_wifi_connected;
static volatile bool s_get_ip_success;
static bool s_driver_inited;
static bool s_mgr_inited;

static int8_t local_get_wifi_mac(uint8_t mac_addr[6])
{
    if (!mac_addr || !s_mac_manager) {
        return -1;
    }
    return mac_manager_get(s_mac_manager, mac_addr, 6);
}

static void dhcp_status_callback(int vif_idx, bool success, uint32_t ip_addr,
                                  uint32_t netmask, uint32_t gateway, void *arg)
{
    (void)netmask;
    (void)gateway;
    (void)arg;

    if (success) {
        LISA_LOGI(LOG_TAG, "DHCP Success VIF-%d: IP=%u.%u.%u.%u",
                  vif_idx,
                  (unsigned)(ip_addr & 0xff),
                  (unsigned)((ip_addr >> 8) & 0xff),
                  (unsigned)((ip_addr >> 16) & 0xff),
                  (unsigned)((ip_addr >> 24) & 0xff));
        s_get_ip_success = true;
    } else {
        LISA_LOGI(LOG_TAG, "DHCP Failed VIF-%d", vif_idx);
    }
}

static void wifi_mgr_connection_status_cb(wifi_mgr_connection_info_t *info, void *arg)
{
    (void)arg;
    if (!info) {
        return;
    }

    switch (info->status) {
    case WIFI_MGR_STA_CONNECTED: {
        net_if_t *net_if = net_if_get(WIFI_VIF_STA_IDX);
        s_wifi_connected = true;
        if (net_if) {
            net_if_up(net_if);
            if (!net_if->static_ip) {
                ls_dhcpc_start(WIFI_VIF_STA_IDX);
            }
        }
        break;
    }
    case WIFI_MGR_STA_DISCONNECTED:
        s_wifi_connected = false;
        s_get_ip_success = false;
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        net_if_down(net_if_get(WIFI_VIF_STA_IDX));
        break;
    default:
        break;
    }
}

int wifi_utils_init(void)
{
    if (s_driver_inited) {
        return 0;
    }

    mac_manager_config_t config = {
        .random_mac_if_mac_invalid = false,
    };
    s_mac_manager = mac_manager_init(mac_manager_ops_get()->mem_ops,
                                      mac_manager_ops_get()->content_ops, &config);
    if (!s_mac_manager) {
        LISA_LOGE(LOG_TAG, "mac_manager_init failed");
        return -1;
    }

    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    lisa_wifi_ops_t ops = {
        .custom_mac = local_get_wifi_mac,
    };
    lisa_wifi_init(&ops);

    lisa_kv_init();

    s_driver_inited = true;
    LISA_LOGI(LOG_TAG, "WiFi driver initialized");
    return 0;
}

int wifi_utils_connect(const char *ssid, const char *pwd, int timeout_s)
{
    if (!ssid) {
        return -1;
    }

    if (s_wifi_connected && s_get_ip_success) {
        LISA_LOGI(LOG_TAG, "Already connected");
        return 0;
    }

    if (!s_driver_inited) {
        if (wifi_utils_init() != 0) {
            return -1;
        }
    }

    if (!s_mgr_inited) {
        wifi_mgr_init(wifi_mgr_ops_get());
        wifi_mgr_sta_enable();
        wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_status_cb, NULL);
        s_mgr_inited = true;
    }

    wifi_mgr_sta_config_t cfg = {0};
    strncpy((char *)cfg.ssid, ssid, sizeof(cfg.ssid) - 1);
    if (pwd) {
        strncpy((char *)cfg.pwd, pwd, sizeof(cfg.pwd) - 1);
    }

    wifi_mgr_autoconn_config_t autoconn_cfg = {
        .interval_ms = 2000,
    };

    (void)wifi_mgr_storage_save_ap(&cfg);
    (void)wifi_mgr_auto_connect_start(&autoconn_cfg);

    LISA_LOGI(LOG_TAG, "Connecting to WiFi: %s ...", ssid);

    if (timeout_s <= 0) {
        timeout_s = 30;
    }

    while (timeout_s > 0) {
        if (s_wifi_connected && s_get_ip_success) {
            LISA_LOGI(LOG_TAG, "WiFi connected and IP obtained");
            lisa_thread_mdelay(500);
            return 0;
        }
        lisa_thread_mdelay(1000);
        timeout_s--;
    }

    LISA_LOGE(LOG_TAG, "WiFi/DHCP timeout (wifi=%d ip=%d)",
              (int)s_wifi_connected, (int)s_get_ip_success);
    return -1;
}

bool wifi_utils_is_connected(void)
{
    return s_wifi_connected && s_get_ip_success;
}

#else /* !CONFIG_AVI_PLAYER_HTTP_RANGE */

int wifi_utils_init(void)
{
    return -1;
}

int wifi_utils_connect(const char *ssid, const char *pwd, int timeout_s)
{
    (void)ssid;
    (void)pwd;
    (void)timeout_s;
    return -1;
}

bool wifi_utils_is_connected(void)
{
    return false;
}

#endif /* CONFIG_AVI_PLAYER_HTTP_RANGE */
