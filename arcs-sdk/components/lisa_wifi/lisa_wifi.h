/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 自定义 MAC 地址回调函数类型
 * @param mac_addr 6字节的MAC地址数组
 * @return 0 成功，非0 失败
 */
typedef int8_t (*custom_mac_func_t)(uint8_t mac_addr[6]);

/**
 * @brief WiFi 初始化完成回调函数类型
 */
typedef void (*wifi_init_done_cb_t)(void);

/**
 * @brief WiFi 操作回调函数集合
 */
typedef struct {
    custom_mac_func_t custom_mac;   /**< 用户自定义 MAC 地址接口 */
    wifi_init_done_cb_t init_done;  /**< WiFi 初始化完成回调 */
} lisa_wifi_ops_t;

/*
 * ============================================================================
 * WiFi 初始化接口
 * ============================================================================
 * 根据不同的配置模式，lisa_wifi_init 函数有不同的签名：
 * 
 * 1. 双核模式 - WiFi 协议栈侧 (CONFIG_WIFI_LWIP_DIFF_CORE && CONFIG_WIFI)
 *    - 不支持设置 ops，由对端 LWIP 侧设置
 *    - 函数签名：int lisa_wifi_init(void)
 * 
 * 2. 双核模式 - LWIP 侧 (CONFIG_WIFI_LWIP_DIFF_CORE && CONFIG_LWIP)
 *    - 支持设置 ops，会同步到 WiFi 协议栈侧
 *    - 函数签名：int lisa_wifi_init(lisa_wifi_ops_t *ops)
 * 
 * 3. 单核模式 (CONFIG_WIFI_LWIP_SAME_CORE)
 *    - 支持设置 ops
 *    - 函数签名：int lisa_wifi_init(lisa_wifi_ops_t *ops)
 * 
 * @warning 所有模式下，lisa_wifi_init 都是异步初始化
 * ============================================================================
 */

#if defined(CONFIG_WIFI_LWIP_DIFF_CORE) && defined(CONFIG_WIFI)
/* 双核模式 - WiFi 协议栈侧 */
int lisa_wifi_init(void);

#elif defined(CONFIG_WIFI_LWIP_DIFF_CORE) && defined(CONFIG_LWIP)
/* 双核模式 - LWIP 侧 */
int lisa_wifi_init(lisa_wifi_ops_t *ops);

#elif defined(CONFIG_WIFI_LWIP_SAME_CORE)
/* 单核模式 */
int lisa_wifi_init(lisa_wifi_ops_t *ops);

#else
#error "Invalid WiFi configuration: must define one of the supported modes"
#endif

#ifdef __cplusplus
}
#endif
