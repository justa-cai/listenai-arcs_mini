#ifndef __WIFI_API_H_
#define __WIFI_API_H_
// Copyright 2024-2025 ListenAI
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ls_wifi_type.h"

/**
 * @brief     This API register wifi ops
 *
 * @attention
 *
 * @params struct wifi_ops
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_ops_register(struct wifi_ops *ops);


/**
 * @brief     This API initializes the WiFi FW
 *
 * @attention
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_init(void);

/**
 * @brief     This API gets the wifi mode of the interface
 *
 * @attention
 *
 * @params  wifi_idx  wifi interface index
 * @params[out]  wifi mode,WIFI_MODE_STA/WIFI_MODE_AP/WIFI_MODE_MONITOR/WIFI_MODE_NULL
 *
 * @return
 *    - LS_OK: succeed
 *    - others: Fail or other error
 */
ls_err_t wifi_get_mode(wifi_vif_idx_e wifi_idx, wifi_mode_e *mode);

/**
 * @brief     This API enable sta mode
 *
 * @attention
 *
 * @params
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: Fail or other error
 */
ls_err_t wifi_sta_mode_enable(void);

/**
 * @brief     This API disable sta mode
 *
 * @attention
 *
 * @params
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: Fail or other error
 */
ls_err_t wifi_sta_mode_disable(void);
/**
 * @brief     This API connect wifi sta interface to the AP
 *
 * @attention
 *
 * @params    pointer of struct wifi_connect_cfg_t
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sta_connect(wifi_connect_cfg_t *config);

/**
 * @brief     This API disconnect the WIFI connection
 *
 * @attention
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sta_disconnect(void);

/**
 * @brief     This API enable auto reconnect in supplicant
 *
 * @attention
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sta_auto_reconnect_enable(void);

/**
 * @brief     This API disable auto reconnect in supplicant
 *
 * @attention
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sta_auto_reconnect_disable(void);

/**
 * @brief     This API start wifi scan
 *
 * @attention
 *
 * @params    pointer of struct wifi_scan_params_t
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_scan_start(wifi_scan_params_t *config);

/**
 * @brief     This API show the scan results
 *
 * @attention
 *
 * @params    scan_results-- get scan buffer pointer
 * @params    cnt   AP number in scan results
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_get_scan_result(wifi_scan_result_t **scan_results, int8_t *cnt);

/**
 * @brief     This API get the scan results AP number
 *
 * @attention
 *
 * @params ap_num : ap number variable pointer
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_get_sta_scanlist_nums(int *ap_num);

/**
 * @brief     This API dump the scan results
 *
 * @attention
 *
 * @params    results -- pointer of dump buffer
 * @params    tgt_num -- the number of scaned AP expected to be saved in the dump buffer
 * @params    rel_num -- the number of scaned AP be saved in realilty
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sta_scanlist_dump(wifi_scan_result_t *results, int tgt_num, int *rel_num);

/**
 * @brief     This API get AP rssi info
 *
 * @attention
 *
 * @params  rssi value
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_get_ap_rssi(int *rssi);

/**
 * @brief     This API is to get the association id assigned to STA by AP
 *
 * @attention
 *
 * @params[out] aid store the aid
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sta_aid_get(uint16_t *aid);

/**
 * @brief     This API is to get current operating channel
 *
 * @attention
 *
 * @params   channel number
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_get_channel(int *channel);

/**
 * @brief     This API gets STA interface link status
 *
 * @attention
 *
 * @params  link_status  pointer of wifi link status struct
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_get_link_status(struct wifi_link_status *link_status);

/**
 * @brief     This API starts soft AP mode
 *
 * @attention
 *
 * @params   pointer of struct wifi_ap_cfg_params_t
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_ap_start(const wifi_ap_cfg_params_t *config);

/**
 * @brief     This API stops soft AP mode
 *
 * @attention
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_ap_stop(void);

/**
 * @brief     This API gets SAP interface basic information
 *
 * @attention
 *
 * @params  ap_info  pointer of softap information struct
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_ap_get_basic_info(wifi_ap_info_t *ap_info);

/**
 * @brief     This API is to set country code
 *
 * @attention 1 support country code "CN" "US" "EU" "JP"
 * @attention 2 the default country code in WiFi FW is CN
 *
 * @params  country_code
 *
 * @return
 *    - LS_OK: succeed
 *    - LS_FAIL: failed
 */
ls_err_t wifi_set_country_code(char *country_code);

