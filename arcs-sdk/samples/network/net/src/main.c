/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief 网络设备抽象层（NetDev）示例
 *
 * 本示例演示如何使用 LISA 网络设备抽象层实现以下功能：
 * 1. 初始化网络设备子系统
 * 2. 注册和管理多个网络设备（WiFi、4G）
 * 3. 设置网络设备优先级
 * 4. 监听网络状态变化
 * 5. 使用标准 socket API 进行网络通信
 */

#define LOG_TAG "net"
#include <lisa_log.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#include "netdev.h"
#include "user_fs.h"
#include "lisa_kv.h"

#ifdef CONFIG_WIFI
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "wifi_manager/wifi_manager.h"
#include "lisa_wifi.h"
#include "ls_wifi_type.h"
#include "net_al.h"
#include "net_def.h"
#endif

#ifdef CONFIG_LISA_MODEM
#include "lisa_modem_module.h"
#endif

// ============================================================
// 重要：使用前请修改以下配置！
// ============================================================
#ifdef CONFIG_WIFI
#define TARGET_WIFI_SSID    "your_actual_wifi_name"      // 修改为你的WiFi名称
#define TARGET_WIFI_PWD     "your_actual_wifi_password"  // 修改为你的WiFi密码
#endif

#ifdef CONFIG_LISA_MODEM
#define MODEM_UART_DEVICE   "uart2"
#endif

#define WIFI_MGR_AUTO_CONNECT_INTERVAL_MS 2000
#define WIFI_MGR_SEARCH_AP_BUFFER_SIZE 10

#ifdef CONFIG_WIFI
static mac_manager_t *m_mac_manager;
static wifi_mgr_sta_config_t list[WIFI_MGR_SEARCH_AP_BUFFER_SIZE] = {0};
static volatile bool g_wifi_connected = false;
static volatile bool g_wifi_got_ip = false;
#endif

/**
 * @brief 网络设备状态变化回调
 */
static void netdev_status_callback(struct netdev *netdev, enum netdev_cb_type type)
{
    const char *type_str = "UNKNOWN";

    switch (type) {
    case NETDEV_CB_STATUS_UP:
        type_str = "UP";
        break;
    case NETDEV_CB_STATUS_DOWN:
        type_str = "DOWN";
        break;
    case NETDEV_CB_STATUS_LINK_UP:
        type_str = "LINK_UP";
        break;
    case NETDEV_CB_STATUS_LINK_DOWN:
        type_str = "LINK_DOWN";
        break;
    case NETDEV_CB_STATUS_INTERNET_UP:
        type_str = "INTERNET_UP";
        break;
    case NETDEV_CB_STATUS_INTERNET_DOWN:
        type_str = "INTERNET_DOWN";
        break;
    default:
        break;
    }

    LISA_LOGI(LOG_TAG, "NetDev [%s] status changed: %s", netdev->name, type_str);
}

/**
 * @brief 网络设备地址变化回调
 */
static void netdev_addr_callback(struct netdev *netdev, enum netdev_cb_type type)
{
    const char *type_str = "UNKNOWN";

    switch (type) {
    case NETDEV_CB_ADDR_IP:
        type_str = "IP_CHANGED";
        LISA_LOGI(LOG_TAG, "NetDev [%s] IP: %d.%d.%d.%d",
                  netdev->name,
                  netdev->ip_addr.addr & 0xFF,
                  (netdev->ip_addr.addr >> 8) & 0xFF,
                  (netdev->ip_addr.addr >> 16) & 0xFF,
                  (netdev->ip_addr.addr >> 24) & 0xFF);
        break;
    case NETDEV_CB_ADDR_NETMASK:
        type_str = "NETMASK_CHANGED";
        break;
    case NETDEV_CB_ADDR_GATEWAY:
        type_str = "GATEWAY_CHANGED";
        break;
    case NETDEV_CB_ADDR_DNS_SERVER:
        type_str = "DNS_CHANGED";
        break;
    default:
        break;
    }

    LISA_LOGI(LOG_TAG, "NetDev [%s] address changed: %s", netdev->name, type_str);
}

