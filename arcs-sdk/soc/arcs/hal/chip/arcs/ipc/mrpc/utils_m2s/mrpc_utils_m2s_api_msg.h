#ifndef __MRPC_UTILS_M2S_API_MSG_H__
#define __MRPC_UTILS_M2S_API_MSG_H__
#include <stdint.h>
#include "ls_err.h"
#include "mrpc.h"
#include "pm_impl.h"

typedef struct
{
    struct mrpc_req_msg hdr;
    pm_config_t config;
} mrpc_pm_sync_config_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_pm_sync_config_resp_t;

#endif //__MRPC_UTILS_M2S_API_MSG_H__