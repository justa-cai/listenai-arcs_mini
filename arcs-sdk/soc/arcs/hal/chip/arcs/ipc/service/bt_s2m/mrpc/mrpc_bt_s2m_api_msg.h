#ifndef __MRPC_BT_S2M_API_MSG_H__
#define __MRPC_BT_S2M_API_MSG_H__
#include <stddef.h>    // standard definitions
#include <stdint.h>    // standard integer definition
#include <stdbool.h>   // boolean definition
#include "ls_err.h"
#include "ls_bt_type.h"
#include "mrpc.h"
#include "btos_def.h"
#include "bt_ipc_api.h"

typedef struct
{
    struct mrpc_req_msg hdr;
    uint32_t size;
} mrpc_cp_btos_malloc_api_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    void * buffer_ptr;
} mrpc_cp_btos_malloc_api_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    void * ptr;
} mrpc_cp_btos_free_api_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_cp_btos_free_api_resp_t;

#endif //__MRPC_BT_S2M_API_MSG_H__
