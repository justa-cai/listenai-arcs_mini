/**
 ****************************************************************************************
 *
 * Copyright (C) ListenAI 2026
 *
 ****************************************************************************************
 */
#ifndef _IPC_RX_TASK_H_
#define _IPC_RX_TASK_H_

#include <stdint.h>
#include "ipc_core.h"

typedef int32_t (*ipc_rx_task_callback_t)(struct ipc_ccb *ccb,
                                          struct ipc_msg_desc *desc,
                                          void *param);

struct ipc_rx_task_env
{
    struct ipc_ccb *ccb;
    ipc_rx_task_callback_t callback;
    void *param;
};

void ipc_rx_task(void *env);
int32_t ipc_recv_msg_callback(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, void *param);

#endif
