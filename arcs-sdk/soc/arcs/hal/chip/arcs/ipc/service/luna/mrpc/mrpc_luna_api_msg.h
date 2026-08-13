#ifndef __MRPC_LUNA_API_MSG_H__
#define __MRPC_LUNA_API_MSG_H__
#include <stdint.h>
#include "ls_err.h"
#include "mrpc.h"

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_luna_demo_get_version_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    uint32_t version;
} mrpc_luna_demo_get_version_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    int32_t a;
    int32_t b;
} mrpc_luna_demo_add_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    int32_t result;
} mrpc_luna_demo_add_resp_t;

#endif //__MRPC_LUNA_API_MSG_H__