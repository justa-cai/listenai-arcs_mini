/**
 ****************************************************************************************
 *
 * @file ipc_test_internal.h
 *
 * @brief Internal IPC test MRPC types.
 *
 * Copyright (C) ListenAI 2024
 *
 ****************************************************************************************
 */
#ifndef _IPC_TEST_INTERNAL_H_
#define _IPC_TEST_INTERNAL_H_

#include <stdint.h>
#include "ipc_shared.h"
#include "ipc_test.h"
#include "ls_err.h"
#include "mrpc.h"

struct ipc_ep;
struct mrpc_server_env;

struct ipc_test_conf
{
    uint32_t cnt;
    uint16_t task_id;
    uint16_t type;
    struct ipc_ep *ep;
};

struct ipc_echo
{
    uint32_t pkt_id;
    uint32_t sec_tx;
    uint32_t usec_tx;
    uint32_t sec_rx;
    uint32_t usec_rx;
};

#define IPC_TEST_MIN(a, b)            (((a) < (b)) ? (a) : (b))

#ifdef IPC_MSG_SEGMENT
#define IPC_TEST_MRPC_BUF_LEN         (IPC_TEST_MIN(IPC_MASTER_MSG_BUF_SIZE, IPC_SLAVE_MSG_BUF_SIZE) * IPC_MSG_SEGMENT_MAX)
#else
#define IPC_TEST_MRPC_BUF_LEN         IPC_TEST_MIN(IPC_MASTER_MSG_BUF_SIZE, IPC_SLAVE_MSG_BUF_SIZE)
#endif

#define IPC_TEST_REQ_REMAIN_LEN       (IPC_TEST_MRPC_BUF_LEN - sizeof(struct mrpc_req_msg) - sizeof(struct ipc_ep *) - sizeof(struct ipc_echo))
#define IPC_TEST_RESP_REMAIN_LEN      (IPC_TEST_MRPC_BUF_LEN - sizeof(struct mrpc_resp_msg) - sizeof(struct ipc_echo))
#define IPC_TEST_DATA_LEN             ((IPC_TEST_MIN(IPC_TEST_REQ_REMAIN_LEN, IPC_TEST_RESP_REMAIN_LEN) / 4) * 4)

struct cfg_ipc_echo
{
    struct ipc_echo echo;
    uint8_t data[IPC_TEST_DATA_LEN];
};

struct cfg_ipc_echo_resp
{
    struct ipc_echo echo;
    uint8_t data[IPC_TEST_DATA_LEN];
};

ls_err_t m2s_echo_req(struct ipc_ep *mrpc_ep, struct cfg_ipc_echo *echo_req, struct cfg_ipc_echo_resp *echo_resp);
ls_err_t m2s_start_slave_test(struct ipc_ep *mrpc_ep, struct ipc_test_param *conf);
ls_err_t m2s_stop_slave_test(struct ipc_ep *mrpc_ep);
ls_err_t s2m_echo_req(struct ipc_ep *mrpc_ep, struct cfg_ipc_echo *echo_req, struct cfg_ipc_echo_resp *echo_resp);

#endif
