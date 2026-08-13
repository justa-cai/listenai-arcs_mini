#ifndef __MRPC_LUNA_API_SERVER_H__
#define __MRPC_LUNA_API_SERVER_H__

#include "mrpc_types.h"

typedef enum
{
    MRPC_MSG_ID_LUNA_START = MRPC_SERVICE_TYPE_LUNA << 24,
    MRPC_MSG_ID_LUNA_DEMO_GET_VERSION,
    MRPC_MSG_ID_LUNA_DEMO_ADD,
    MRPC_MSG_ID_LUNA_MAX
} ipc_msg_id_luna_t;

extern mrpc_msg_handler_t mrpc_msg_luna_handlers[MRPC_MSG_ID_LUNA_MAX - MRPC_MSG_ID_LUNA_START];

#endif