/**
 * @brief     This API is to get country code
 *
 * @attention
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_get_country_code(char *country_code);

/**
 * @brief     This API enable power
 *
 * @attention
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sta_ps_enter(void);

/**
 * @brief     This API disable power save
 *
 * @attention
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sta_ps_exit(void);


/**
 * @brief     This API set listen interval
 *
 * @attention
 *
 * @params   listen interval value (value < 20)
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sta_set_listen_itv(uint8_t listen_itv);

/**
 * @brief     This API gets listen interval value
 *
 * @attention
 *
 * @params[out] listen_itv   listen interval value
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sta_get_listen_itv(uint8_t *listen_itv);

/**
 * @brief     This API set keep alive time for sta mode
 *
 * @attention the default keep alive time in FW is 30s
 *
 * @params     time_seconds  the keep alive time (unit: seconds)
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sta_keepalive_time_set(uint8_t time_seconds);

/**
 * @brief     This API is to set vendor element for sta mode.
 *
 * @attention the vendor element would be added in following probe_req and assoc_req frame.
 *             ie length is limited to a maximum 48 byte, it can be changed by fw update
 *
 * @params    ie      pointer of the ie
 * @params    ie_len  length of the ie
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sta_set_vendor_ie(char *ie, int ie_len);

/**
 * @brief     This API is to clear vendor element for sta mode.
 *
 * @attention
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sta_del_vendor_ie(void);

/**
 * @brief     This API is to set vendor element for ap mode.
 *
 * @attention  shall call this api after ap mode started.
 *             the vendor element would be added in beacon,probe_rsp and assoc_rsp frame.
 *             ie length is limited to a maximum 48 byte, it can be changed by fw update
 *
 * @params    ie      pointer of the ie
 * @params    ie_len  length of the ie
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_ap_set_vendor_ie(char *ie, int ie_len);

/**
 * @brief     This API is to clear vendor element for ap mode.
 *
 * @attention shall call this api before ap mode closed.
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_ap_del_vendor_ie(void);

/**
 * @brief     This API set max number of sta could be supported for soft AP
 *
 * @attention max sta number should be set before soft AP start
 *
 * @params    max sta number
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_ap_max_sta_num(uint8_t max_sta_supported);

/**
 * @brief     This API is to delelte sta connected to soft AP
 *
 * @attention
 *
 * @params    sta index which to be deleted, get by EVENT_WIFI_AP_STA_ADD or wifi_ap_get_sta_info
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_ap_sta_delete(uint8_t sta_idx);

 /**
 * @brief     This API is to get sta connected status for sta mode
 *
 * @attention
 *
 * @params[out] link_status
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_get_sta_state(int *link_status);

 /**
 * @brief     This API is to get whether soft ap has started or not
 *
 * @attention
 *
 * @params[out] ap_state
 *
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_get_ap_state(int *ap_state);

 /**
 * @brief     This API is to get connected sta info in Soft AP mode
 *
 * @attention
 *
 * @params    pointer of struct  wifi_sta_basic_info
 *
 * @params    sta index
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_ap_get_sta_info(wifi_sta_basic_info_t *sta_info, uint8_t sta_idx);

 /**
 * @brief     This API set STA/AP interface mac address
 *
 * @attention
 *            1.set mac address should after initialize done
 *            2.set mac address should before STA connecting
 *            3.set mac address should before softap start
 *
 * @params    Array of mac address
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_mac_set(uint8_t mac[6]);

 /**
 * @brief     This API is to get sta interface mac address
 *
 * @attention  get mac address should after initialize done
 *
 * @params    Array of mac address
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_get_sta_mac(uint8_t mac[6]);

 /**
 * @brief     This API is to get AP mode interface mac address
 *
 * @attention  get mac address should after initialize done
 *
 * @params  Array of mac address
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_get_ap_mac(uint8_t mac[6]);

/**
 * @brief     This API enable sniffer mode
 *
 * @attention
 *
 * @params  sniffer parameter struct
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sniffer_enable(wifi_sniffer_para_t *sniffer_item);

/**
 * @brief     This API set channel for sniffer mode
 *
 * @attention Only support 20Mhz for BW
 *
 * @params chan_num: tar get channle index
 * @params band: current working band
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sniffer_set_chan(uint8_t chan_num, enum wifi_band band);

/**
 * @brief     This API disable sniffer mode
 *
 * @attention
 *
 * @params  sniffer parameter struct
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sniffer_disable(wifi_sniffer_para_t *sniffer_item);

/**
 * @brief     This API set fix rate for sta mode
 *
 * @attention set rate should be in connection state
 *
 * @params    rate: refer rate define in wifi_phy_rate_e
 *                  auto rate 0xff
 *                  11B 1Mbps ~ 11Mbps long preamble : 0x0 ~ 0x3"
 *                  11B 2Mbps ~ 11Mbps short preamble : 0x4 ~ 0x6"
 *                  11G 6Mbps ~ 54Mbps  : 0x10 ~ 0x7"
 *                  MCS0 ~ MCS9 (Long GI or 11ax 1.6 us GI) : 0x20 ~ 0x29
 *                  MCS0 ~ MCS9 (Short GI or 11ax 0.8 us GI) : 0x30 ~ 0x39
 *                  MCS0 ~ MCS9 ( 11ax 3.2 us GI) : 0x40 ~ 0x49
 *
 * @return
 *    - LS_OK: succeed
 *    - LS_FAIL: setting rate is invalid, or not in connection state
 */
