/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 默认服务器配置宏 */
#define LSC_HOST             "api.listenai.com"
#define LSC_HOST_STAGING     "staging-api.listenai.com"
#define LSC_HOST_INTEGRATION "integration-api.listenai.com"
#define LSC_TOKEN_URL        "http://api.listenai.com/v1/auth/tokens"
#define LSC_TOKEN_URL_STAGING "http://staging-api.listenai.com/v1/auth/tokens"
#define LSC_TOKEN_URL_INTEGRATION "http://integration-api.listenai.com/v1/auth/tokens"
#define LSC_MUSCI_ACTIVE_URL "http://api.listenai.com/v1/kuwo/active"
#define LSC_MUSCI_TRANLINK   "http://api.listenai.com/v1/kuwo/tranklink"
#define LSC_PORT             "80"
#define LSC_SCHEME           "ws"

// 使能后，会将原始PCM音频进行ICO压缩后再上传云端
#define LSC_AUDIO_TYPE_ICO 1

/* 直接从 lsc_server_config_t* 获取配置的辅助宏 */
#define LSC_GET_HOST_FROM_SERVER(server_config) \
	((server_config) && (server_config)->host ? \
		(server_config)->host : LSC_HOST)

#define LSC_GET_HOST_STAGING_FROM_SERVER(server_config) \
	((server_config) && (server_config)->host_staging ? \
		(server_config)->host_staging : LSC_HOST_STAGING)

#define LSC_GET_HOST_INTEGRATION_FROM_SERVER(server_config) \
	((server_config) && (server_config)->host_integration ? \
		(server_config)->host_integration : LSC_HOST_INTEGRATION)

#define LSC_GET_TOKEN_URL_FROM_SERVER(server_config) \
	((server_config) && (server_config)->token_url ? \
		(server_config)->token_url : LSC_TOKEN_URL)

#define LSC_GET_TOKEN_URL_STAGING_FROM_SERVER(server_config) \
	((server_config) && (server_config)->token_url_staging ? \
		(server_config)->token_url_staging : LSC_TOKEN_URL_STAGING)

#define LSC_GET_TOKEN_URL_INTEGRATION_FROM_SERVER(server_config) \
	((server_config) && (server_config)->token_url_integration ? \
		(server_config)->token_url_integration : LSC_TOKEN_URL_INTEGRATION)

#define LSC_GET_MUSIC_ACTIVE_URL_FROM_SERVER(server_config) \
	((server_config) && (server_config)->music_active_url ? \
		(server_config)->music_active_url : LSC_MUSCI_ACTIVE_URL)

#define LSC_GET_MUSIC_TRANLINK_URL_FROM_SERVER(server_config) \
	((server_config) && (server_config)->music_tranlink_url ? \
		(server_config)->music_tranlink_url : LSC_MUSCI_TRANLINK)

#define LSC_GET_PORT_FROM_SERVER(server_config) \
	((server_config) && (server_config)->port ? \
		(server_config)->port : LSC_PORT)

#define LSC_GET_SCHEME_FROM_SERVER(server_config) \
	((server_config) && (server_config)->scheme ? \
		(server_config)->scheme : LSC_SCHEME)

#ifdef __cplusplus
}
#endif
