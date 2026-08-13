/**
 ****************************************************************************************
 *
 * @file ipc_test.h
 *
 * @brief Public IPC test control API.
 *
 * Copyright (C) ListenAI 2024
 *
 ****************************************************************************************
 */
#ifndef _IPC_TEST_API_H_
#define _IPC_TEST_API_H_

#include <stdint.h>

struct mrpc_server_env;

struct ipc_test_param
{
    uint16_t task_pairs;
    uint16_t cnt;
};

int32_t ipc_test_start(struct ipc_test_param *param);
void ipc_test_stop(void);

#ifdef CFG_AMP_IPC_MASTER
int32_t ipc_test_start_slave(struct ipc_test_param *conf);
void ipc_test_stop_slave(void);
#endif

void ipc_test_case_init(struct mrpc_server_env *mrpc_server);

#endif
