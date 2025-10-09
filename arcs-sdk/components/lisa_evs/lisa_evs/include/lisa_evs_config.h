#ifndef __LISA_EVS_SDK_CONFIG_H__
#define __LISA_EVS_SDK_CONFIG_H__

#include <stdint.h>

typedef struct lisa_evs_config {
	/// 设备client_id iflyos 平台分配
	uint8_t *client_id;
	/// 设备id，iflyos平台申请
	uint8_t *device_id;
	/// ota密钥 iflyos平台分配
	uint8_t *ota_secret;
} lisa_evs_config_t;

#endif