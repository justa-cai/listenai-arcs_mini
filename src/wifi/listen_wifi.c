#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "wlif.h"
#include "net_ip.h"
#include "net_al.h"
#include "cli_main.h"
#include "ipc_master.h"
#include "ipc_master_utils.h"
#include "shell_def.h"

#include "sysheap.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "evs_utils.h"
#include "listen_wifi.h"
#include "ls_event.h"
#include "ls_misc.h"
#include "wifi_api.h"
#include "ls_wifi_type.h"
#include "rf_cali.h"

#include "assistant_controller.h"
#include "assistant_view.h"

#if CONFIG_WIFI_MANAGER
#include "wifi_manager/wifi_manager_ops.h"
#include "wifi_manager/wifi_manager.h"
#endif

#if CONFIG_MAC_MANAGER
#include "listen_mac_manager.h"
#endif

#include "lwip/dns.h"

#define TAG "app-wifi"

static bool s_wifi_statck_init_done = false;
static ls_wifi_stack_init_done_cb s_pre_init_done_cb = NULL;
static ls_wifi_t *s_handle = NULL;

static int _handle_wifi_got_ip(void *arg)
{
    net_if_t *net_if = net_if_get(WIFI_VIF_STA_IDX);
    if (net_if) {
        uint32_t ip, mask, gw;
        net_if_get_ip(net_if, &ip, &mask, &gw);

        char addr_str[16 + 1];
        ip4_addr_t addr;

        addr.addr = ip;
        if (ip4addr_ntoa_r(&addr, addr_str, 16) != NULL) {
            LISA_LOGI(TAG, "IP: %s", addr_str);
        }

        addr.addr = mask;
        if (ip4addr_ntoa_r(&addr, addr_str, 16) != NULL) {
            LISA_LOGI(TAG, "Mask: %s", addr_str);
        }

        addr.addr = gw;
        if (ip4addr_ntoa_r(&addr, addr_str, 16) != NULL) {
            LISA_LOGI(TAG, "Gateway: %s", addr_str);
        }
    }
    // add new dns server
    ls_wifi_refresh_dnsserver("114.114.114.114");

    if (s_handle && s_handle->m_cb) {
        s_handle->m_cb(LS_WIFI_STA_CONNECTED);
    }
    return 0;
}

static int _handle_wifi_disconnect(void *arg)
{
    if (s_handle && s_handle->m_cb) {
        s_handle->m_cb(LS_WIFI_STA_DISCONNECTED);
    }
    return 0;
}

static int _app_wifi_event_cb(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    ls_wifi_t *handle = s_handle;
    LISA_LOGI(TAG, "_app_wifi_event_cb event_id:%d",event_id);
    switch (event_id) {
    case EVENT_WIFI_INIT_DONE: {
        LISA_LOGD(TAG, "wifi init done");
        s_wifi_statck_init_done = true;
        // next can connect wifi
        if (s_pre_init_done_cb) {
            s_pre_init_done_cb();
            s_pre_init_done_cb = NULL;
        }
    } break;
    case EVENT_WIFI_CONNECTED: {
        LISA_LOGD(TAG, "wifi connected");
        if (handle) {
            handle->m_st = LS_WIFI_STA_AP_CONNECTED;
        }
        net_if_t *net_if = net_if_get(WIFI_VIF_STA_IDX);
        if (!net_if->static_ip) {
            // start dhcp
            net_if_up(net_if);
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        }
    } break;
    case EVENT_WIFI_GOT_IP: {
        LISA_LOGD(TAG, "Got IP");
        if (handle) {
            handle->m_st = LS_WIFI_STA_CONNECTED;
        }
        evs_handler_post_runnable(_handle_wifi_got_ip, NULL);
    } break;
    case EVENT_WIFI_STA_DHCP_FAIL: {
        LISA_LOGE(TAG, "wifi dhcp fail");
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
    } break;
    case EVENT_WIFI_DISCONNECT: {
        LISA_LOGD(TAG, "wifi disconnected");
        if (handle) {
            handle->m_st = LS_WIFI_STA_DISCONNECTED;
        }

        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        net_if_down(net_if_get(WIFI_VIF_STA_IDX));
    } break;
    case EVENT_WIFI_STA_CONNECT_FAIL: {
        LISA_LOGE(TAG, "wifi connect fail");
    } break;
    case EVENT_WIFI_AP_STARTED: {
        LISA_LOGD(TAG, "wifi ap started");
        // start DHCPS
        ls_dhcps_start(WIFI_VIF_AP_IDX);
    } break;
    case EVENT_WIFI_AP_STA_ADD: {
        LISA_LOGD(TAG, "wifi ap sta add");
    } break;
    case EVENT_WIFI_AP_STA_DEL: {
        LISA_LOGD(TAG, "wifi ap sta delete");
    } break;
    case EVENT_WIFI_AP_STOPPED: {
        LISA_LOGD(TAG, "wifi ap stoped");
        ls_dhcps_stop();
    } break;
    default: {
        LISA_LOGW(TAG, "[%s %d]unknow wifi event %d", __FUNCTION__,__LINE__,event_id);
    } break;
    }

    return LS_OK;
}

