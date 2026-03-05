/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "lisa_evt_pub.h"
#include "lisa_websocket.h"
#include "lisa/ultis.h"
#include "lsc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */
#define STRINGS_CONN_EVT(evt)                                             \
	((evt == CONN_CONNECTED)                      ? "conn connected"      \
			: (evt == CONN_DISCONNECTED)          ? "conn disconnected"   \
			: (evt == CONN_AUTH_SUCESS)           ? "conn auth sucess"    \
			: (evt == CONN_AUTH_FAILD)            ? "conn auth faild"     \
			: (evt == CONN_DATA_CJSON)            ? "conn data cjson"     \
			: (evt == CONN_CONNECTING)            ? "conn connecting"     \
													    : "faild evt")
/* clang-format on */

typedef enum {
	CONN_CONNECTED = BIT(0),
	CONN_DISCONNECTED = BIT(1),
	CONN_AUTH_SUCESS = BIT(2),
	// CONN_TOKEN_INVALID = BIT(3),
	CONN_AUTH_FAILD = BIT(3),
	CONN_DATA_CJSON = BIT(4),
	CONN_CONNECTING = BIT(5),
} conn_event_e;

typedef void (*conn_event_cb_t)(conn_event_e evt, void *data, uint32_t size, void *usr);

typedef struct {
	char *auth_header;
	lisa_ws_t *ws_hdl;
	lisa_evt_publisher_t evt_cb_list;
	lsc_device_mode_t device_mode;
	lsc_server_config_t *server_config;  // 只保存服务器配置指针
	// 调用方法
	int (*auth)(char *device_id, char *product_id, char *secret_id, char *extra_param);
	int (*connect)(char *token);
	int (*disconnect)(void);
	int (*add_evt_callback)(conn_event_cb_t cb, conn_event_e evt, void *usr);
	int (*remove_evt_callback)(conn_event_cb_t cb);
	int (*send_bin)(const uint8_t *data, uint32_t size);
	int (*send_text)(char *text);
} lsc_conn_t;

/**
 * @brief 目前只支持单实例
 *
 * @param server_config 服务器配置,用于获取服务器地址和端口信息
 * @retval 0： 成功
 */
lsc_conn_t *lsc_conn_create(lsc_server_config_t *server_config);
int lsc_conn_destroy(lsc_conn_t *hdl);
#ifdef __cplusplus
}
#endif
