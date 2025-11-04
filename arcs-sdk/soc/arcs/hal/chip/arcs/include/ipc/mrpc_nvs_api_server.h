#ifndef __MRPC_NVS_API_SERVER_H__
#define __MRPC_NVS_API_SERVER_H__

#include "mrpc_server.h"

typedef enum
{
    MRPC_MSG_ID_NVS_START = MRPC_SERVICE_TYPE_NVS << 24,
    MRPC_MSG_ID_IPC_NVDS_GET,
    MRPC_MSG_ID_IPC_NVDS_PUT,
    MRPC_MSG_ID_IPC_NVDS_DEL,
    MRPC_MSG_ID_NVS_MAX
} ipc_msg_id_nvs_t;

extern mrpc_msg_handler_t mrpc_msg_nvs_handlers[MRPC_MSG_ID_NVS_MAX - MRPC_MSG_ID_NVS_START];

#endif