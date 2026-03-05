#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "arcs_ap_base.h"
#include "cli_main.h"
#include "spiflash.h"

#include "lisa_bluetooth.h"
#include "bt_app_if.h"
#include "bt_stack_if.h"

#include "hogpd_msg.h"
#include "hogpd.h"
#include "bass.h"
#include "diss.h"
#include "netcfg_bles.h"

#include "FreeRTOS.h"
#include "task.h"
#include "nvs.h"
#include "shell.h"

#define TAG "app-wifi"
#include "lisa_log.h"

#include "lisa_shell.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "wifi_manager/wifi_manager.h"
#if CONFIG_LISA_WIFI
#include "lisa_wifi.h"
#endif
#include "ls_event.h"
#include "ls_wifi_type.h"
#include "net_al.h"
#include "net_def.h"

#define TARGET_WIFI_SSID "listenai"
#define TARGET_WIFI_PWD "listenai"

#define WIFI_MGR_SEARCH_AP_BUFFER_SIZE 10
#define WIFI_MGR_AUTO_CONNECT_INTERVAL_MS 2000

static mac_manager_t *m_mac_manager;
static wifi_mgr_sta_config_t list[WIFI_MGR_SEARCH_AP_BUFFER_SIZE] = { 0 };

#if CFG_NVS
#define NVDS_FLASH_ADDRESS_OFFSET   (0xFF8000) //The last 32KB of 16B flash
#define NVDS_FLASH_SIZE      (0x8000) //32KB

struct nvs_fs arcs_nvs_fs;
FLASH_DEV arcs_flash_dev  = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xFF, //divider is 1
    .run_mod = RUN_WITHOUT_INT,
    .timeout = 0x180000
};

int arcs_nvs_init(void)
{
    struct flash_pages_info info;

    flash_if_init(&arcs_flash_dev, 0, 0);

    //init nvs module
    arcs_nvs_fs.offset = NVDS_FLASH_ADDRESS_OFFSET;
    arcs_nvs_fs.flash_device = &arcs_flash_dev;
    flash_get_page_info_by_offs(&arcs_flash_dev, arcs_nvs_fs.offset, &info);
    arcs_nvs_fs.sector_size = info.size;
    arcs_nvs_fs.sector_count = NVDS_FLASH_SIZE/info.size;

    nvds_init(&arcs_nvs_fs);

    return 0;
}
#endif

static mac_manager_t *m_mac_manager;

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

    // wifi_mgr_storage_save_ap(&cfg);
    // wifi_mgr_auto_connect_start(&autoconn_cfg);
}

uint8_t manufacturer_data[18] = {
    0xab, 0x0a, 0xa1, 0xdc, 0xa8, 0x76, 0x83, 0x65, 0x73, 0x83, 0x72, 0x65, 0x82, 0x67, 0x83, 0x68, 0x00, 0x78,
};

ble_gap_cfg_t user_bt_stack_dev_cfg = {
    .addr = {{0x44, 0x55, 0x66, 0x03, 0x23, 0x20}, 0},
    .name_len = sizeof(DEVICE_NAME),
    .name = DEVICE_NAME,
    .appearance = GAP_APP_GENERIC_MEDIA_PLAYER, // hid_keyboard
    .iocap = GAP_IO_CAP_NO_INPUT_NO_OUTPUT,
    .auth = GAP_SEC_NOT_ENC,
    .pairing_mode = GAPM_PAIRING_LEGACY,
};

#if (ADV_USER_DATA)

