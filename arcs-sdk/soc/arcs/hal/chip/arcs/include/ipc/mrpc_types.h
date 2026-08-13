/**
 ****************************************************************************************
 *
 * @file mrpc_types.h
 *
 * @brief Public MRPC service IDs and handler types.
 *
 * Copyright (C) ListenAI 2026
 *
 ****************************************************************************************
 */
#ifndef _MRPC_TYPES_H_
#define _MRPC_TYPES_H_

#include <stdint.h>

enum
{
    MRPC_SERVICE_TYPE_WIFI,
    MRPC_SERVICE_TYPE_LWIP,
    MRPC_SERVICE_TYPE_BT,
    MRPC_SERVICE_TYPE_BT_S2M,
    MRPC_SERVICE_TYPE_M2S_TEST,
    MRPC_SERVICE_TYPE_S2M_TEST,
    MRPC_SERVICE_TYPE_NVS,
    MRPC_SERVICE_TYPE_FLASH_IF,
    MRPC_SERVICE_TYPE_UTILS_S2M,
    MRPC_SERVICE_TYPE_UTILS_M2S,
    MRPC_SERVICE_TYPE_LUNA,
	MRPC_SERVICE_TYPE_IPC_BUS,
    MRPC_SERVICE_TYPE_AP_PM_LOCK,
    MRPC_SERVICE_TYPE_MAX
};

struct mrpc_resp_msg;
typedef void (*mrpc_msg_handler_t)(void *msg,
                                   struct mrpc_resp_msg *resp_msg,
                                   uint32_t resp_buf_len);

#endif
