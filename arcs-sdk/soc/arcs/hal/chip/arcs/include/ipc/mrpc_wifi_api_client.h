#ifndef __MRPC_WIFI_API_CLIENT_H__
#define __MRPC_WIFI_API_CLIENT_H__

ls_err_t wifi_init(void);

ls_err_t wifi_get_mode(wifi_vif_idx_e wifi_idx, wifi_mode_e * mode);

ls_err_t wifi_sta_mode_enable(void);

ls_err_t wifi_sta_mode_disable(void);

ls_err_t wifi_sta_connect(wifi_connect_cfg_t * config);

ls_err_t wifi_sta_disconnect(void);

ls_err_t wifi_sta_auto_reconnect_enable(void);

ls_err_t wifi_sta_auto_reconnect_disable(void);

ls_err_t wifi_scan_start(wifi_scan_params_t * config);

ls_err_t wifi_sta_scanlist_dump(wifi_scan_result_t * results, int tgt_num, int * rel_num);

ls_err_t wifi_get_scan_result(wifi_scan_result_t ** scan_results, int8_t * cnt);

ls_err_t wifi_get_sta_scanlist_nums(int * ap_num);

ls_err_t wifi_get_ipv4_addr(uint32_t * addr, uint32_t * mask, uint32_t * gw, uint32_t * dns);

ls_err_t wifi_get_ap_rssi(int * rssi);

ls_err_t wifi_sta_aid_get(uint16_t * aid);

ls_err_t wifi_get_channel(int * channel);

ls_err_t wifi_get_link_status(struct wifi_link_status * link_status);

ls_err_t wifi_ap_start(const wifi_ap_cfg_params_t * config);

ls_err_t wifi_ap_stop(void);

ls_err_t wifi_ap_get_basic_info(wifi_ap_info_t * ap_info);

ls_err_t wifi_set_country_code(char country_code[3]);

ls_err_t wifi_get_country_code(char country_code[3]);

ls_err_t wifi_sta_set_listen_itv(uint8_t listen_itv);

ls_err_t wifi_sta_get_listen_itv(uint8_t * listen_itv);

ls_err_t wifi_sta_keepalive_time_set(uint8_t time_seconds);

ls_err_t wifi_sta_set_vendor_ie(char * ie, int ie_len);

ls_err_t wifi_sta_del_vendor_ie(void);

ls_err_t wifi_ap_set_vendor_ie(char * ie, int ie_len);

ls_err_t wifi_ap_del_vendor_ie(void);

ls_err_t wifi_ap_max_sta_num(uint8_t max_sta_supported);

ls_err_t wifi_ap_sta_delete(uint8_t sta_idx);

ls_err_t wifi_get_sta_state(int * link_status);

ls_err_t wifi_get_ap_state(int * ap_state);

ls_err_t wifi_ap_get_sta_info(wifi_sta_basic_info_t * sta_info, uint8_t sta_idx);

ls_err_t wifi_mac_set(uint8_t mac[6]);

ls_err_t wifi_get_sta_mac(uint8_t mac[6]);

ls_err_t wifi_get_ap_mac(uint8_t mac[6]);

ls_err_t wifi_sniffer_enable(wifi_sniffer_para_t * sniffer_item);

ls_err_t wifi_sniffer_set_chan(uint8_t chan_num, enum wifi_band band);

ls_err_t wifi_sniffer_disable(wifi_sniffer_para_t * sniffer_item);

ls_err_t wifi_rate_config(wifi_phy_rate_e rate);

ls_err_t wifi_set_max_tx_pwr(struct pwr_table * pwr, int8_t chan_type);

ls_err_t wifi_get_max_tx_pwr(struct pwr_table * pwr, int8_t chan_type);

ls_err_t wifi_send_80211_frame(wifi_80211_tx_info_t * tx_info);

ls_err_t wifi_twt_setup(uint8_t setup_type, uint16_t mantissa, uint8_t min_twt);

ls_err_t wifi_twt_teardown(void);

ls_err_t wifi_dbg_level_set(uint16_t fw_filter_module, uint8_t fw_filter_severity, uint8_t wpa_dbg_level);

ls_err_t wifi_event_clear(wifi_event_id_e id);

ls_err_t wifi_event_wait(wifi_event_id_e id, uint32_t timeout_ms);

ls_err_t wifi_ls_mac_version(uint8_t * ver, uint32_t size);

ls_err_t wifi_mfg_exec(char * params, int32_t params_len);

ls_err_t wifi_on(void);

ls_err_t wifi_off(void);

ls_err_t wifi_free_rx_buff(void);

ls_err_t wifi_reinit_rx_buff(void);

ls_err_t ls_rf_cali_redo(int8_t ppa_cap);

ls_err_t wifi_dpd_track_connect_switch(uint8_t en);

ls_err_t wifi_ps_mode_set(wifi_ps_mode_e mode);

ls_err_t wifi_sta_set_dont_wait_bcmc(uint8_t dont_wait_bcmc);

ls_err_t wifi_sta_get_dont_wait_bcmc(uint8_t * dont_wait_bcmc);

ls_err_t wifi_ps_dbg_level_set(uint8_t level);

ls_err_t wifi_get_pmk(uint8_t pmk[32]);

ls_err_t wifi_set_pmk(uint8_t pmk[32]);

ls_err_t wifi_calc_pmk(uint8_t ssid[33], uint8_t ssid_len, uint8_t passphrase[65], uint8_t passphrase_len, uint8_t pmk[32]);


#endif