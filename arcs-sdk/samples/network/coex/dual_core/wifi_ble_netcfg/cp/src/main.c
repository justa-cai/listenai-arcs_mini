#include "FreeRTOS.h"
#include "task.h"

#include "user_fs.h"
#include "lisa_kv.h"

#define TAG "user_wifi"
#include "lisa_log.h"
#include "lisa_wifi.h"
#include "ls_wifi_type.h"
#include "net_al.h"
#include "net_def.h"
#include "mac_manager_ops.h"
#include "shell.h"
#include "wifi_manager/wifi_manager.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "lisa_ble_api.h"

static mac_manager_t *m_mac_manager = NULL;

/**
 * @brief This function used to custom WiFi MAC address 
 * @warning This function is called by peer core in wifi dual core mode, 
 *          Don't do any slow operation here.
 */
static int8_t user_custom_mac(uint8_t mac_addr[6])
{
    int ret = mac_manager_get(m_mac_manager, mac_addr, 6);
    if (ret != 0) {
        LOGI("[user_wifi]mac_manager_get failed\n");
    }
    LOGI("set mac: %02X:%02X:%02X:%02X:%02X:%02X\n",
         mac_addr[0], mac_addr[1], mac_addr[2],
         mac_addr[3], mac_addr[4], mac_addr[5]);

    return ret;
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

static void dhcp_status_callback(int vif_idx, bool success, uint32_t ip_addr, uint32_t netmask, uint32_t gateway, void *arg)
{
    if (success) {
        LOGI("DHCP Success on VIF-%d: IP=%d.%d.%d.%d, Mask=%d.%d.%d.%d, GW=%d.%d.%d.%d",
             vif_idx,
             ip_addr & 0xff, (ip_addr >> 8) & 0xff, (ip_addr >> 16) & 0xff, (ip_addr >> 24) & 0xff,
             netmask & 0xff, (netmask >> 8) & 0xff, (netmask >> 16) & 0xff, (netmask >> 24) & 0xff,
             gateway & 0xff, (gateway >> 8) & 0xff, (gateway >> 16) & 0xff, (gateway >> 24) & 0xff);
    } else {
        LOGI("DHCP Failed on VIF-%d", vif_idx);
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

static int netcfg_wifi_connect_handler(const char *ssid, const char *pwd)
{
    wifi_mgr_sta_config_t cfg = {0};

    LOGI("BLE netcfg: connecting to '%s'", ssid);
    strncpy(cfg.ssid, ssid, sizeof(cfg.ssid) - 1);
    strncpy(cfg.pwd, pwd, sizeof(cfg.pwd) - 1);
    return wifi_mgr_sta_connect(&cfg, false);
}

static void user_wifi_manager_init(void)
{
    wifi_mgr_init(wifi_mgr_ops_get());
    wifi_mgr_sta_enable();
    wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_cb, NULL);

    /* Register WiFi connect handler for BLE netcfg IPC from AP */
    lisa_ble_netcfg_set_handler(netcfg_wifi_connect_handler);

    LOGI("WiFi manager initialized, waiting for BLE provisioning...");
}

static void cb_wifi_init_done(void)
{
    LOGI("[user_wifi]wifi init done\n");

    // Initialize file system and key-value storage
    user_fs_init();
    lisa_kv_init();

    // Initialize the user wifi manager (no auto-connect, wait for BLE provisioning)
    user_wifi_manager_init();

    // Start BLE advertising immediately for WiFi provisioning
    LOGI("Starting BLE advertising for WiFi provisioning...");
    lisa_ble_adv_start(0, LISA_BLE_ADV_GEN);
}

static void user_shell_start(void)
{
    lisa_shell_init();
}

int main(int argc, char **argv)
{
    user_mac_manager_init();

    // 注册 DHCP 状态回调
    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    lisa_wifi_ops_t ops = {
        .custom_mac = user_custom_mac,
        .init_done = cb_wifi_init_done,
    };

    lisa_wifi_init(&ops);
    user_shell_start();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    return 0;
}

static int user_wifi_command(int argc, char *argv[])
{
    char cmd_str[256] = {0};
    int offset = 0;

    for (int i = 1; i < argc && offset < (int)sizeof(cmd_str) - 1; i++) {
        offset += snprintf(cmd_str + offset, sizeof(cmd_str) - offset,
                           "%s%s", (i > 1) ? " " : "", argv[i]);
    }

    if (cmd_str[0]) {
        wifi_cmd_handler(cmd_str, strlen(cmd_str) + 1);
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, wifi,
                 user_wifi_command, wifi commands);
