#ifndef __MRPC_LWIP_API_MSG_H__
#define __MRPC_LWIP_API_MSG_H__
#include <stdio.h>
#include <stdint.h>
#include <lwip/opt.h>
#include <lwip/pbuf.h>
#include <lwip/netif.h>
#include "ls_err.h"
#include "ls_wifi_type.h"
#include "mrpc.h"

typedef struct
{
    struct mrpc_req_msg hdr;
    struct pbuf ** pbuf;
    pbuf_layer layer;
    u16_t length;
    pbuf_type type;
} mrpc_lwip_pbuf_alloc_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_lwip_pbuf_alloc_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    struct pbuf * p;
    uint8_t * count;
} mrpc_lwip_pbuf_free_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_lwip_pbuf_free_resp_t;

#endif //__MRPC_LWIP_API_MSG_H__