/**
 * @brief 打印所有已注册的网络设备信息
 */
static void print_netdev_list(void)
{
    struct netdev *netdev = netdev_list;
    int count = 0;

    LISA_LOGI(LOG_TAG, "=== Registered Network Devices ===");

    while (netdev != NULL) {
        LISA_LOGI(LOG_TAG, "[%d] Name: %s, Priority: %d, Flags: 0x%04X",
                  count, netdev->name, netdev->priority, netdev->flags);

        LISA_LOGI(LOG_TAG, "    IP: %d.%d.%d.%d",
                  netdev->ip_addr.addr & 0xFF,
                  (netdev->ip_addr.addr >> 8) & 0xFF,
                  (netdev->ip_addr.addr >> 16) & 0xFF,
                  (netdev->ip_addr.addr >> 24) & 0xFF);

        LISA_LOGI(LOG_TAG, "    Status: UP=%d, LINK=%d, INET=%d, DHCP=%d",
                  netdev_is_up(netdev),
                  netdev_is_link_up(netdev),
                  netdev_is_internet_up(netdev),
                  netdev_is_dhcp_enabled(netdev));

        netdev = netdev->next;
        count++;
    }

    if (count == 0) {
        LISA_LOGI(LOG_TAG, "No network devices registered");
    }

    if (netdev_default) {
        LISA_LOGI(LOG_TAG, "Default device: %s", netdev_default->name);
    } else {
        LISA_LOGI(LOG_TAG, "Default device: (none)");
    }
}

#ifdef CONFIG_WIFI
static int8_t custom_get_wifi_mac(uint8_t mac_addr[6])
{
    int8_t ret = 0;

    if (!mac_addr)
        return -1;

    ret = mac_manager_get(m_mac_manager, mac_addr, 6);
    LISA_LOGI(LOG_TAG, "WiFi MAC: %02X:%02X:%02X:%02X:%02X:%02X",
              mac_addr[0], mac_addr[1], mac_addr[2],
              mac_addr[3], mac_addr[4], mac_addr[5]);
    return ret;
}

static void dhcp_status_callback(int vif_idx, bool success, uint32_t ip_addr,
                                  uint32_t netmask, uint32_t gateway, void *arg)
{
    if (success) {
        LISA_LOGI(LOG_TAG, "DHCP Success: IP=%d.%d.%d.%d",
                  ip_addr & 0xff, (ip_addr >> 8) & 0xff,
                  (ip_addr >> 16) & 0xff, (ip_addr >> 24) & 0xff);
        g_wifi_got_ip = true;
    } else {
        LISA_LOGI(LOG_TAG, "DHCP Failed on VIF-%d", vif_idx);
    }
}

