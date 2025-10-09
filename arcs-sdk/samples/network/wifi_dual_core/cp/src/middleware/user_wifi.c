/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "user_wifi.h"
#include "ipc_master.h"
#include "ipc_master_utils.h"
#include "ls_event.h"
#include "wlif.h"
#include "cli_main.h"
#include "net_ip.h"
#include "dhcps.h"
// #include "net/net_common.h"
#include "wifi_api.h"
#include "net_al.h"
#include "shell_def.h"
#include "netif/dhcp_state.h"
#include "ic_lock.h"

#include "user_fs.h"
#include "sdmmc_init.h"
#include "lisa_kv.h"
#include "user_lisa_tag_def.h"

char *lisa_ssid = NULL;
char  nv_ssid[32+1] = {0};
char  config_ssid[32+1] = {0};
int lisa_channel = 0;
uint8_t *restored_ip_addr = NULL;
uint32_t global_ip = 0;

bool g_wifi_connected = false;

static bool m_wifi_pre_initialized = false;

/*---------------------------------------------
 * 任务函数：WiFiConfigTask
 * 参数：pvParameters - 接收SSID字符串
 * 功能：配置WiFi参数并保存到NVDS
 *--------------------------------------------*/
#include "rtos_al.h"

#define WIFI_CONFIG_TASK_PRIORITY     RTOS_TASK_PRIORITY(8)

void handle_wifi_save_ip() {
    net_if_t *net_if = net_if_get(WIFI_VIF_STA_IDX);
    if (net_if) {
        uint32_t ip = 0, mask = 0, gw = 0;
        net_if_get_ip(net_if, &ip, &mask, &gw);
        if (ip != 0 && ip != global_ip) {
            global_ip = ip;
            lisa_kv_set_blob(LISA_KV_DEV_IP_ADDR, (uint8_t *)&ip, 4);
        }
    }
}

void handle_wifi_got_lisa_ip() {
    restored_ip_addr = NULL;
    int len = 4;
    lisa_kv_get_blob(LISA_KV_DEV_IP_ADDR, &restored_ip_addr, &len);
    if (restored_ip_addr) {
        // CLOGI("restored ip addr: %d.%d.%d.%d\n", restored_ip_addr[0], restored_ip_addr[1], restored_ip_addr[2], restored_ip_addr[3]);
        set_restored_ip_addr(restored_ip_addr);
    }
}

void wifi_ssid_ip_restore() {
    lisa_kv_del(LISA_KV_DEV_WIFI_SSID);
    lisa_kv_del(LISA_KV_DEV_WIFI_CHANNEL);
    lisa_kv_del(LISA_KV_DEV_IP_ADDR);
}

void WiFiConfigTask(void *pvParameters)
{
    int err = 0;
    int ret = 0;
    int ret1 = 0;
    int lisa_channel = 0;
    char *lisa_ssid = (char *)pvParameters;
    // int len = 32;
    // uint8_t ssid[32 + 1] = {0};
    // uint8_t channel3;

    /*-------------------------------
     * WiFi配置处理流程
     *-----------------------------*/
    if (lisa_ssid != NULL) {
        // 1. 获取WiFi信道
        err = wifi_get_channel(&lisa_channel);
        if (err) {
            CLOGI("[ERROR] wifi_get_channel failed\n");
            goto task_exit;
        }
        // 2. 保存SSID
        // nvds_get(NVDS_TAG_WIFI_STA_SSID, &len, ssid);
        ret = lisa_kv_set_string(LISA_KV_DEV_WIFI_SSID, lisa_ssid);
        if (ret) {
            CLOGI("[ERROR] nvds_put ssid failed\n");
            goto task_exit;
        }
        // 3. 保存信道
        ret1 = lisa_kv_set_int(LISA_KV_DEV_WIFI_CHANNEL, lisa_channel);
        if (ret1) {
            CLOGI("[ERROR] nvds_put channel failed\n");
            goto task_exit;
        }
        CLOGI("[SUCCESS] WiFi配置已保存\n");
    }

task_exit:
    vTaskDelete(NULL);
}
// 创建任务的函数
void createWiFiConfigTask(char *ssid) {
    strcpy(config_ssid, ssid);
    if (xTaskCreate(
            WiFiConfigTask,       // 任务函数
            "WiFiConfig",        // 任务名称
            512,                // 栈大小（单位：字）
            (void *)config_ssid,        // 传递SSID作为参数
            WIFI_CONFIG_TASK_PRIORITY, // 优先级
            NULL                 // 任务句柄（不需要）
        ) != pdPASS) {
        CLOGI("Failed to create WiFi config task\n");
    }
}


