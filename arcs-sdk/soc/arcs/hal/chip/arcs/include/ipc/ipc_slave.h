/**
 ****************************************************************************************
 *
 * @file ipc_slave.h
 *
 * @brief IPC module.
 *
 * Copyright (C) ListenAI 2023
 *
 ****************************************************************************************
 */
#ifndef _IPC_SLAVE_H_
#define _IPC_SLAVE_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdint.h>
#include <stdbool.h>
#include "ipc_shared.h"
#include "ipc_msg.h"
#include "ipc_config.h"
#include "ipc_mem.h"
#include "amp_shared.h"
#include "ipc_utils.h"
#ifdef CFG_AMP_IPC_WIFI_CHAN
#include "ipc_slave_wifi.h"
#endif

#define IPC_MSG_MGMT

#ifdef IPC_MSG_SEGMENT

#define IPC_MSG_BUFFER_SIZE          (IPC_C2A_MSG_BUF_SIZE * IPC_MSG_SEGMENT_MAX)

#else

#define IPC_MSG_BUFFER_SIZE          IPC_C2A_MSG_BUF_SIZE

#endif

enum
{
    IPC_LINK_STATE_INIT,
    IPC_LINK_STATE_READY,
    IPC_LINK_STATE_FAIL
};


struct ipc_slave_cb_tag
{
    int32_t (*ipc_wifi_tx)(void *ecb, void *param);
    int32_t (*ipc_wifi_rx_cfm)(void *ecb, void *param);
};

struct ipc_slave_env_tag
{
    struct ipc_slave_cb_tag cb;
    struct ipc_shared_env_tag *shared;
    uint32_t link_state;
};


int32_t ipc_slave_init(struct ipc_slave_cb_tag *cb);
int32_t ipc_slave_msg_reply(uint16_t dst_id, uint16_t src_id, int32_t len, void *data);
int32_t ipc_slave_msg_push(uint32_t chan, uint32_t ep_idx, int32_t len, void *data);
int32_t ipc_slave_print(char *string, int32_t len);
void ipc_slave_putchar(char c);
void ipc_slave_release_msg(void *msg);
struct ipc_ep* ipc_slave_ep_register(uint32_t ep_idx, ipc_ep_handler_t handler, void *arg);
volatile struct amp_shared_info* ipc_get_amp_shared_info(void);

#endif