ls_err_t wifi_rate_config(wifi_phy_rate_e rate);

/**
 * @brief     This API update the max tx power for different rate and channel
 *
 * @attention 1. the default transmit power table is like belw, customer could modify the table by below API
 *                pwr[] = {18, 18, 18, 18,                            //11b rate (1/2/5.5/11 Mbps) power
 *                         18, 18, 18, 18, 18, 18, 17, 17,            //11g rate (6/../54 Mbps) power
 *                         17, 17, 17, 16, 16, 16, 16, 15,            // 11n rate (mcs0~7)power
 *                         17, 17, 17, 16, 16, 16, 16, 15, 15, 15     // 11ax rate (mcs0~9)power
 *                        }
 * @attention 2. chan_type ALL_CHAN:  means power table for all channels
 *               chan_type LOW_CHAN:  means power table for low channels
 *               chan_type MID_CHAN:  means power table for mid channels
 *               chan_type HIGH_CHAN: means power table for high channels
 * @params   pwr  pointer of power table struct
 * @params   chan_type   CHAN_ALL/LOW_CHAN/MID_CHAN/HIGH_CHAN
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_set_max_tx_pwr(struct pwr_table *pwr, int8_t chan_type);


/**
 * @brief     This API get max tx power table current using
 *
 * @attention
 *
 * @params pwr  pointer of power table struct
 * @params chan_type   CHAN_ALL/LOW_CHAN/MID_CHAN/HIGH_CHAN
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_get_max_tx_pwr(struct pwr_table *pwr, int8_t chan_type);


/**
 * @brief     This API send 802.11 frame
 *
 * @attention 1.can be used in sta mode, softap mode and sniffer mode.
 * @attention 2.the frame sequence will be overwritten by WiFi IP.
 * @attention 3.caller need to guarantee the correctness of the frame.
 *
 * @params tx_info : 1. tx information, refer to wifi_80211_tx_info_t.
                     2. use wifi_vif_idx to indicate current wifi mode which be used to send the frame.
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_send_80211_frame(wifi_80211_tx_info_t *tx_info);

/**
 * @brief     twt setup
 *
 * @attention
 *
 * @param     setup_type
 *            1: suggest type , STA provides the TWT parameters
              2: demand type, uses AP's TWT paramter
 * @param     mantissa   TWT Wake Interval Mantissa
 *            wake interval = mantissa * 1024 us (exponent:10) = mantissa (ms)
 * @param     min_twt   Nominal Minimum TWT Wake Duration, max value 255, unit ms
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors
 */
ls_err_t wifi_twt_setup(uint8_t setup_type, uint16_t mantissa, uint8_t min_twt);
/**
 * @brief    twt teardown
 *
 * @attention
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors
 */
ls_err_t wifi_twt_teardown(void);

/**
 * @brief     This API set wifi fw/supplicant log level to enable/disable log output for debug purpose
 *
 * @attention Call this API after  wifi init done event received
 *
 * @params  fw_filter_module
 *          BIT0 ~ BIT10: KE/DBG/IPC/DMA/MM/TX/RX/PHY/SM/FHOST/ME
 * @params  fw_filter_severity
 *          0~5: none/CRT/ERR/WAR/INFO/VRB
 *          log severity level < setting level could be print out
 * @params  wpa_dbg_level
 *          0~5: MSG_EXCESSIVE, MSG_MSGDUMP, MSG_DEBUG, MSG_INFO, MSG_WARNING, MSG_ERROR
 *          log level > setting level could be print out
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_dbg_level_set(uint16_t fw_filter_module, uint8_t fw_filter_severity, uint8_t wpa_dbg_level);

/**
 * @brief     This API to clear wifi event id
 * *
 * @params wifi_event_id_e : wifi event id
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_event_clear(wifi_event_id_e id);

/**
 * @brief     This API to wait wifi event done
 * *
 * @params wifi_event_id_e : wifi event id
 *         timeout_ms : timeout value to wait the event, in milliseconds
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_event_wait(wifi_event_id_e id, uint32_t timeout_ms);

/**
 * @brief     This API to get wifi lib information string
 *
 * @params ver
 *         Buffer to store version informatioin
 * @params size
 *         Buffer size
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_ls_mac_version(uint8_t *ver, uint32_t size);

/**
 * @brief     This API is to restart wifi.
 *
 * @attention shall use this api after wifi_stop was called.
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors
 */