/*---------------------------------------------
 *            任务函数：WiFiConfigTask end
 *--------------------------------------------*/

void wifi_ssid_ip_save() {
    if (get_ssid_config()) {
        CLOGI("wifi ssid save\n");
        createWiFiConfigTask(nv_ssid); // 使用内置SSID
    }
    CLOGI("wifi ip save\n");
    handle_wifi_save_ip();
}

extern int wifi_cli_exec_sta_auto_conn(void);
int wifi_event_cb(void *arg, event_module_t event_module,
                  int event_id, void *event_data)
{
    event_ap_sta_add_param_t *sta_add_param;
    event_ap_sta_del_param_t *sta_del_param;
    event_connect_fail_param_t *conn_fail_evt;
    event_disconnect_param_t *disc_evt;

    switch (event_id)
    {
        case EVENT_WIFI_INIT_DONE:
        CLOGI("event <%d %d>  wifi init done\n", event_module, event_id);

        //if sta_autoconn flag and ssid/pwd setted in flash, try to auto connect ap
        if (wifi_cli_exec_sta_auto_conn() == 0)
        {
            CLOGI("sta mode auto connect\n");
            break;
        }

        break;
        case EVENT_WIFI_CONNECTED:
        net_if_t *net_if;
        CLOGI("event <%d %d>  connected \n", event_module, event_id);
        wlif_netif_up(WIFI_VIF_STA_IDX, VIF_STA);
        net_if = net_if_get(WIFI_VIF_STA_IDX);
        if (!net_if->static_ip)
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        break;
        case EVENT_WIFI_GOT_IP:
        CLOGI("event <%d %d>  IP obtained \n", event_module, event_id);
        if (get_ssid_config()) {
            createWiFiConfigTask(nv_ssid);
        }
         handle_wifi_save_ip();
        g_wifi_connected = true;
        break;
        case EVENT_WIFI_STA_DHCP_FAIL:
        CLOGI("event <%d %d>  DHCP FAILED \n", event_module, event_id);
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        break;
        case EVENT_WIFI_DISCONNECT:
        g_wifi_connected = false;
        disc_evt = (event_disconnect_param_t *)event_data;
        CLOGI("event <%d %d>  disconnected:%d max retry reach %d \n", event_module, event_id, disc_evt->reason_code, disc_evt->max_retry_reach);
        wlif_netif_down(WIFI_VIF_STA_IDX);
        CLOGI("wlif_netif_down completed\n");
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        CLOGI("ls_dhcpc_stop completed\n");
        break;
        case EVENT_WIFI_SCAN_DONE:
        CLOGI("event <%d %d>  scan done \n", event_module, event_id);
        break;
        case EVENT_WIFI_STA_CONNECT_FAIL:
        conn_fail_evt = (event_connect_fail_param_t *)event_data;
        CLOGI("event <%d %d>  connect fail:%d max retry reach %d \n", event_module, event_id, conn_fail_evt->reason_code, conn_fail_evt->max_retry_reach);
        break;
        case EVENT_WIFI_AP_STARTED:
        CLOGI("event <%d %d>  ap_started \n", event_module, event_id);
        wlif_netif_up(WIFI_VIF_AP_IDX, VIF_AP);
        // start DHCPS
        ls_dhcps_start(WIFI_VIF_AP_IDX);
        break;
        case EVENT_WIFI_AP_STA_ADD:
        sta_add_param = (event_ap_sta_add_param_t *)event_data;
        CLOGI("event <%d %d>  ap_sta_add:%d\n", event_module, event_id, sta_add_param->sta_idx);
        break;
        case EVENT_WIFI_AP_STA_DEL:
        sta_del_param = (event_ap_sta_del_param_t *)event_data;
        CLOGI("event <%d %d>  ap_sta_del:%d \n", event_module, event_id, sta_del_param->sta_idx);
        break;
        case EVENT_WIFI_AP_STOPPED:
        CLOGI("event <%d %d>  ap_stopped \n", event_module, event_id);
        ls_dhcps_stop();
        wlif_netif_down(WIFI_VIF_AP_IDX);
        break;
        default:
        CLOGI("rx event <%d %d>\n", event_module, event_id);
        break;
    }

    return LS_OK;
}

