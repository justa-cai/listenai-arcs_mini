#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "FreeRTOS.h"
#include "task.h"
 #include "queue.h"

#define TAG "app-wifi"
#include "lisa_log.h"

#include "lisa_kv.h"
#include "shell.h"
#include "lisa_shell.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "wifi_manager/wifi_manager.h"
#include "user_fs.h"
#include "lisa_wifi.h"
#include "net_al.h"
#include "net_def.h"

#define TARGET_WIFI_SSID "listenai"
#define TARGET_WIFI_PWD "listenai"

#define WIFI_MGR_SEARCH_AP_BUFFER_SIZE 10
#define WIFI_MGR_AUTO_CONNECT_INTERVAL_MS 2000
 
 #define WIFI_SAVE_AP_DELAY_MS 2000
 #define WIFI_SAVE_AP_QUEUE_LEN 1

static mac_manager_t *m_mac_manager;
static wifi_mgr_sta_config_t list[WIFI_MGR_SEARCH_AP_BUFFER_SIZE] = { 0 };

static QueueHandle_t s_save_ap_queue;

static void wifi_save_ap_worker(void *arg)
{
    (void)arg;
    wifi_mgr_sta_config_t ap;
    while (1) {
        if (xQueueReceive(s_save_ap_queue, &ap, portMAX_DELAY) == pdTRUE) {
            vTaskDelay(pdMS_TO_TICKS(WIFI_SAVE_AP_DELAY_MS));
            int ret = wifi_mgr_storage_save_ap(&ap);
            LOGI("save ap delayed: ret=%d, ssid=%s, bssid=%s, pmk_valid=%d", ret, ap.ssid, ap.bssid, ap.pmk_valid);
        }
    }
}

static void wifi_schedule_save_ap(const wifi_mgr_connection_info_t *connection_info)
{
    if (connection_info == NULL || connection_info->sta_info == NULL) {
        return;
    }

    wifi_mgr_sta_config_t ap = {0};
    memcpy(&ap, connection_info->sta_info, sizeof(ap));
    if (s_save_ap_queue != NULL) {
        // 这是耗时操作，为了不影响此次连接的速度，放到队列中异步处理
        (void)xQueueOverwrite(s_save_ap_queue, &ap);
    }
}

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

            // 如果需要快速连接，需要保存AP信息到存储，可以加快下次wifi_manager自动连接的速度
            wifi_schedule_save_ap(connection_info);
            break;

        case WIFI_MGR_STA_CONNECT_FAILED:
            LOGE("WiFi manager connect failed, reason: %d", connection_info->reason);
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

    if (s_save_ap_queue == NULL) {
        s_save_ap_queue = xQueueCreate(WIFI_SAVE_AP_QUEUE_LEN, sizeof(wifi_mgr_sta_config_t));
        if (s_save_ap_queue != NULL) {
            (void)xTaskCreate(wifi_save_ap_worker, "wifi_save_ap", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);
        }
    }

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

int main(int argc, char **argv)
{
    int ret = 0;

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


    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
