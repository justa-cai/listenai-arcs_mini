/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int ls_err_t;

#define LS_OK 0

#define WIFI_VIF_DEFAULT_IDX 0

typedef enum {
    WIFI_MODE_NULL = 0,
    WIFI_MODE_STA,
} wifi_mode_e;

#define WIFI_SSID_LEN 32
typedef enum {
    WIFI_SEC_UNKNOWN = 0,
    WIFI_SEC_AUTO,
    WIFI_SEC_OPEN,
    WIFI_SEC_WEP,
    WIFI_SEC_WPA_PSK,
    WIFI_SEC_WPA2_PSK,
    WIFI_SEC_WPA_PSK_WPA2_PSK,
    WIFI_SEC_WPA_ENTERPRISE,
    WIFI_SEC_WPA3_SAE,
    WIFI_SEC_WPA2_PSK_WPA3_SAE,
} wifi_security_e;

typedef struct {
    char ssid[32];
} wifi_scan_params_t;

typedef struct {
    char ssid[32];
    uint8_t bssid[6];
    int rssi;
    int channel;
    wifi_security_e auth;
} wifi_scan_result_t;

typedef struct {
    uint8_t ssid[32];
    uint8_t key[64];
    uint8_t bssid[6];
    uint16_t freq[2];
    wifi_security_e sec;
    uint8_t failure_retry_cnt;
} wifi_connect_cfg_t;

struct wifi_link_status {
    uint8_t is_connected;
    uint8_t bssid[6];
    int rssi;
    int noise;
    char ssid[WIFI_SSID_LEN + 1];
    int channel;
    int state;
    uint16_t aid;
};

typedef struct wifi_link_status wifi_link_status_t;

typedef enum {
    STA_INACTIVE = 0,
    STA_IN_CONNECTING,
    STA_CONNECTED,
    STA_DISCONNECTED,
} wifi_link_state_t;

ls_err_t wifi_get_pmk(uint8_t *pmk);
ls_err_t wifi_set_pmk(const uint8_t *pmk);
ls_err_t wifi_get_link_status(wifi_link_status_t *link_status);
ls_err_t wifi_sta_connect(wifi_connect_cfg_t *cfg);
ls_err_t wifi_sta_disconnect(void);
ls_err_t wifi_sta_mode_enable(void);
ls_err_t wifi_sta_mode_disable(void);
ls_err_t wifi_get_mode(int vif_idx, wifi_mode_e *mode);
ls_err_t wifi_scan_start(wifi_scan_params_t *param);
ls_err_t wifi_sta_scanlist_dump(wifi_scan_result_t *results, int size, int *out_num);
ls_err_t wifi_get_sta_mac(uint8_t *mac_addr);

#ifdef __cplusplus
}
#endif
