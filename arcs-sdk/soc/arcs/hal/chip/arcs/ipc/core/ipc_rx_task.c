/**
 ****************************************************************************************
 *
 * @file ipc_rx_task.c
 *
 * @brief Common IPC RX task helpers.
 *
 * Copyright (C) ListenAI 2026
 *
 ****************************************************************************************
 */

#include <stdbool.h>
#include "rtos_al.h"
#include "ipc_rx_task.h"
#include "ipc_msg.h"
#include "ipc_ep.h"

IPC_FUNC_ATTR void ipc_rx_task(void *pvParameters)
{
    uint8_t *msg;
    struct ipc_msg_desc desc;
    struct ipc_rx_task_env *chan_env = (struct ipc_rx_task_env*)pvParameters;

    while (1)
    {
        if (ipc_get_rbuffer(chan_env->ccb, &desc, -1))
        {
            if (chan_env->callback(chan_env->ccb, &desc, chan_env->param) == IPC_MSG_RELEASE)
                ipc_free_rbuffer(chan_env->ccb, desc.data, 0);
        }
    }
}

IPC_FUNC_ATTR int32_t ipc_recv_msg_callback(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, void *param)
{
    int32_t ret = IPC_MSG_RELEASE;

    if (!ipc_msg_process(desc))
        ret = ipc_ep_process(ccb, desc);

    return ret;
}