#if CONFIG_WIFI_MANAGER

static void wifi_mgr_connection_cb(wifi_mgr_connection_info_t *connection_info, void *arg)
{
    LISA_LOGI(TAG, "[%s %d]mgr connection status:%d,ssid:%s", 
        __FUNCTION__, __LINE__, connection_info->status, connection_info->sta_info->ssid);

    if (s_handle == NULL) {
        return;
    }

    s_handle->sta_connect.is_valid = true;
    s_handle->sta_connect.status = connection_info->status;
    if(connection_info->status == WIFI_MGR_STA_CONNECTED){
        memcpy(&s_handle->sta_connect.sta_cfg,connection_info->sta_info,sizeof(s_handle->sta_connect.sta_cfg));
        wifi_mgr_storage_save_ap_force(connection_info->sta_info);
    }

    for(int ii=0; ii< sizeof(s_handle->m_hotspot_list.hotspot)/sizeof(s_handle->m_hotspot_list.hotspot[0]);ii++){
        if((0 == strcmp(s_handle->sta_connect.sta_cfg.ssid,s_handle->m_hotspot_list.hotspot[ii].info.ssid))){
            if(s_handle->sta_connect.status == WIFI_MGR_STA_CONNECTED){
                s_handle->m_hotspot_list.hotspot[ii].state = LS_WIFI_HOTSPOT_STATE_CONNECTED;
            }
            else{
                s_handle->m_hotspot_list.hotspot[ii].state = LS_WIFI_HOTSPOT_STATE_DISCONNECT;
            }
        }
        else{
            s_handle->m_hotspot_list.hotspot[ii].state = LS_WIFI_HOTSPOT_STATE_DISCONNECT;
        }
    }
    
    view_wifi_info_t view_info;
    view_info.is_enable = wifi_mgr_sta_is_enable();
    snprintf(view_info.sta_cfg.ssid,sizeof(view_info.sta_cfg.ssid),s_handle->sta_connect.sta_cfg.ssid);
    snprintf(view_info.sta_cfg.bssid,sizeof(view_info.sta_cfg.bssid),s_handle->sta_connect.sta_cfg.bssid);
    snprintf(view_info.sta_cfg.pwd,sizeof(view_info.sta_cfg.pwd),s_handle->sta_connect.sta_cfg.pwd); 

    if(connection_info->status == WIFI_MGR_STA_CONNECTED){
        view_info.state = VIEW_WIFI_STATE_CONNECTED;
        assist_controller_trigger_event(CONTROLLER_EVENT_STATE_WIFI_CONNECTED, &view_info, sizeof(view_wifi_info_t));
    }
    else if(connection_info->status == WIFI_MGR_STA_CONNECTING){
        view_info.state = VIEW_WIFI_STATE_CONNECTING;
        assist_controller_trigger_event(CONTROLLER_EVENT_STATE_WIFI_CONNECTING, &view_info, sizeof(view_wifi_info_t));
    }
    else{
        view_info.state = VIEW_WIFI_STATE_DISCONNECTED;
        assist_controller_trigger_event(CONTROLLER_EVENT_STATE_WIFI_DISCONNECTED, &view_info, sizeof(view_wifi_info_t));

        evs_handler_post_runnable(_handle_wifi_disconnect, NULL);
    }
    
    
}

