#ifndef __MRPC_UTILS_API_SERVER_H__
#define __MRPC_UTILS_API_SERVER_H__

#include "mrpc_server.h"

typedef enum
{
    MRPC_MSG_ID_UTILS_START = MRPC_SERVICE_TYPE_UTILS << 24,
    MRPC_MSG_ID_LS_READ_TEMP_VOLTAGE,
    MRPC_MSG_ID_LS_GET_MAC_FROM_NVS,
    MRPC_MSG_ID_LS_GET_MAC_CUSTOMIZED,
    MRPC_MSG_ID_UTILS_MAX
} ipc_msg_id_utils_t;

extern mrpc_msg_handler_t mrpc_msg_utils_handlers[MRPC_MSG_ID_UTILS_MAX - MRPC_MSG_ID_UTILS_START];

#endif