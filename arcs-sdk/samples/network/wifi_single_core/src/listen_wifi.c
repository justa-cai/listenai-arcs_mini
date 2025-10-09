#include <string.h>
#include <stdbool.h>
#include "wlif.h"
#include "net_ip.h"
#include "net_al.h"
#include "cli_main.h"

#include "sysheap.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "listen_wifi.h"
#include "ls_event.h"
#include "ls_wifi_type.h"
#include "wifi_api.h"
#include "vrtc.h"
#include "rf_cali.h"

#if CONFIG_ARCS_HAL_MODULE_SHELL
#include "shell_def.h"
#endif

#define TAG "app-wifi"

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

    return 0;
}

static int _app_wifi_event_cb(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    event_ap_sta_add_param_t *sta_add_param;
    event_ap_sta_del_param_t *sta_del_param;
    event_connect_fail_param_t *conn_fail_evt;
    event_disconnect_param_t *disc_evt;

    switch (event_id) 
    {
        case EVENT_WIFI_INIT_DONE:
        LISA_LOGI(TAG, "EVENT_WIFI_INIT_DONE");
        break;
        case EVENT_WIFI_CONNECTED:
        LISA_LOGI(TAG, "EVENT_WIFI_CONNECTED");
        net_if_t *net_if;
        net_if = net_if_get(WIFI_VIF_STA_IDX);
        net_if_up(net_if);
        if (!net_if->static_ip)
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        break;
        case EVENT_WIFI_GOT_IP:
        LISA_LOGI(TAG, "EVENT_WIFI_GOT_IP");
        break;
        case EVENT_WIFI_STA_DHCP_FAIL:
        LISA_LOGI(TAG, "EVENT_WIFI_STA_DHCP_FAIL");
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        break;
        case EVENT_WIFI_DISCONNECT:
        disc_evt = (event_disconnect_param_t *)event_data;
        LISA_LOGI(TAG, "EVENT_WIFI_DISCONNECT");
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        net_if_down(net_if_get(WIFI_VIF_STA_IDX));
        break;
        case EVENT_WIFI_SCAN_DONE:
        LISA_LOGI(TAG, "EVENT_WIFI_SCAN_DONE");
        break;
        case EVENT_WIFI_STA_CONNECT_FAIL:
        conn_fail_evt = (event_connect_fail_param_t *)event_data;
        LISA_LOGI(TAG, "EVENT_WIFI_STA_CONNECT_FAIL, reason: %d", conn_fail_evt->reason_code);
        break;
        case EVENT_WIFI_AP_STARTED:
        LISA_LOGI(TAG, "EVENT_WIFI_AP_STARTED");
        net_if_up(net_if_get(WIFI_VIF_AP_IDX));
        // start DHCPS
        ls_dhcps_start(WIFI_VIF_AP_IDX);
        break;
        case EVENT_WIFI_AP_STA_ADD:
        sta_add_param = (event_ap_sta_add_param_t *)event_data;
        LISA_LOGI(TAG, "EVENT_WIFI_AP_STA_ADD, sta_idx: %d", sta_add_param->sta_idx);
        break;
        case EVENT_WIFI_AP_STA_DEL:
        sta_del_param = (event_ap_sta_del_param_t *)event_data;
        LISA_LOGI(TAG, "EVENT_WIFI_AP_STA_DEL, sta_idx: %d", sta_del_param->sta_idx);
        break;
        case EVENT_WIFI_AP_STOPPED:
        LISA_LOGI(TAG, "EVENT_WIFI_AP_STOPPED");
        ls_dhcps_stop();
        net_if_down(net_if_get(WIFI_VIF_AP_IDX));
        break;
        default:
        LISA_LOGI(TAG, "rx event <%d %d>", event_module, event_id);
        break;
    }
}

extern void ls_crypto_init(void);
extern uint8_t _sshram[], _eshram[];

void ls_wifi_init(void)
{
    memset(_sshram, 0, (_eshram - _sshram));
    int ret = 0;

#if CONFIG_ARCS_HAL_MODULE_SHELL
    shell_init(cli_shell_process);
#endif

    ls_rf_cali_proc();

    ls_crypto_init();

    ls_event_init();

    ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, _app_wifi_event_cb, NULL);

    wifi_init();

    vrtc_init();


}