uint8_t lisa_ble_gen_user_adv_data(uint8_t *p_data)
{
    uint8_t nb_uuid = 1;

    uint16_t uuids[1] = {HID_UUID};

    // Remaining Length
    uint8_t rem_len = LEGA_ADV_DATA_LEN - 3;

    uint8_t *p_buf = p_data;
    uint8_t length = 0;

    /// add Manufacturer specific
    *p_buf++ = sizeof(manufacturer_data) + 1;
    *p_buf++ = 0xff; // GAP_AD_TYPE_MANU_SPECIFIC_DATA;
    memcpy(p_buf, manufacturer_data, 18);
    p_buf += sizeof(manufacturer_data);
    length += (sizeof(manufacturer_data) + 2);

    // Sanity check
    ASSERT_ERR(rem_len >= LEGA_ADV_DATA_LEN - 3);

    // Get remaining space in the Advertising Data - 2 bytes are used for name length/flag
    rem_len -= length;

    // Check if additional data can be added to the Advertising data - 2 bytes needed for type and length
    if (rem_len > 2) {
        uint8_t dev_name_length = MIN(user_bt_stack_dev_cfg.name_len, (rem_len - 2));

        // Device name length
        *p_buf = dev_name_length + 1;
        // Device name flag (check if device name is complete or not)
        *(p_buf + 1) = (dev_name_length == user_bt_stack_dev_cfg.name_len)
                           ? 0x09
                           : 0x08; // GAP_AD_TYPE_COMPLETE_NAME : GAP_AD_TYPE_SHORTENED_NAME;
        // Copy device name
        memcpy(p_buf + 2, user_bt_stack_dev_cfg.name, dev_name_length);

        // Update advertising data length
        length += (dev_name_length + 2);
    }
    return length;
}
#endif

uint8_t adv_user_data[LEGA_ADV_DATA_LEN];
const uint8_t *lisa_bt_get_adv_data(uint8_t *len)
{
    *len = lisa_ble_gen_user_adv_data(adv_user_data);
    return adv_user_data;
}
extern uint16_t netcfg_bles_profile_set_cb(uint8_t conidx, uint8_t att_idx, uint16_t op, uint8_t *p_value);

/// Message callback handle from APP
static const netcfg_bles_cb_t netcfg_app_cb = {
    .cb_value_set = netcfg_bles_profile_set_cb,
};
const diss_cb_t user_bt_stack_ble_diss_msg_cb = {
    .cb_value_get = NULL, // dis_profile_get_cb,
};

/**
 * @brief 初始化自定义服务
 *
 * 注册 GATT 用户回调并添加服务到数据库。
 * 此函数应在协议栈初始化完成后调用。
 */
void app_ble_init_cmp(void)
{
    // enable bass service
    ble_bass_init();
    ble_bass_enable(0, bt_stack_vbat_percent_get());
    // enable net config.
    ble_netcfg_bles_init((netcfg_bles_cb_t *)&netcfg_app_cb);

    // enable diss.
    ble_diss_init((diss_cb_t *)&user_bt_stack_ble_diss_msg_cb);

#if BLE_VOICE_SIMULATOR
    // enable hid service
    uint8_t svc_features = HOGPD_CFG_KEYBOARD | HOGPD_CFG_MOUSE | HOGPD_CFG_PROTO_MODE | HOGPD_CFG_REPORT_NTF_EN;
    uint8_t report_char_cfg = HOGPD_CFG_REPORT_IN;
    hogpd_report_map_t report_map = {sizeof(hid_report_map), 0, (uint8_t *)hid_report_map};
    ble_hogpd_init(svc_features, report_char_cfg, (hogpd_cb_t *)&bt_stack_ble_hogpd_msg_cb, &report_map);
    ble_hogpd_enable(0);
#endif
}

int main(int argc, char **argv)
{
    int ret = 0;

    atcmd_init();

    arcs_nvs_init();

    ret = lisa_shell_init();
    if (ret != 0) {
        LOGI("Failed to initialize shell (error: %d)\n", ret);
    }

    user_mac_manager_init();

    // 注册 DHCP 状态回调
    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    lisa_wifi_ops_t ops = {
        .init_done = cb_lisa_wifi_init_done,
        .custom_mac = custom_get_wifi_mac,
    };
    lisa_wifi_init(&ops);

    // BLE 初始化
    lisa_bluetooth_init();

    // Start Advertising
    app_ble_adv_start(0, BLE_ADV_GEN);

    // 初始化AT命令处理系统
    at_cmd_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}

static int user_wifi_command(int argc, char *argv[])
{
    char cmd_str[256] = {0};

    for (int i = 1; i < argc; i++) {
        strncat(cmd_str, argv[i], sizeof(cmd_str) - strlen(cmd_str) - 1);
        if (i < argc - 1) {
            strncat(cmd_str, " ", sizeof(cmd_str) - strlen(cmd_str) - 1);
        }
    }

    if (cmd_str[0]) {
        wifi_cmd_handler(cmd_str, strlen(cmd_str) + 1);
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, wifi,
                 user_wifi_command, wifi commands);

                 
