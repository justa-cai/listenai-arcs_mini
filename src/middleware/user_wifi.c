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
#include "wifi_api.h"
#include "net_al.h"
#include "shell_def.h"

bool g_wifi_connected = false;

static bool m_wifi_pre_initialized = false;

// extern int wifi_cli_exec_sta_auto_conn(void);
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

        break;
        case EVENT_WIFI_CONNECTED:
        net_if_t *net_if;
        CLOGI("event <%d %d>  connected \n", event_module, event_id);
        net_if = net_if_get(WIFI_VIF_STA_IDX);
        if (!net_if->static_ip)
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        break;
        case EVENT_WIFI_GOT_IP:
        CLOGI("event <%d %d>  IP obtained \n", event_module, event_id);

#if (TEST_NET_PROTOCOL & TEST_SNTP)
        sntp_test();
#endif
#if (TEST_NET_PROTOCOL & TEST_HTTP_HTTPS)
        http_test();
#elif (TEST_NET_PROTOCOL & TEST_WS_WSS)
        ws_test();
#endif

        g_wifi_connected = true;
        break;
        case EVENT_WIFI_STA_DHCP_FAIL:
        CLOGI("event <%d %d>  DHCP FAILED \n", event_module, event_id);
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        break;
        case EVENT_WIFI_DISCONNECT:
        g_wifi_connected = false;
        disc_evt = (event_disconnect_param_t *)event_data;
        CLOGI("event <%d %d>  disconnected:%d \n", event_module, event_id, disc_evt->reason_code);
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        break;
        case EVENT_WIFI_SCAN_DONE:
        CLOGI("event <%d %d>  scan done \n", event_module, event_id);
        break;
        case EVENT_WIFI_STA_CONNECT_FAIL:
        conn_fail_evt = (event_connect_fail_param_t *)event_data;
        CLOGI("event <%d %d>  connect fail:%d \n", event_module, event_id, conn_fail_evt->reason_code);
        break;
        case EVENT_WIFI_AP_STARTED:
        CLOGI("event <%d %d>  ap_started \n", event_module, event_id);
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

    ipc_master_init(&ipc_cb);
    m_wifi_pre_initialized = true;
}

void user_wifi_start(void) {

    ASSERT_ERR(m_wifi_pre_initialized);

    // ipc_wifi_init();

    // register event
    ls_event_init();
    ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, wifi_event_cb, NULL);

    
    shell_init(cli_shell_process);

}