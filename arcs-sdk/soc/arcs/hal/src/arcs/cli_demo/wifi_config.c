/*
 * wifi_config.c
 *
 *  Created on: 2024-10-16
 */

/*
 * INCLUDES
 ****************************************************************************************
 */
#include <stdbool.h>          // standard boolean definitions
#include <stdint.h>           // standard integer functions
#include <string.h>

#include "log_print.h"
#include "ls_misc.h"
#include "wifi_api.h"
#include "ls_utils.h"
#include "ls_wifi_type.h"

#include "nvs.h"
#include "nvds_tag_def.h"
#include "rtos_al.h"
#include "ls_event.h"
#include "cli_main.h"
#include "net_al.h"
#include "net_ip.h"
/**
 ****************************************************************************************
 * @addtogroup DRIVERS
 * @{
 *
 *
 * ****************************************************************************************
 */

/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * STRUCTURE DEFINITIONS
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/*
 * LOCAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */

static void wifi_error_status_info(uint16_t erro, uint16_t status_code, uint16_t reason_code)
{
    switch(erro) {
        case WIFI_ERROR_STA_AUTH_FAIL:
            CLOGI("wifi auth fail \n");
            break;
        case WIFI_ERROR_STA_ASSOC_FAIL:
            CLOGI("wifi associate fail , statu code %d\n", status_code);
            break;
        case WIFI_ERROR_STA_CONNECT_NO_TARGET_AP:
            CLOGI("wifi connect fail, did not find targe AP \n");
            break;
        case WIFI_ERROR_STA_LINK_LOSS:
            CLOGI("wifi beacon lost \n");
            break;
        case WIFI_ERROR_WPA3_PWD_OR_AUTH_FAIL:
        case WIFI_ERROR_FOUND_SSID_BUT_KEY_MISMATCH:
             CLOGI("wifi password may wrong \n");
             break;
        case WIFI_ERROR_NO_FRAME_ALLC_FOR_AUTH_ASSO:
        case WIFI_ERROR_ADD_STA_FAIL:
            CLOGI("wifi connect fail caused by allocate fail \n");
            break;
        case WIFI_ERROR_AUTH_ASSOC_TIMEOUT:
            CLOGI("wifi auth/associate time out \n");
            break;
        case WIFI_ERROR_DEAUTH_BY_AP:
            CLOGI("Receive deauth from AP, reason code %d \n",reason_code);
            if (reason_code == 15)
                CLOGI("May password wrong \n");
            break;
        case WIFI_ERROR_DEAUTH_BY_LOCAL:
            CLOGI("wifi disconnect by local, reason code %d \n", reason_code);
            if (reason_code == 15)
                CLOGI("May password wrong \n");
            break;
        default:
            break;
    }
}

static int wifi_sta_auto_connect(void)
{
#if CFG_NVS
    uint8_t sta_auto_conn_en = 0;
    uint8_t ssid[WIFI_SSID_LEN + 1] = {0};
    uint8_t pwd[WIFI_PASSWORD_LEN + 1] = {0};
    uint32_t len, chan;
    int ret;
    wifi_connect_cfg_t sta_config = {0};
    uint8_t pmk[32];
    uint8_t pmk_set;

    len = NVDS_LEN_WIFI_STA_AUTOCONN;
    ret = nvds_get(NVDS_TAG_WIFI_STA_AUTOCONN, &len, &sta_auto_conn_en);
    if (ret == NVDS_OK && sta_auto_conn_en == 1)
    {
        len = WIFI_SSID_LEN;
        ret = nvds_get(NVDS_TAG_WIFI_STA_SSID, &len, ssid);
        if (ret == NVDS_OK && len <= WIFI_SSID_LEN)
        {
            memcpy(sta_config.ssid, ssid, len);
            len = WIFI_PASSWORD_LEN;
            ret = nvds_get(NVDS_TAG_WIFI_STA_PWD, &len, pwd);
            if (ret == NVDS_OK)
            {
                if (len > WIFI_PASSWORD_LEN)
                {
                    return -1;
                }
                memcpy(sta_config.key, pwd, len);
            }
            len = NVDS_LEN_WIFI_CHANNEL;
            ret = nvds_get(NVDS_TAG_WIFI_CHANNEL, &len, &chan);
            if (ret == NVDS_OK)
            {
                sta_config.freq[0] = sta_config.freq[1] = 2412 + (chan -1) * 5;
            }
            len = NVDS_LEN_WIFI_PMK_SET;
            nvds_get(NVDS_TAG_WIFI_PMK_SET, &len, &pmk_set);
            if (pmk_set) {
                len = NVDS_LEN_WIFI_PMK;
                nvds_get(NVDS_TAG_WIFI_PMK, &len, pmk);
                wifi_set_pmk(pmk);
            }
            len = NVDS_LEN_WIFI_BSSID;
            nvds_get(NVDS_TAG_WIFI_BSSID, &len, sta_config.bssid);
            ret = wifi_sta_connect(&sta_config);
            if (ret == LS_OK)
            {
                return 0;
            }
        }
    }
#endif
    return -1;
}