static int compare_wifi_hotspot_by_rssi(const void *a, const void *b) {
    const wifi_hotspot_info_t *ha = (const wifi_hotspot_info_t *)a;
    const wifi_hotspot_info_t *hb = (const wifi_hotspot_info_t *)b;
    return hb->info.rssi - ha->info.rssi; // 按 rssi 降序
}

static void wifi_mgr_scan_done_cb(wifi_mgr_scan_info_t *aps_info, int ap_num, void *arg){

    int num;
    if(s_handle == NULL){
        return;
    }
    view_wifi_info_t *p_view_info;
    uint32_t size = sizeof(view_wifi_info_t) + sizeof(wifi_mgr_scan_info_t) * ap_num;
    p_view_info = (view_wifi_info_t *)exram_malloc(4, size);
    if (!p_view_info) {
        LISA_LOGE(TAG, "[%s %d]Failed to allocate memory for ap_info", __FUNCTION__, __LINE__);
        return ;
    }
    p_view_info->is_enable = wifi_mgr_sta_is_enable();
    p_view_info->state = VIEW_WIFI_STATE_SCANNED;
    p_view_info->hotspot_list.count = ap_num;
    memcpy(p_view_info->hotspot_list.hotspots, aps_info, sizeof(wifi_mgr_scan_info_t) * ap_num);
  
    assist_controller_trigger_event(CONTROLLER_EVENT_STATE_WIFI_SCANNED, p_view_info, size);
    exram_free(p_view_info);
}

void ls_wifi_mgr_init(void)
{

    wifi_mgr_autoconn_config_t cnn_cfg = {
        .interval_ms = 3000,
    };

    wifi_mgr_init(wifi_mgr_ops_get());
    wifi_mgr_sta_enable();
    wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_cb, NULL);
    wifi_mgr_add_scan_done_cb(wifi_mgr_scan_done_cb, NULL);
    wifi_mgr_auto_connect_start(&cnn_cfg);
}
#endif

static int8_t get_wifi_mac(uint8_t mac_addr[6])
{
    int8_t ret = -1;

#if CONFIG_MAC_MANAGER
    uint8_t *p_mac_addr = listen_mac_manager_get();
    if(p_mac_addr){
        memcpy(mac_addr, p_mac_addr, 6);
        LISA_LOGI(TAG, "WiFi mac address:%02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        ret = 0;
    }
#else
    ret = ls_get_wifi_mac(mac_addr);
#endif

    return ret;
}

struct wifi_ops ops = {
    .get_mac = get_wifi_mac,
    .temp_update = ls_temp_por_update,
};

void ls_wifi_pre_init(ls_wifi_stack_init_done_cb pre_init_done)
{
    s_pre_init_done_cb = pre_init_done;
    int ret = 0;

    struct ipc_master_cb_tag ipc_cb = {.wifi_tx_data_cfm = wlif_tx_cfm,
                                       .wifi_rx_data_ind = wlif_rx_buf_forward,
                                       .indication_handler = ipc_indication_handler};
#if CONFIG_MAC_MANAGER
    listen_mac_manager_init();
#endif
    
    ls_rf_cali_proc();

    extern void ls_crypto_init(void);
    ls_crypto_init();

    ls_event_init();

    ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, _app_wifi_event_cb, NULL);

    wifi_ops_register(&ops);
    wifi_init();

    // shell_init(cli_shell_process);

    // init wifi manager
#if CONFIG_WIFI_MANAGER
    do{
        vTaskDelay(pdMS_TO_TICKS(10));
    }while(!s_wifi_statck_init_done);
    /*TODO: remove this delay*/
    vTaskDelay(pdMS_TO_TICKS(1000));
    ls_wifi_mgr_init();
#endif
}

