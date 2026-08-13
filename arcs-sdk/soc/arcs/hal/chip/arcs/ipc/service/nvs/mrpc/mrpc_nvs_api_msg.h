#ifndef __MRPC_NVS_API_MSG_H__
#define __MRPC_NVS_API_MSG_H__
#include <stdio.h>
#include <stdint.h>
#include "ls_err.h"
#include "ls_wifi_type.h"
#include "mrpc.h"

typedef struct
{
    struct mrpc_req_msg hdr;
    uint16_t tag;
    size_t buf_len;
} mrpc_ipc_nvds_get_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    uint8_t * buf;
    size_t buf_len;
    uint8_t data_buffer[];
} mrpc_ipc_nvds_get_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint16_t tag;
    uint8_t * buf;
    size_t buf_len;
    uint8_t data_buffer[];
} mrpc_ipc_nvds_put_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_ipc_nvds_put_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint16_t tag;
} mrpc_ipc_nvds_del_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_ipc_nvds_del_resp_t;

#endif //__MRPC_NVS_API_MSG_H__