/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "wifi_manager/dlist.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief WiFi 事件类型
 */
typedef enum {
    WIFI_MGR_WIFI_EVT_STA_CONNECTED       = (1 << 0),
    WIFI_MGR_WIFI_EVT_STA_DISCONNECTED    = (1 << 1),
    WIFI_MGR_WIFI_EVT_STA_CONNECTING      = (1 << 2),
    WIFI_MGR_WIFI_EVT_STA_CONNECTION_FAILED = (1 << 3),
    WIFI_MGR_WIFI_EVT_SCAN_DONE           = (1 << 4),
    WIFI_MGR_WIFI_EVT_SCAN_FAILED         = (1 << 5),
} wifi_mgr_wifi_event_t;

/**
 * @brief WiFi 连接失败信息
 */
typedef struct {
    int error_code;
    int status_code;
    int reason_code;
} wifi_mgr_connect_fail_info_t;

/**
 * @brief WiFi 加密模式
 */
typedef enum {
    WIFI_MGR_WIFI_AUTH_AUTO = 0,         /* 自动认证模式 */
    WIFI_MGR_WIFI_AUTH_OPEN,             /* 开放模式 */
    WIFI_MGR_WIFI_AUTH_WEP,              /* WEP */
    WIFI_MGR_WIFI_AUTH_WPA_PSK,          /* WPA_PSK */
    WIFI_MGR_WIFI_AUTH_WPA2_PSK,         /* WPA2_PSK */
    WIFI_MGR_WIFI_AUTH_WPA_WPA2_PSK,     /* WPA_WPA2_PSK */
    WIFI_MGR_WIFI_AUTH_WPA2_ENTERPRISE,  /* WPA2_ENTERPRISE */
    WIFI_MGR_WIFI_AUTH_WPA3_PSK,         /* WPA3_PSK */
    WIFI_MGR_WIFI_AUTH_WPA2_WPA3_PSK,    /* WPA2_WPA3_PSK */
    WIFI_MGR_WIFI_AUTH_UNKNOWN,          /* 未知模式 */
    WIFI_MGR_WIFI_AUTH_MAX,
} wifi_mgr_wifi_encryption_mode_t;

/**
 * @brief WiFi Station 配置结构
 */
typedef struct {
    char ssid[32];
    char pwd[64];
    char bssid[18];
    int channel;
    int rssi;
    wifi_mgr_wifi_encryption_mode_t encryption_mode;

    uint8_t pmk[32];      // PMK (Pairwise Master Key) 缓存
    uint8_t pmk_valid;    // PMK 有效性标志 (0=无效, 1=有效)
} wifi_mgr_wifi_sta_config_t;

/**
 * @brief WiFi 断开事件信息（包含失败原因）
 */
typedef struct {
    wifi_mgr_wifi_sta_config_t sta_config;
    wifi_mgr_connect_fail_info_t fail_info;
} wifi_mgr_disconnect_event_info_t;

/**
 * @brief WiFi 扫描信息结构
 */
typedef struct {
    char ssid[32];
    char bssid[18];
    int channel;
    int rssi;
    wifi_mgr_wifi_encryption_mode_t encryption_mode;
} wifi_mgr_wifi_scan_info_t;

/**
 * @brief WiFi 连接状态
 */
typedef enum {
    WIFI_MGR_WIFI_STATUS_STA_CONNECTED = 0,
    WIFI_MGR_WIFI_STATUS_STA_CONNECTING,
    WIFI_MGR_WIFI_STATUS_STA_DISCONNECTED,
    WIFI_MGR_WIFI_STATUS_STA_UNKNOWN,
} wifi_mgr_wifi_status_t;

/**
 * @brief WiFi 事件回调函数类型
 */
typedef void (*wifi_mgr_wifi_event_handler_t)(wifi_mgr_wifi_event_t event, void *event_data, uint32_t event_data_len, void *arg);

/**
 * @brief WiFi 事件回调结构
 */
typedef struct {
    sys_dnode_t node;  // 链表节点，由实现层管理
    wifi_mgr_wifi_event_handler_t handler;
    uint32_t events;
    void *event_data;
    uint32_t event_data_len;
    void *arg;
} wifi_mgr_wifi_event_cb_t;

/**
 * @brief WiFi 硬件操作接口
 */
typedef struct {
    /**
     * @brief 初始化 WiFi 硬件
     */
    int (*init)(void);

    /**
     * @brief 去初始化 WiFi 硬件
     */
    int (*deinit)(void);

    /**
     * @brief 使能 WiFi 管理器 station 模式
     */
    int (*sta_enable)(void);
    
    /**
     * @brief 禁用 WiFi 管理器 station 模式
     */
    int (*sta_disable)(void);

    /**
     * @brief 检查 WiFi 管理器 station 模式是否使能
     */
    bool (*sta_is_enable)(void);

    /**
     * @brief 添加事件回调
     */
    int (*add_callback)(wifi_mgr_wifi_event_cb_t *wifi_event_cb);

    /**
     * @brief 移除事件回调
     */
    int (*remove_callback)(wifi_mgr_wifi_event_cb_t *wifi_event_cb);

    /**
     * @brief 扫描周围的 WiFi 接入点
     */
    int (*scan_ap)(wifi_mgr_wifi_scan_info_t *ap_info, uint32_t size, uint32_t timeout_ms);

    /**
     * @brief 连接到 WiFi 接入点
     */
    int (*sta_connect)(wifi_mgr_wifi_sta_config_t *sta_config, uint32_t timeout_ms);

    /**
     * @brief 断开 WiFi 连接
     */
    int (*sta_disconnect)(uint32_t timeout_ms);

    /**
     * @brief 获取 WiFi 连接状态
     */
    wifi_mgr_wifi_status_t (*sta_get_status)(void);

    /**
     * @brief 获取 WiFi MAC 地址
     */
    int (*get_mac)(uint8_t *mac_addr);
} wifi_manager_wifi_ops_t;

#ifdef __cplusplus
}
#endif 
