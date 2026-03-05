#ifndef __MRPC_BT_API_SERVER_H__
#define __MRPC_BT_API_SERVER_H__

#include "mrpc_server.h"

typedef enum
{
    MRPC_MSG_ID_BT_START = MRPC_SERVICE_TYPE_BT << 24,
    MRPC_MSG_ID_BTOS_SEND_EVENT_API,
    MRPC_MSG_ID_BTOS_SEND_AT_EVT_API,
    MRPC_MSG_ID_BTOS_SEND_APP_EVT_API,
    MRPC_MSG_ID_BLE_GAP_SET_LOC_PUB_ADDR_API,
    MRPC_MSG_ID_LLM_GET_LOCAL_PUB_ADDR_API,
    MRPC_MSG_ID_BTOS_MALLOC_API,
    MRPC_MSG_ID_BTOS_FREE_API,
    MRPC_MSG_ID_LSIP_RESET_API,
    MRPC_MSG_ID_BT_MAX
} ipc_msg_id_bt_t;

extern mrpc_msg_handler_t mrpc_msg_bt_handlers[MRPC_MSG_ID_BT_MAX - MRPC_MSG_ID_BT_START];

#endif