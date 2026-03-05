/**
 * @brief   Wifi状态管理
 * @version 0.1
 * @date 2022-08-18
 * 
 * Copyright (C) 2022 ANHUI LISTENAI Co., LTD All Rights Reserved
 */
#ifndef __LISTENAI_WIFI_MANAGER_H__
#define __LISTENAI_WIFI_MANAGER_H__

#include <stdint.h>
#include <stdbool.h>
#if CONFIG_WIFI_MANAGER
#include "wifi_manager/wifi_manager.h"
#endif

typedef enum ls_wifi_status_s {
    LS_WIFI_NONE,
    LS_WIFI_STA_AP_CONNECTED,
    LS_WIFI_STA_CONNECTED,
    LS_WIFI_STA_DISCONNECTED,
} ls_wifi_status_t;
#if CONFIG_WIFI_MANAGER
#define WIFI_HOTSPOT_LIST_MAX_NUMBER  (32)
typedef enum{
    LS_WIFI_HOTSPOT_STATE_DISCONNECT = 0,
    LS_WIFI_HOTSPOT_STATE_CONNECTING,
    LS_WIFI_HOTSPOT_STATE_CONNECTED
}ls_wifi_hotspot_state_t;
typedef struct{
    ls_wifi_hotspot_state_t state;
    wifi_mgr_scan_info_t info;
}wifi_hotspot_info_t;
typedef struct{
    uint32_t number;
    wifi_hotspot_info_t hotspot[WIFI_HOTSPOT_LIST_MAX_NUMBER];

}wifi_hotspot_list_t;

typedef struct{
    bool is_valid;
    wifi_mgr_connection_status_t status;
    wifi_mgr_sta_config_t sta_cfg;
}wifi_sta_connect_info_t;
#endif

typedef void (*ls_wifi_status_cb)(ls_wifi_status_t status);
typedef void (*ls_wifi_stack_init_done_cb)(void);

typedef struct ls_wifi_s {
    ls_wifi_status_t m_st;
    ls_wifi_status_cb m_cb;
#if CONFIG_WIFI_MANAGER
    wifi_hotspot_list_t m_hotspot_list;
    wifi_sta_connect_info_t sta_connect;
#endif
} ls_wifi_t;

/**
 * Wifi预初始化
 */
void ls_wifi_pre_init(ls_wifi_stack_init_done_cb pre_init_done);

/**
 * @brief   初始化Wifi管理
 * @param   cb  状态回调
 */
ls_wifi_t * ls_wifi_init(ls_wifi_status_cb cb);

/**
 * 检查wifi是否连接成功
 * @return true: wifi连接成功, false: wifi连接失败
 */
bool ls_wifi_is_connected();

/**
 * @brief   连接Wifi
 * @param   ssid   SSID
 * @param   passwd 密码
 * @return  0:成功, -1:失败
 */
int ls_wifi_connect(const char *const ssid, const char *const passwd);

/**
 * @brief   获取网络状态
 */
ls_wifi_status_t ls_wifi_get_status();

int ls_wifi_ap_start(const char *const ssid, const char *const passwd);

/**
 * @brief 	刷新 DNS Server, Ex.114.114.114.114
 */
void ls_wifi_refresh_dnsserver(const char *const dns_srv);

#if CONFIG_WIFI_MANAGER
wifi_hotspot_list_t *ls_wifi_get_hotspot_list(void);
#endif
#endif