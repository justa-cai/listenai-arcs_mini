#ifndef __MRPC_H__
#define __MRPC_H__

#include <stdint.h>
#include "mrpc_types.h"

#ifdef CFG_AMP_IPC_MRPC_SERVER
#include "mrpc_server.h"
#endif

#ifdef CFG_AMP_IPC_MRPC_CLIENT
#include "mrpc_client.h"
#endif


#define mrpc_malloc                  rtos_malloc
#define mrpc_free                    rtos_free

struct mrpc_req_msg
{
    uint32_t id;
    int32_t len;
    uint8_t data[];
};

struct mrpc_resp_msg
{
    int32_t status;
    int32_t len;
    uint8_t data[];
};
#endif
