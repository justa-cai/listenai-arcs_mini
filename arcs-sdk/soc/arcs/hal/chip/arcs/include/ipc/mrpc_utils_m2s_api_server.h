#ifndef __MRPC_UTILS_M2S_API_SERVER_H__
#define __MRPC_UTILS_M2S_API_SERVER_H__

#include "mrpc_server.h"

typedef enum
{
    MRPC_MSG_ID_UTILS_M2S_START = MRPC_SERVICE_TYPE_UTILS_M2S << 24,
    MRPC_MSG_ID_PM_SYNC_CONFIG,
    MRPC_MSG_ID_UTILS_M2S_MAX
} ipc_msg_id_utils_m2s_t;

extern mrpc_msg_handler_t mrpc_msg_utils_m2s_handlers[MRPC_MSG_ID_UTILS_M2S_MAX - MRPC_MSG_ID_UTILS_M2S_START];

#endif