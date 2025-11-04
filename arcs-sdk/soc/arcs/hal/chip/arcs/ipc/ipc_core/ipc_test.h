/**
 ****************************************************************************************
 *
 * @file ipc_test.h
 *
 * @brief CLI task for Fully Hosted firmware.
 *
 * Copyright (C) ListenAI 2024
 *
 ****************************************************************************************
 */
#ifndef _IPC_TEST_H_
#define _IPC_TEST_H_

#define IPC_TEST_DATA_LEN    200

struct ipc_test_conf
{
    uint32_t cnt;
    uint16_t task_id;
    uint16_t type;
    struct ipc_ep *ep;
};

struct ipc_test_param
{
    uint16_t task_num;
    uint16_t cnt;
};

struct ipc_echo
{
    uint32_t pkt_id;
    uint32_t sec_tx;
    uint32_t usec_tx;
    uint32_t sec_rx;
    uint32_t usec_rx;
};

struct cfg_ipc_echo
{
    struct ipc_echo echo;
#ifdef IPC_MSG_SEGMENT
    uint8_t data[IPC_TEST_DATA_LEN];
#endif
};

struct cfg_ipc_echo_resp
{
    struct ipc_echo echo;
#ifdef IPC_MSG_SEGMENT
    uint8_t data[IPC_TEST_DATA_LEN];
#endif
};

#ifdef CFG_AMP_IPC_MASTER
int32_t ipc_test_start(struct ipc_test_param *param);
void ipc_test_stop(void);
void ipc_test_start_slave(struct ipc_test_param *conf);
void ipc_test_stop_slave(void);
#endif
void ipc_test_case_init(struct mrpc_server_env *mrpc_server);
#endif