static int wifi_event_cb(void *arg, event_module_t event_module,
                  int event_id, void *event_data)
{
    event_ap_sta_add_param_t *sta_add_param;
    event_ap_sta_del_param_t *sta_del_param;
    event_disconnect_param_t *disc_evt;
    event_scan_done_param_t *scan_evt;
    net_if_t *net_if;

    switch (event_id)
    {
        case EVENT_WIFI_INIT_DONE:
        CLOGN("event <%d %d>  wifi init done\n", event_module, event_id);

        //if sta_autoconn flag and ssid/pwd setted in flash, try to auto connect ap
        if (wifi_sta_auto_connect() == 0)
        {
            CLOGV("sta mode auto connect\n");
            break;
        }

        break;
        case EVENT_WIFI_CONNECTED:
        CLOGN("event <%d %d>  connected \n", event_module, event_id);
        net_if = net_if_get(WIFI_VIF_STA_IDX);
        net_if_up(net_if);
        if (!net_if->static_ip)
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        break;
        case EVENT_WIFI_GOT_IP:
        uint8_t pmk[32];
        uint8_t pmk_set = 0;
        uint32_t chan = 0;
        struct wifi_link_status link_status;
        CLOGN("event <%d %d>  IP obtained \n", event_module, event_id);
        #if CFG_NVS
        if (!wifi_get_pmk(pmk)) {
            pmk_set = 1;
            nvds_del(NVDS_TAG_WIFI_PMK_SET);
            nvds_del(NVDS_TAG_WIFI_PMK);
            pmk_set = 1;
            nvds_put(NVDS_TAG_WIFI_PMK, NVDS_LEN_WIFI_PMK, pmk);
            nvds_put(NVDS_TAG_WIFI_PMK_SET, NVDS_LEN_WIFI_PMK_SET, &pmk_set);
        }
        if (!wifi_get_link_status(&link_status)) {
            chan = link_status.channel;
            nvds_del(NVDS_TAG_WIFI_BSSID);
            nvds_del(NVDS_TAG_WIFI_CHANNEL);
            nvds_put(NVDS_TAG_WIFI_CHANNEL, NVDS_LEN_WIFI_CHANNEL, &chan);
            nvds_put(NVDS_TAG_WIFI_BSSID, NVDS_LEN_WIFI_BSSID, link_status.bssid);
        }
        #endif
        break;
        case EVENT_WIFI_STA_DHCP_FAIL:
        CLOGI("event <%d %d>  DHCP FAILED \n", event_module, event_id);
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        break;
        case EVENT_WIFI_DISCONNECT:
        case EVENT_WIFI_STA_CONNECT_FAIL:
        disc_evt = (event_disconnect_param_t *)event_data;
        CLOGI("event <%d %d>  disconnected or connect fail, max retry reach %d \n", event_module, event_id, disc_evt->max_retry_reach);
        wifi_error_status_info(disc_evt->erro_code, disc_evt->status_code, disc_evt->reason_code);
        net_if = net_if_get(WIFI_VIF_STA_IDX);
        if (net_if && netif_is_up(net_if)) {
            ls_dhcpc_stop(WIFI_VIF_STA_IDX);
            net_if_down(net_if_get(WIFI_VIF_STA_IDX));
        }
        break;
        case EVENT_WIFI_SCAN_DONE:
        scan_evt = (event_scan_done_param_t *)event_data;
        CLOGI("event <%d %d>  scan done \n", event_module, event_id);
        if (!scan_evt->status)
            CLOGI("scan success, scan cnt %d  \n", scan_evt->result_cnt);
        break;

        case EVENT_WIFI_AP_STARTED:
        CLOGI("event <%d %d>  ap_started \n", event_module, event_id);
        net_if_up(net_if_get(WIFI_VIF_AP_IDX));
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
        net_if_down(net_if_get(WIFI_VIF_AP_IDX));
        break;
        default:
        CLOGI("rx event <%d %d>\n", event_module, event_id);
        break;
    }

    return LS_OK;
}

