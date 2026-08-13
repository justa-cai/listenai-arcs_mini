#ifndef __MRPC_BT_S2M_API_SERVER_H__
#define __MRPC_BT_S2M_API_SERVER_H__

#include "mrpc_types.h"

typedef enum
{
    MRPC_MSG_ID_BT_S2M_START = MRPC_SERVICE_TYPE_BT_S2M << 24,
    MRPC_MSG_ID_CP_BTOS_MALLOC_API,
    MRPC_MSG_ID_CP_BTOS_FREE_API,
    MRPC_MSG_ID_BT_S2M_MAX
} ipc_msg_id_bt_s2m_t;

extern mrpc_msg_handler_t mrpc_msg_bt_s2m_handlers[MRPC_MSG_ID_BT_S2M_MAX - MRPC_MSG_ID_BT_S2M_START];

#endif