static void wifi_mgr_connection_status_cb(wifi_mgr_connection_info_t *connection_info, void *arg)
{
    net_if_t *net_if;

    LISA_LOGI(LOG_TAG, "WiFi connection status: %d", connection_info->status);

    switch (connection_info->status) {
    case WIFI_MGR_STA_CONNECTED:
        LISA_LOGI(LOG_TAG, "WiFi connected to AP");
        g_wifi_connected = true;
        net_if = net_if_get(WIFI_VIF_STA_IDX);
        net_if_up(net_if);
        if (!net_if->static_ip) {
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        }
        break;

    case WIFI_MGR_STA_DISCONNECTED:
        LISA_LOGI(LOG_TAG, "WiFi disconnected from AP");
        g_wifi_connected = false;
        g_wifi_got_ip = false;
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        net_if_down(net_if_get(WIFI_VIF_STA_IDX));
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

    m_mac_manager = mac_manager_init(mac_manager_ops_get()->mem_ops,
                                      mac_manager_ops_get()->content_ops, &config);
    if (m_mac_manager == NULL) {
        LISA_LOGE(LOG_TAG, "Error: mac_manager_init failed");
    }
}

static int init_wifi_device(void)
{
    LISA_LOGI(LOG_TAG, "Initializing WiFi device...");

    /* 初始化 MAC 管理器 */
    user_mac_manager_init();

    /* 注册 DHCP 状态回调 */
    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    /* 初始化 LISA WiFi */
    lisa_wifi_ops_t ops = {
        .custom_mac = custom_get_wifi_mac,
    };
    lisa_wifi_init(&ops);

    /* 初始化 WiFi 管理器 */
    wifi_mgr_sta_config_t cfg = {
        .ssid = TARGET_WIFI_SSID,
        .pwd = TARGET_WIFI_PWD,
    };

    wifi_mgr_autoconn_config_t autoconn_cfg = {
        .interval_ms = WIFI_MGR_AUTO_CONNECT_INTERVAL_MS,
    };

    wifi_mgr_init(wifi_mgr_ops_get());
    wifi_mgr_sta_enable();
    wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_status_cb, NULL);

    /* 删除所有已保存的 AP */
    int count = wifi_mgr_storage_search_ap(list, WIFI_MGR_SEARCH_AP_BUFFER_SIZE, SEARCH_ALL, NULL);
    for (int i = 0; i < count; i++) {
        wifi_mgr_storage_delete_ap(&list[i]);
    }

    /* 保存 AP 配置并启动自动连接 */
    wifi_mgr_storage_save_ap(&cfg);
    wifi_mgr_auto_connect_start(&autoconn_cfg);

    /* 注册 WiFi 网络设备（优先级 100，较高优先级） */
    if (app_netdev_register("wifi0", 100) != 0) {
        LISA_LOGE(LOG_TAG, "Error: Failed to register WiFi network device");
        return -1;
    }

    LISA_LOGI(LOG_TAG, "WiFi device initialized and registered");
    return 0;
}
#endif

#ifdef CONFIG_LISA_MODEM
static int init_modem_device(void)
{
    LISA_LOGI(LOG_TAG, "Initializing 4G modem device...");

    /* 初始化 4G 模块 */
    if (!lisa_modem_module_init(MODEM_UART_DEVICE)) {
        LISA_LOGE(LOG_TAG, "Error: Failed to initialize 4G modem");
        return -1;
    }

    /* 4G 设备在 lisa_modem_module_init 中已自动注册，优先级 500（较低优先级） */
    LISA_LOGI(LOG_TAG, "4G modem device initialized and registered");
    return 0;
}
#endif

/**
 * @brief 演示网络设备优先级管理
 */
static void demo_priority_management(void)
{
    LISA_LOGI(LOG_TAG, "=== Priority Management Demo ===");

    print_netdev_list();

#ifdef CONFIG_WIFI
    /* 获取 WiFi 设备优先级 */
    int wifi_priority = netdev_get_priority("wifi0");
    if (wifi_priority >= 0) {
        LISA_LOGI(LOG_TAG, "WiFi priority: %d", wifi_priority);
    }
#endif

#ifdef CONFIG_LISA_MODEM
    /* 获取 4G 设备优先级 */
    int modem_priority = netdev_get_priority("ml307");
    if (modem_priority >= 0) {
        LISA_LOGI(LOG_TAG, "4G modem priority: %d", modem_priority);
    }

    /* 演示修改优先级 */
    LISA_LOGI(LOG_TAG, "Changing 4G modem priority to 50...");
    if (netdev_set_priority("ml307", 50) == 0) {
        LISA_LOGI(LOG_TAG, "4G modem priority changed successfully");
        print_netdev_list();

        /* 恢复原优先级 */
        LISA_LOGI(LOG_TAG, "Restoring 4G modem priority to 500...");
        netdev_set_priority("ml307", 500);
    }
#endif
}

/**
 * @brief 演示网络设备状态查询
 */
