#ifndef __MRPC_M2S_TEST_API_MSG_H__
#define __MRPC_M2S_TEST_API_MSG_H__
#include <stdio.h>
#include <stdint.h>
#include "ls_err.h"
#include "ls_wifi_type.h"
#include "mrpc.h"
#include "ipc_test.h"

typedef struct
{
    struct mrpc_req_msg hdr;
    struct ipc_ep * mrpc_ep;
    struct cfg_ipc_echo echo_req;
} mrpc_m2s_echo_req_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    struct cfg_ipc_echo_resp echo_resp;
} mrpc_m2s_echo_req_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    struct ipc_ep mrpc_ep;
    struct ipc_test_param conf;
} mrpc_m2s_start_slave_test_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_m2s_start_slave_test_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    struct ipc_ep mrpc_ep;
} mrpc_m2s_stop_slave_test_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_m2s_stop_slave_test_resp_t;

#endif //__MRPC_M2S_TEST_API_MSG_H__