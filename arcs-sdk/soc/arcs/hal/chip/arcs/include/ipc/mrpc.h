#ifndef __MRPC_H__
#define __MRPC_H__

#ifdef CFG_AMP_IPC_MRPC_SERVER
#include "mrpc_server.h"
#endif

#ifdef CFG_AMP_IPC_MRPC_CLIENT
#include "mrpc_client.h"
#endif


#define mrpc_malloc                  rtos_malloc
#define mrpc_free                    rtos_free

enum
{
    MRPC_SERVICE_TYPE_WIFI,
    MRPC_SERVICE_TYPE_LWIP,
    MRPC_SERVICE_TYPE_M2S_TEST,
    MRPC_SERVICE_TYPE_S2M_TEST,
    MRPC_SERVICE_TYPE_NVS,
    MRPC_SERVICE_TYPE_FLASH_IF,
    MRPC_SERVICE_TYPE_UTILS,
    MRPC_SERVICE_TYPE_OTP,
    MRPC_SERVICE_TYPE_MAX
};

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