static void demo_device_status(void)
{
    struct netdev *netdev;

    LISA_LOGI(LOG_TAG, "=== Device Status Demo ===");

#ifdef CONFIG_WIFI
    netdev = netdev_get_by_name("wifi0");
    if (netdev) {
        LISA_LOGI(LOG_TAG, "WiFi device status:");
        LISA_LOGI(LOG_TAG, "  UP: %s", netdev_is_up(netdev) ? "Yes" : "No");
        LISA_LOGI(LOG_TAG, "  Link: %s", netdev_is_link_up(netdev) ? "Up" : "Down");
        LISA_LOGI(LOG_TAG, "  Internet: %s", netdev_is_internet_up(netdev) ? "Up" : "Down");
        LISA_LOGI(LOG_TAG, "  DHCP: %s", netdev_is_dhcp_enabled(netdev) ? "Enabled" : "Disabled");
    }
#endif

#ifdef CONFIG_LISA_MODEM
    netdev = netdev_get_by_name("ml307");
    if (netdev) {
        LISA_LOGI(LOG_TAG, "4G modem device status:");
        LISA_LOGI(LOG_TAG, "  UP: %s", netdev_is_up(netdev) ? "Yes" : "No");
        LISA_LOGI(LOG_TAG, "  Link: %s", netdev_is_link_up(netdev) ? "Up" : "Down");
        LISA_LOGI(LOG_TAG, "  Internet: %s", netdev_is_internet_up(netdev) ? "Up" : "Down");
    }
#endif

    /* 查找具有 LINK_UP 状态的设备 */
    netdev = netdev_get_first_by_flags(NETDEV_FLAG_LINK_UP);
    if (netdev) {
        LISA_LOGI(LOG_TAG, "First device with LINK_UP: %s", netdev->name);
    } else {
        LISA_LOGI(LOG_TAG, "No device with LINK_UP status");
    }
}

/**
 * @brief 为网络设备设置回调
 */
static void setup_netdev_callbacks(void)
{
    struct netdev *netdev;

#ifdef CONFIG_WIFI
    netdev = netdev_get_by_name("wifi0");
    if (netdev) {
        netdev_set_status_callback(netdev, netdev_status_callback);
        netdev_set_addr_callback(netdev, netdev_addr_callback);
        LISA_LOGI(LOG_TAG, "Callbacks set for WiFi device");
    }
#endif

#ifdef CONFIG_LISA_MODEM
    netdev = netdev_get_by_name("ml307");
    if (netdev) {
        netdev_set_status_callback(netdev, netdev_status_callback);
        netdev_set_addr_callback(netdev, netdev_addr_callback);
        LISA_LOGI(LOG_TAG, "Callbacks set for 4G modem device");
    }
#endif
}

/**
 * @brief 主函数
 */
int main(int argc, char **argv)
{
    /* 早期打印测试 */
    printf("=== NetDev Example Started ===\n");
    
    LISA_LOGI(LOG_TAG, "=== NetDev (Network Device Abstraction) Example ===");

    /* 初始化文件系统和 KV 存储 */
    user_fs_init();
    lisa_kv_init();

    /* 初始化网络设备子系统 */
    LISA_LOGI(LOG_TAG, "Initializing NetDev subsystem...");
    if (netdev_init() != 0) {
        LISA_LOGE(LOG_TAG, "Error: Failed to initialize NetDev subsystem");
        goto exit;
    }
    LISA_LOGI(LOG_TAG, "NetDev subsystem initialized");

    sal_init();
    /* 初始化网络设备 */
#ifdef CONFIG_WIFI
    init_wifi_device();
#endif

#ifdef CONFIG_LISA_MODEM
    init_modem_device();
#endif

    /* 设置网络设备回调 */
    setup_netdev_callbacks();

    /* 等待网络设备就绪 */
    LISA_LOGI(LOG_TAG, "Waiting for network devices to be ready...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    /* 演示功能 */
    demo_priority_management();
    vTaskDelay(pdMS_TO_TICKS(1000));

    demo_device_status();

    LISA_LOGI(LOG_TAG, "=== Example completed ===");

exit:
    /* 主循环 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        print_netdev_list();
    }

    return 0;
}
