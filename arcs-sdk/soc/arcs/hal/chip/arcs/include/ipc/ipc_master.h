/**
 ****************************************************************************************
 *
 * @file ipc_master.h
 *
 *
 * Copyright (C) ListenAI 2023
 *
 ****************************************************************************************
 */

#ifndef _IPC_MASTER_H_
#define _IPC_MASTER_H_

#include <stdbool.h>
#include <stdint.h>

#include "ipc_types.h"
#include "ipc_config.h"
#include "ipc_shared.h"
#include "ipc_utils.h"

#ifdef IPC_MSG_SEGMENT

#define IPC_MSG_BUFFER_SIZE          IPC_SLAVE_MSG_BUF_SIZE * IPC_MSG_SEGMENT_MAX

#else

#define IPC_MSG_BUFFER_SIZE          IPC_SLAVE_MSG_BUF_SIZE

#endif


struct ipc_master_env_tag
{
    struct ipc_shared_env_tag *shared;
    uint32_t *config;

    bool link_state;
};


int32_t ipc_master_send_msg(struct ipc_ep *ep, void *data, uint32_t len, void *resp);
bool ipc_master_get_link_status(void);
bool ipc_master_wait_linkup(uint32_t timeout_ms);
int32_t ipc_master_init(void);
struct ipc_ep* ipc_master_ep_register(uint32_t ep_idx, ipc_ep_handler_t handler, void *arg);

#endif
