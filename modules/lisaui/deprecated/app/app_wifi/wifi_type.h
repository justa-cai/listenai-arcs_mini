/**
 * @file wifi_type.h
 * @author TrekMax (QinYUN575@Foxmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-25
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LISAUI_APP_WIFI_TYPE_H__

#define __LISAUI_APP_WIFI_TYPE_H__

#ifdef __cplusplus
extern "C" {
#endif

#define LISAUI_APP_WIFI_SSID_MAX_LEN  33
#define LISAUI_APP_WIFI_BSSID_MAX_LEN 32
#define LISAUI_APP_WIFI_PWD_MAX_LEN   65
#define LISAUI_APP_WIFI_LIST_ITEM_MAX 10

typedef enum {
    LISAUI_WIFI_STATUS_DISCONNECT = 0,
    LISAUI_WIFI_STATUS_CONNECTING,
    LISAUI_WIFI_STATUS_CONNECTED,
} lisaui_wifi_status_t;

typedef enum {
    LISAUI_WIFI_SECURITY_NONE = 0,
    LISAUI_WIFI_SECURITY_WEP,
    LISAUI_WIFI_SECURITY_WPA,
    LISAUI_WIFI_SECURITY_WPA2,
    LISAUI_WIFI_SECURITY_WPA3,
} lisaui_wifi_security_t;

typedef enum  {
    LISAUI_WIFI_ITEM_NONE = 0,
    LISAUI_WIFI_ITEM_AP,
    LISAUI_WIFI_ITEM_AP_LIST,
} lisaui_wifi_item_t;

typedef struct {
    int id;
    char SSID[LISAUI_APP_WIFI_SSID_MAX_LEN];
    char BSSID[LISAUI_APP_WIFI_BSSID_MAX_LEN];
    char PWD[LISAUI_APP_WIFI_PWD_MAX_LEN];
    int rssi;
    lisaui_wifi_security_t security;
    lisaui_wifi_status_t status;
} wifi_metadata_t;

typedef enum {
    LISAUI_WIFI_OP_SCAN = 0,
    LISAUI_WIFI_OP_GET_AP_LIST,
    LISAUI_WIFI_OP_GET_AP_INFO,

    LISAUI_WIFI_OP_SET_AP_INFO,
    LISAUI_WIFI_OP_ADD_AP_INFO,
    // LISAUI_WIFI_OP_SET_AP_SSID,
    // LISAUI_WIFI_OP_SET_AP_PASSWORD,
    
    LISAUI_WIFI_OP_CONNECT,
    LISAUI_WIFI_OP_DISCONNECT,
    LISAUI_WIFI_OP_FORGET,
    LISAUI_WIFI_OP_RECONNECT,

} lisaui_wifi_op_t;

#define LISAUI_APP_WIFI_LIST_ITEM_MAX 10
typedef struct {
    int count;
    // wifi_metadata_t wifi_list[LISAUI_APP_WIFI_LIST_ITEM_MAX];
    wifi_metadata_t list[0];
} lisaui_wifi_list_t;

#ifdef __cplusplus
}
#endif

#endif // __LISAUI_APP_WIFI_TYPE_H__