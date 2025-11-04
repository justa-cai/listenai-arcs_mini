#ifndef __MRPC_S2M_TEST_API_MSG_H__
#define __MRPC_S2M_TEST_API_MSG_H__
#include <stdio.h>
#include <stdint.h>
#include <lwip/opt.h>
#include <lwip/pbuf.h>
#include <lwip/netif.h>
#include "ls_err.h"
#include "ls_wifi_type.h"
#include "mrpc.h"
#include "ipc_test.h"

typedef struct
{
    struct mrpc_req_msg hdr;
    struct ipc_ep * mrpc_ep;
    struct cfg_ipc_echo echo_req;
} mrpc_s2m_echo_req_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    struct cfg_ipc_echo_resp echo_resp;
} mrpc_s2m_echo_req_resp_t;

#endif //__MRPC_S2M_TEST_API_MSG_H__