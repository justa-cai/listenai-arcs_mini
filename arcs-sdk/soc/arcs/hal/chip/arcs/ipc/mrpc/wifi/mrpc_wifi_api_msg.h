#ifndef __MRPC_WIFI_API_MSG_H__
#define __MRPC_WIFI_API_MSG_H__
#include <stdint.h>
#include "ls_err.h"
#include "ls_wifi_type.h"
#include "mrpc.h"

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_init_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_init_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    wifi_vif_idx_e wifi_idx;
} mrpc_wifi_get_mode_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    wifi_mode_e mode;
} mrpc_wifi_get_mode_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_sta_mode_enable_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_sta_mode_enable_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_sta_mode_disable_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_sta_mode_disable_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    wifi_connect_cfg_t config;
} mrpc_wifi_sta_connect_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_sta_connect_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_sta_disconnect_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_sta_disconnect_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_sta_auto_reconnect_enable_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_sta_auto_reconnect_enable_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_sta_auto_reconnect_disable_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_sta_auto_reconnect_disable_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    wifi_scan_params_t config;
} mrpc_wifi_scan_start_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_scan_start_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    wifi_scan_result_t * results;
    int tgt_num;
} mrpc_wifi_sta_scanlist_dump_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    int rel_num;
} mrpc_wifi_sta_scanlist_dump_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_get_scan_result_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    wifi_scan_result_t * scan_results;
    int8_t cnt;
} mrpc_wifi_get_scan_result_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_get_sta_scanlist_nums_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    int ap_num;
} mrpc_wifi_get_sta_scanlist_nums_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_get_ipv4_addr_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    uint32_t addr;
    uint32_t mask;
    uint32_t gw;
    uint32_t dns;
} mrpc_wifi_get_ipv4_addr_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_get_ap_rssi_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    int rssi;
} mrpc_wifi_get_ap_rssi_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_sta_aid_get_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    uint16_t aid;
} mrpc_wifi_sta_aid_get_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_get_channel_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    int channel;
} mrpc_wifi_get_channel_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_get_link_status_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    struct wifi_link_status link_status;
} mrpc_wifi_get_link_status_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    wifi_ap_cfg_params_t config;
} mrpc_wifi_ap_start_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_ap_start_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_ap_stop_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_ap_stop_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_ap_get_basic_info_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    wifi_ap_info_t ap_info;
} mrpc_wifi_ap_get_basic_info_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    char country_code[3];
} mrpc_wifi_set_country_code_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_set_country_code_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_get_country_code_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    char country_code[3];
} mrpc_wifi_get_country_code_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint8_t listen_itv;
} mrpc_wifi_sta_set_listen_itv_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_sta_set_listen_itv_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_sta_get_listen_itv_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    uint8_t listen_itv;
} mrpc_wifi_sta_get_listen_itv_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint8_t time_seconds;
} mrpc_wifi_sta_keepalive_time_set_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_sta_keepalive_time_set_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    char * ie;
    int ie_len;
    uint8_t data_buffer[];
} mrpc_wifi_sta_set_vendor_ie_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_sta_set_vendor_ie_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_sta_del_vendor_ie_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_sta_del_vendor_ie_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    char * ie;
    int ie_len;
    uint8_t data_buffer[];
} mrpc_wifi_ap_set_vendor_ie_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_ap_set_vendor_ie_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_ap_del_vendor_ie_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_ap_del_vendor_ie_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint8_t max_sta_supported;
} mrpc_wifi_ap_max_sta_num_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_ap_max_sta_num_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint8_t sta_idx;
} mrpc_wifi_ap_sta_delete_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_ap_sta_delete_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_get_sta_state_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    int link_status;
} mrpc_wifi_get_sta_state_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_get_ap_state_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    int ap_state;
} mrpc_wifi_get_ap_state_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint8_t sta_idx;
} mrpc_wifi_ap_get_sta_info_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    wifi_sta_basic_info_t sta_info;
} mrpc_wifi_ap_get_sta_info_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint8_t mac[6];
} mrpc_wifi_mac_set_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_mac_set_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_get_sta_mac_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    uint8_t mac[6];
} mrpc_wifi_get_sta_mac_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_get_ap_mac_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    uint8_t mac[6];
} mrpc_wifi_get_ap_mac_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    wifi_sniffer_para_t sniffer_item;
} mrpc_wifi_sniffer_enable_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_sniffer_enable_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint8_t chan_num;
    enum wifi_band band;
} mrpc_wifi_sniffer_set_chan_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_sniffer_set_chan_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    wifi_sniffer_para_t sniffer_item;
} mrpc_wifi_sniffer_disable_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_sniffer_disable_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    wifi_phy_rate_e rate;
} mrpc_wifi_rate_config_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_rate_config_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    struct pwr_table pwr;
    int8_t chan_type;
} mrpc_wifi_set_max_tx_pwr_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_set_max_tx_pwr_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    int8_t chan_type;
} mrpc_wifi_get_max_tx_pwr_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    struct pwr_table pwr;
} mrpc_wifi_get_max_tx_pwr_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    wifi_80211_tx_info_t tx_info;
} mrpc_wifi_send_80211_frame_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_send_80211_frame_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint8_t setup_type;
    uint16_t mantissa;
    uint8_t min_twt;
} mrpc_wifi_twt_setup_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_twt_setup_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_twt_teardown_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_twt_teardown_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint16_t fw_filter_module;
    uint8_t fw_filter_severity;
    uint8_t wpa_dbg_level;
} mrpc_wifi_dbg_level_set_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_dbg_level_set_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    wifi_event_id_e id;
} mrpc_wifi_event_clear_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_event_clear_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    wifi_event_id_e id;
    uint32_t timeout_ms;
} mrpc_wifi_event_wait_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_event_wait_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint8_t * ver;
    uint32_t size;
} mrpc_wifi_ls_mac_version_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_ls_mac_version_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    char * params;
    int32_t params_len;
    uint8_t data_buffer[];
} mrpc_wifi_mfg_exec_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_mfg_exec_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_on_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_on_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_off_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_off_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_free_rx_buff_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_free_rx_buff_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_reinit_rx_buff_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_reinit_rx_buff_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    int8_t ppa_cap;
} mrpc_ls_rf_cali_redo_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_ls_rf_cali_redo_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint8_t en;
} mrpc_wifi_dpd_track_connect_switch_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_dpd_track_connect_switch_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    wifi_ps_mode_e mode;
} mrpc_wifi_ps_mode_set_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_ps_mode_set_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint8_t dont_wait_bcmc;
} mrpc_wifi_sta_set_dont_wait_bcmc_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_sta_set_dont_wait_bcmc_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_sta_get_dont_wait_bcmc_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    uint8_t dont_wait_bcmc;
} mrpc_wifi_sta_get_dont_wait_bcmc_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint8_t level;
} mrpc_wifi_ps_dbg_level_set_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_ps_dbg_level_set_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_wifi_get_pmk_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    uint8_t pmk[32];
} mrpc_wifi_get_pmk_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint8_t pmk[32];
} mrpc_wifi_set_pmk_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_wifi_set_pmk_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint8_t ssid[33];
    uint8_t ssid_len;
    uint8_t passphrase[65];
    uint8_t passphrase_len;
} mrpc_wifi_calc_pmk_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    uint8_t pmk[32];
} mrpc_wifi_calc_pmk_resp_t;

#endif //__MRPC_WIFI_API_MSG_H__