ls_err_t wifi_on(void);

/**
 * @brief     This API is to turn off wifi.
 *
 * @attention
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors
 */
ls_err_t wifi_off(void);
/**
 * @brief     This API is to release rx buffer.
 *
 * @attention
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors
 */
ls_err_t wifi_free_rx_buff(void);

/**
 * @brief     This API is to reinit rx buffer.
 *
 * @attention
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors
 */
ls_err_t wifi_reinit_rx_buff(void);

/**
 * @brief     This API is to get pmk after connect success
 *
 * @attention
 *
 * @params
 *    - pmk: wpa/wpa2 pmk
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors
 */
ls_err_t wifi_get_pmk(uint8_t pmk[32]);

/**
 * @brief     This API is to set pmk for sta mode
 *
 * @attention
 *
 * @params
 *    - pmk:  wpa/wpa2 pmk
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors
 */
ls_err_t wifi_set_pmk(uint8_t pmk[32]);

/**
 * @brief     This API is to calculate pmk by ssid/passphrase directly, would take nearly 100ms
 *
 * @attention
 *
 * @params
 *    - ssid: input ssid stream
 *    - ssid_len: input ssid str lenght
 *    - passphrase: input passphrase
 *    - passphrase_len: input length of passphrase
 *    - pmk:  output wpa/wpa2 pmk
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors
 */
ls_err_t wifi_calc_pmk(uint8_t *ssid, uint8_t ssid_len, uint8_t *passphrase, uint8_t passphrase_len, uint8_t *pmk);

/**
 * @brief     This function callback for user to handle received management frame
 *
 * @attention
 *    - these manage frame did not be filtered by wifi driver, user should process the frame they interested.
 *    - don't do any modify to the incoming frame or free the frame, the frame also should be processed by upper layer, like wpa supplicant
 * @params
 *    - frame: received management frame
 *    - len: length of frame
 *    - param: specified by user
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors
 */
typedef void (* wifi_mgmt_frame_cb_func)(uint8_t *frame, uint32_t len, void *param);

/**
 * @brief     This API register customer specified callback to process mgmt frame
 *
 * @attention
 *
 * @params
 *    - wifi_mgmt_frame_cb_func: callback function
 *    - param: specified by user
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors
 */
ls_err_t wifi_mgmt_frame_cb_register(wifi_mgmt_frame_cb_func cb, void *param);

/**
 * @brief     This API is to set ps mode.
 *
 * @attention
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors
 */
ls_err_t wifi_ps_mode_set(wifi_ps_mode_e mode);


/**
 * @brief     This API is to block ps.
 *
 * @attention
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors
 */
ls_err_t wifi_ps_lock_acquire(uint32_t lock, bool force);

/**
 * @brief     This API is to unblock ps.
 *
 * @attention
 *
 * @params
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors
 */
ls_err_t wifi_ps_lock_release(uint32_t lock);

/**
 * @brief     This API set dont_wait_bcmc
 *
 * @attention
 *
 * @params   dont_wait_bcmc value
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sta_set_dont_wait_bcmc(uint8_t dont_wait_bcmc);

/**
 * @brief     This API gets dont_wait_bcmc
 *
 *
 * @attention
 *
 * @params[out] listen_itv   dont_wait_bcmc value
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors.
 */
ls_err_t wifi_sta_get_dont_wait_bcmc(uint8_t *dont_wait_bcmc);

/**
 * @brief     This API is switch the DPD calibration tracking on STA connect or AP start.
 *
 * @attention
 *
 * @params
 *    - en 1: enable DPD tracking 0: disable DPD tracking
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors
 */
ls_err_t wifi_dpd_track_connect_switch(uint8_t en);

/**
 * @brief     This API is to set the debug level of PS module.
 *
 * @attention
 *
 * @params
 *    - level :
 *
 * @return
 *    - LS_OK: succeed
 *    - others: other errors
 */
ls_err_t wifi_ps_dbg_level_set(uint8_t level);
#endif