void user_wifi_pre_init(void) {

    struct ipc_master_cb_tag ipc_cb = {
            .wifi_tx_data_cfm = wlif_tx_cfm,
            .wifi_rx_data_ind = wlif_rx_buf_forward,
            .indication_handler = ipc_indication_handler
    };

    ic_lock_init();
    ipc_master_init(&ipc_cb);
    m_wifi_pre_initialized = true;
}

void user_wifi_start(void) {

    ASSERT_ERR(m_wifi_pre_initialized);
    //48-38-b6-45-b5-18
    uint8_t mac_addr[6] = {0x48, 0x38, 0xb6, 0x45, 0xb5, 0x18};
    ipc_wifi_mac_pre_set(mac_addr);
    ipc_wifi_init();

    // register event
    ls_event_init();
    ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, wifi_event_cb, NULL);
}


void customer_wifi_event_start(int (*wifi_cb)(void *, event_module_t, int, void *)) {
    // register event
    ls_event_init();
    ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, wifi_cb, NULL);
}

void customer_wifi_start(void) {

    ASSERT_ERR(m_wifi_pre_initialized);

    ipc_wifi_init();
}


void user_shell_start(void) {
    shell_init(cli_shell_process);
}

/**
 * @brief 将 WiFi 信道转换为频率（MHz）
 * @param channel 信道号
 * @return 中心频率（MHz），如果信道无效则返回 -1
 */
int wifi_channel_to_freq(int channel) {
    if (channel >= 1 && channel <= 14) {
        return 2407 + 5 * channel;  // 2.4GHz 频段
    } else if (channel >= 36 && channel <= 196) {
        return 5000 + 5 * channel;  // 5GHz 频段（UNII-1/2/2e/3）
    } else if (channel >= 1 && channel <= 233) {
        return 5950 + 5 * channel;  // 6GHz 频段（Wi-Fi 6E）
    } else {
        return -1;  // 无效信道
    }
}

int user_wifi_connect(wifi_connect_cfg_t *config) {
    uint8_t ret = 0;
    int freq = 0;

    if (config == NULL) {
        return -1;
    }
    set_ssid_config(false);
    set_restored_ip_addr(NULL);
    lisa_ssid = NULL;
    lisa_channel = 0;
    memset(nv_ssid, 0, sizeof(nv_ssid));
    lisa_kv_get_string("wifi_ssid", &lisa_ssid);

    if (lisa_ssid == NULL) {
        CLOGI("nvds_get failed\n");
        set_ssid_config(true);
        strcpy(nv_ssid, config->ssid);
        goto connect;
    } else {
        CLOGI("get ssid: %s\n", lisa_ssid);
        if (strcmp(config->ssid, lisa_ssid) == 0) {
            CLOGI("ssid is the same,get nvs channel\n");
            lisa_kv_get_int(LISA_KV_DEV_WIFI_CHANNEL, &lisa_channel);
            if (ret || lisa_channel == 0) {
                CLOGI("nvds_get failed\n");
                goto connect;
            }
            CLOGI("get nvs channel: %d\n", lisa_channel);
            freq = wifi_channel_to_freq(lisa_channel);
            if (freq == -1) {
                CLOGI("invalid channel\n");
                goto connect;
            } else {
                CLOGI("channel to freq: %d\n", freq);
            }
            config->freq[0] = freq;
            handle_wifi_got_lisa_ip();
        } else {
            CLOGI("ssid is not same\n");
            set_ssid_config(true);
            strcpy(nv_ssid, config->ssid);
        }
    }
connect:
    ret = wifi_sta_connect(config);
    return ret;
}
