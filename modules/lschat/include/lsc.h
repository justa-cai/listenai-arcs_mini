/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "lisa/ultis.h"

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */
#define STRINGS_LSC_EVT(evt)                                             	\
	((evt == LSC_CONNECTED)                    ? "lsc connected"      		\
			: (evt == LSC_CONNECTING)          ? "lsc connecting"   		\
			: (evt == LSC_DISCONNECTED)        ? "lsc disconnected"    		\
			: (evt == LSC_CLOUD_AUTH_FAILD)    ? "lsc cloud auth faild"     \
			: (evt == LSC_GOT_TOKEN)           ? "lsc got token"     		\
													    : "faild evt")
/* clang-format on */

/**
 * @addtogroup lsc_connect 平台连接以及音乐服务调用方法
 * @{
 */

/**
 * @brief 发起云端连接请求后，下发的事件结果
 */
typedef enum {
	/** 连接已成功 */
	LSC_CONNECTED = BIT(0),
	/** 连接中 */
	LSC_CONNECTING = BIT(1),
	/** 连接已断开 */
	LSC_DISCONNECTED = BIT(2),
	/** 鉴权失败 */
	LSC_CLOUD_AUTH_FAILD = BIT(3),
	/** 鉴权成功并获取到token */
	LSC_GOT_TOKEN = BIT(4),
	/** 数据接收 */
	LSC_DATA_RECEIVED = BIT(5),
	/** 无效事件 */
	LSC_NONE = BIT(31),
} lsc_event_e;

/**
 * @brief 设备运行环境模式
 */
typedef enum {
	LSC_DEVICE_MODE_PROD = 0,
	LSC_DEVICE_MODE_STAGING = 1,
	LSC_DEVICE_MODE_INTEGRATION = 2,
} lsc_device_mode_t;

/**
 * @typedef lsc_event_cb_t
 * @brief 定义回调函数类型
 *
 * @param evt 事件类型
 * @param data 事件数据
 * @param size 事件数据长度
 * @param usr 用户参数
 */
typedef void (*lsc_event_cb_t)(lsc_event_e evt, void *data, uint32_t size, void *usr);

/**
 * @brief 服务器地址配置结构体
 *
 * 用于配置LSC服务器地址和端口信息。
 * 如果设置为NULL，将使用默认值。
 */
typedef struct {
	/** 生产环境Host */
	char *host;
	/** 测试环境Host */
	char *host_staging;
	/** 研发环境Host */
	char *host_integration;
	/** 生产环境Token URL */
	char *token_url;
	/** 测试环境Token URL */
	char *token_url_staging;
	/** 研发环境Token URL */
	char *token_url_integration;
	/** 音乐激活URL */
	char *music_active_url;
	/** 音乐转换链接URL */
	char *music_tranlink_url;
	/** 端口号 */
	char *port;
	/** WebSocket协议scheme */
	char *scheme;
} lsc_server_config_t;

/**
 * @brief 定义lsf组件的配置参数类型
 *
 * 发起连接之前，需提前配置相关参数。
 *
 * @note token用于指定特定token进行连接，如果没有指定token，那么将使用product_id，secret_id进行云端鉴权并获取token。
 */
typedef struct {
	/** 是否自动重连 */
	bool if_auto_reconn;
	/** 重连间隔时间 */
	uint32_t reconn_interval_ms;
	char *device_id;
	char *product_id;
	char *secret_id;
	/** 额外参数，用于鉴权时传递给服务器 */
	char *extra_param;
	/** 指定token */
	char *token;
	/** 设备运行环境模式 */
	lsc_device_mode_t device_mode;
	/** 服务器配置，如果为NULL则使用默认配置 */
	lsc_server_config_t *server_config;
} lsc_config_t;

/**
 * @brief 初始化lsp组件
 *
 * @param cfg 配置参数
 *
 * @retval 0： 成功
 */
int lsc_init(lsc_config_t *cfg);

/**
 * @brief 添加事件回调函数
 * 支持多次添加回调函数，每个回调函数都会收到事件通知。
 *
 * @param cb 回调函数
 * @param evt 监听的事件，参考@see lsc_event_e
 * @param usr 用户参数
 *
 * @retval 0： 成功
 */
int lsc_add_callback(lsc_event_e evt, lsc_event_cb_t cb, void *usr);

/**
 * @brief 移除已注册的回调函数
 *
 * @param cb 回调函数
 *
 * @retval 0： 成功
 */
int lsc_remove_callback(lsc_event_cb_t cb);

/**
 * @brief 发起连接
 *
 * @retval 0： 成功
 */
int lsc_connect(void);

/**
 * @brief 断开连接
 *
 * @retval 0： 成功
 */
int lsc_disconnect(void);

/**
 * @brief LSP组件逆初始化
 *
 * @retval 0： 成功
 */
int lsc_deinit(void);

/**
 * @brief 激活云端音乐服务
 * 调用该接口后，会执行音乐激活流程，并同步返回结果
 *
 * @retval 0： 成功
 */
int lsc_music_active(void);

/**
 * @brief 获取指定音乐id的url
 * 该接口可通过云端下发的音乐ID，来请求实际音乐文件的URL
 *
 * @param music_item_id 待获取的音乐id
 * @param[out] music_url 返回音乐的url
 *
 * @retval 0： 成功
 */
int lsc_music_request_url(const char *music_item_id, char music_url[256]);

/**
 * @brief 设置lsc的配置参数
 *
 * @param cfg 配置参数，参照@see lsc_config_t
 *
 * @retval 0： 成功
 */
int lsc_set_config(lsc_config_t *cfg);

/**
 * @brief 设置接入环境,默认为生产环境
 *
 * @param device_mode 0:生产环境  1:测试环境  2:研发环境
 *
 * @retval 0： 成功
 */
int lsc_set_device_mode(lsc_device_mode_t device_mode);

/**
 * @brief 获取lschat模块当前的JWT令牌
 *
 * @return 当前的JWT令牌，如果未认证则返回NULL
 */
const char *get_lsc_jwt_token(void);
/**
 * @}
 */

#ifdef __cplusplus
}
#endif