ls_wifi_t *ls_wifi_init(ls_wifi_status_cb cb)
{
    if (s_handle) {
        return NULL;
    }

    ls_wifi_t *handle = (ls_wifi_t *)lisa_mem_alloc(sizeof(ls_wifi_t));
    if (!handle) {
        LISA_LOGE(TAG, "listen wifi malloc fail");
        return NULL;
    }

    memset(handle,0,sizeof(ls_wifi_t));

    // init done
    handle->m_st = LS_WIFI_NONE;
    // backup callback
    handle->m_cb = cb;

    s_handle = handle;
    return s_handle;
}

bool ls_wifi_is_connected()
{
    if (s_handle) {
        return (s_handle->m_st == LS_WIFI_STA_CONNECTED);
    }
    return false;
}

int ls_wifi_connect(const char *const ssid, const char *const passwd)
{
    if (s_handle) {
        if (s_wifi_statck_init_done) {
            int ret = 0;
            wifi_connect_cfg_t *config = inram_calloc(4, 1, sizeof(wifi_connect_cfg_t));
            strcpy(config->ssid, ssid);
            strcpy(config->key, passwd);

            LISA_LOGI(TAG, "will connect wifi: %s %s", ssid, passwd);

            ret = wifi_sta_connect(config);
            if (ret != 0) {
                LISA_LOGE(TAG, "connect wifi error %d", ret);
            }
            inram_free(config);

            return 0;
        } else {
            LISA_LOGE(TAG, "wifi not init done");
        }
    } else {
        LISA_LOGE(TAG, "wifi module not init");
    }
    return -1;
}

ls_wifi_status_t ls_wifi_get_status()
{
    if (s_handle) {
        return s_handle->m_st;
    }
    return LS_WIFI_NONE;
}

int ls_wifi_ap_start(const char *const ssid, const char *const passwd)
{
    if (s_handle) {
        if (s_wifi_statck_init_done) {
            int ret = 0;
            wifi_ap_cfg_params_t *config = inram_calloc(4, 1, sizeof(wifi_ap_cfg_params_t));
            strcpy(config->ssid, ssid);
            strcpy(config->pwd, passwd);
            config->channel = 1;
            config->sec = WIFI_SEC_WPA2_PSK;

            LISA_LOGI(TAG, "will start wifi ap: %s %s", ssid, passwd);

            ret = wifi_ap_start(config);
            if (ret != 0) {
                LISA_LOGE(TAG, "start wifi ap error %d", ret);
            }
            inram_free(config);

            return 0;
        } else {
            LISA_LOGE(TAG, "wifi not init done");
        }
    } else {
        LISA_LOGE(TAG, "wifi module not init");
    }
    return -1;
}

void ls_wifi_refresh_dnsserver(const char *const dns_srv)
{
    if (DNS_MAX_SERVERS >= 2) {
        ip_addr_t dns_ip_addr;
        if (ipaddr_aton(dns_srv, &dns_ip_addr)) {
            const ip_addr_t *dns_srv = dns_getserver(DNS_MAX_SERVERS - 1);
            if (dns_srv->addr != dns_ip_addr.addr) {
                LISA_LOGD(TAG, "%dth old dns server: %s", DNS_MAX_SERVERS, ipaddr_ntoa(dns_srv));

                // set new dns server
                dns_setserver(DNS_MAX_SERVERS - 1, &dns_ip_addr);

                dns_srv = dns_getserver(DNS_MAX_SERVERS - 1);
                LISA_LOGD(TAG, "%dth new dns server: %s", DNS_MAX_SERVERS, ipaddr_ntoa(dns_srv));
            }
        }
    }
}