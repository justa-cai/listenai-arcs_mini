#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>

#include "arcs_ap_base.h"
#include "spiflash.h"

#include "lisa_bluetooth.h"
#include "ble_adv_data.h"
#include "lisa_ble_api.h"

#include "FreeRTOS.h"
#include "task.h"
#include "nvs.h"

#define TAG "netcfg"
#include "lisa_log.h"

#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "wifi_manager/wifi_manager.h"
#include "lisa_wifi.h"
#include "net_al.h"
#include "net_def.h"

/* ---------- NVS ---------- */

#if CFG_NVS
#define NVDS_FLASH_ADDRESS_OFFSET   (0xFF8000) // The last 32KB of 16MB flash
#define NVDS_FLASH_SIZE             (0x8000)    // 32KB

struct nvs_fs arcs_nvs_fs;
FLASH_DEV arcs_flash_dev = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width   = 4,
    .sclk_div  = 0xFF, // divider is 1
    .run_mod   = RUN_WITHOUT_INT,
    .timeout   = 0x180000,
};

static int arcs_nvs_init(void)
{
    struct flash_pages_info info;

    flash_if_init(&arcs_flash_dev, 0, 0);

    arcs_nvs_fs.offset       = NVDS_FLASH_ADDRESS_OFFSET;
    arcs_nvs_fs.flash_device = &arcs_flash_dev;
    flash_get_page_info_by_offs(&arcs_flash_dev, arcs_nvs_fs.offset, &info);
    arcs_nvs_fs.sector_size  = info.size;
    arcs_nvs_fs.sector_count = NVDS_FLASH_SIZE / info.size;

    nvds_init(&arcs_nvs_fs);
    return 0;
}
#endif

/* ---------- WiFi ---------- */

static mac_manager_t *m_mac_manager;

static int8_t custom_get_wifi_mac(uint8_t mac_addr[6])
{
    int8_t ret;

    if (!mac_addr)
        return -1;

    ret = mac_manager_get(m_mac_manager, mac_addr, 6);
    LOGI("custom_get_wifi_mac: %d, %02X:%02X:%02X:%02X:%02X:%02X",
         ret, mac_addr[0], mac_addr[1], mac_addr[2],
         mac_addr[3], mac_addr[4], mac_addr[5]);
    return ret;
}

static void dhcp_status_callback(int vif_idx, bool success,
                                 uint32_t ip_addr, uint32_t netmask,
                                 uint32_t gateway, void *arg)
{
    if (success) {
        LOGI("DHCP OK VIF-%d: IP=%d.%d.%d.%d",
             vif_idx,
             ip_addr & 0xff, (ip_addr >> 8) & 0xff,
             (ip_addr >> 16) & 0xff, (ip_addr >> 24) & 0xff);
    } else {
        LOGI("DHCP Failed VIF-%d", vif_idx);
    }
}

static void wifi_mgr_connection_cb(wifi_mgr_connection_info_t *info, void *arg)
{
    net_if_t *net_if;

    LOGI("WiFi connection status: %d", info->status);

    switch (info->status) {
    case WIFI_MGR_STA_CONNECTED:
        net_if = net_if_get(WIFI_VIF_STA_IDX);
        net_if_up(net_if);
        if (!net_if->static_ip)
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        break;

    case WIFI_MGR_STA_DISCONNECTED:
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
                                     mac_manager_ops_get()->content_ops,
                                     &config);
    assert(m_mac_manager);
}

static void user_wifi_manager_init(void)
{
    wifi_mgr_init(wifi_mgr_ops_get());
    wifi_mgr_sta_enable();
    wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_cb, NULL);
}

static void cb_lisa_wifi_init_done(void)
{
    LOGI("lisa_wifi_init_done");
    user_wifi_manager_init();
}

/* ---------- BLE netcfg handler ---------- */

/**
 * @brief WiFi connect handler invoked by BLE netcfg profile.
 *
 * Called when a peer device sends WiFi credentials via the BLE netcfg
 * characteristic. Demonstrates the dependency-inversion pattern: the
 * BLE component calls this application-provided callback instead of
 * invoking WiFi APIs directly.
 */
static int netcfg_wifi_connect_handler(const char *ssid, const char *pwd)
{
    wifi_mgr_sta_config_t cfg = {0};

    LOGI("BLE netcfg: connecting to '%s'", ssid);
    strncpy(cfg.ssid, ssid, sizeof(cfg.ssid) - 1);
    strncpy(cfg.pwd, pwd, sizeof(cfg.pwd) - 1);
    return wifi_mgr_sta_connect(&cfg, false);
}

/* ---------- BLE ---------- */

static const uint8_t user_adv_data[] = {
    BLE_AD_MFG_DATA(18,
        0xab, 0x0a, 0xa1, 0xdc, 0xa8, 0x76, 0x83, 0x65,
        0x73, 0x83, 0x72, 0x65, 0x82, 0x67, 0x83, 0x68,
        0x00, 0x78),
    BLE_AD_COMPLETE_NAME(4, 'A', 'R', 'C', 'S'),
};

const uint8_t *lisa_bt_get_adv_data(uint8_t *len)
{
    *len = sizeof(user_adv_data);
    return user_adv_data;
}

/* ---------- main ---------- */

int main(int argc, char **argv)
{
    arcs_nvs_init();
    user_mac_manager_init();

    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    lisa_wifi_ops_t ops = {
        .init_done  = cb_lisa_wifi_init_done,
        .custom_mac = custom_get_wifi_mac,
    };
    lisa_wifi_init(&ops);

    lisa_bluetooth_init(NULL);

    /* Register WiFi connect handler (dependency inversion) */
    lisa_ble_netcfg_set_handler(netcfg_wifi_connect_handler);

    lisa_ble_adv_start(0, LISA_BLE_ADV_GEN);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
