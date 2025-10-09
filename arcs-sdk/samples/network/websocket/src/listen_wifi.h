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

typedef void (*ls_wifi_stack_init_done_cb)(void);

/**
 * Wifi预初始化
 */
void ls_wifi_pre_init(ls_wifi_stack_init_done_cb pre_init_done);

/**
 * @brief 	刷新 DNS Server, Ex.114.114.114.114
 */
void ls_wifi_refresh_dnsserver(const char *const dns_srv);
#endif