#ifndef __MRPC_LWIP_API_SERVER_H__
#define __MRPC_LWIP_API_SERVER_H__

#include "mrpc_server.h"

typedef enum
{
    MRPC_MSG_ID_LWIP_START = MRPC_SERVICE_TYPE_LWIP << 24,
    MRPC_MSG_ID_LWIP_PBUF_ALLOC,
    MRPC_MSG_ID_LWIP_PBUF_FREE,
    MRPC_MSG_ID_LWIP_MAX
} ipc_msg_id_lwip_t;

extern mrpc_msg_handler_t mrpc_msg_lwip_handlers[MRPC_MSG_ID_LWIP_MAX - MRPC_MSG_ID_LWIP_START];

#endif