static void wifi_mgmt_frame_process_example(uint8_t *frame, uint32_t len, void *arg)
{
    struct wifi_mac_hdr *hdr;

    //hdr = (struct wifi_mac_hdr *)frame;
    //CLOGV("frame type %x len %d \n", hdr->fctl, len);
    // customer_cb(frame, len, arg, other para);
}
/*
 * MAIN FUNCTION
 ****************************************************************************************
 */



struct wifi_ops ops = {
   // .fw_log_level = 3,
    .get_mac = ls_get_wifi_mac,
    .temp_update = ls_temp_por_update,
};


static int wifi_pwr_tbl_set_example(void)
{
    int res = 0;
    struct pwr_table pwr={18,18,18,18,
                          16,16,16,16,16,16,16,16,
                          17,17,17,16,16,16,16,15,
                          17,17,17,16,16,16,16,15,15,15};
    int8_t type = CHAN_ALL;

    CLOGI("11b pwr:%d %d %d %d", pwr.pwr_11b[0],pwr.pwr_11b[1],pwr.pwr_11b[2],pwr.pwr_11b[3]);
    CLOGI("11g pwr: %d %d %d %d %d %d %d %d", pwr.pwr_11g[0],pwr.pwr_11g[1],pwr.pwr_11g[2],pwr.pwr_11g[3],pwr.pwr_11g[4],pwr.pwr_11g[5],pwr.pwr_11g[6],pwr.pwr_11g[7]);
    CLOGI("11n pwr: %d %d %d %d %d %d %d %d ", pwr.pwr_11n_ht20[0],pwr.pwr_11n_ht20[1],pwr.pwr_11n_ht20[2],pwr.pwr_11n_ht20[3],pwr.pwr_11n_ht20[4],pwr.pwr_11n_ht20[5],pwr.pwr_11n_ht20[6],pwr.pwr_11n_ht20[7]);
    CLOGI("11ax pwr: %d %d %d %d %d %d %d %d %d %d", pwr.pwr_11ax_he20[0], pwr.pwr_11ax_he20[1],pwr.pwr_11ax_he20[2],pwr.pwr_11ax_he20[3],pwr.pwr_11ax_he20[4],pwr.pwr_11ax_he20[5],pwr.pwr_11ax_he20[6],pwr.pwr_11ax_he20[7],pwr.pwr_11ax_he20[8],pwr.pwr_11ax_he20[9]);


    res = wifi_set_max_tx_pwr(&pwr, type);

    return res;
}

void ls_wifi_init(void)
{
    // register wifi event
    ls_event_init();
    ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, wifi_event_cb, NULL);

    wifi_mgmt_frame_cb_register(wifi_mgmt_frame_process_example, NULL);

    wifi_ops_register(&ops);
    wifi_init();

    //wifi_pwr_tbl_set_example();
}
