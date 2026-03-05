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

#include "rtos_al.h"
#include "ipc_msg.h"
#include "ipc_config.h"
#include "ipc_mem.h"
#include "ipc_shared.h"
#include "amp_shared.h"
#include "ipc_utils.h"
#ifdef CFG_AMP_IPC_WIFI_CHAN
#include "ipc_master_wifi.h"
#endif

#ifdef IPC_MSG_SEGMENT

#define IPC_MSG_BUFFER_SIZE          IPC_A2C_MSG_BUF_SIZE * IPC_MSG_SEGMENT_MAX

#else

#define IPC_MSG_BUFFER_SIZE          IPC_A2C_MSG_BUF_SIZE

#endif

struct ipc_master_cb_tag
{
    void (*wifi_tx_data_cfm)(void *data, uint32_t status);
    void (*wifi_rx_data)(void *data);
    int32_t (*indication_handler)(struct ipc_msg_desc *desc, void *arg);
};

struct ipc_master_env_tag
{
    struct ipc_master_cb_tag cb;
    struct ipc_shared_env_tag *shared;
    uint32_t *config;

    bool link_state;
};


int32_t ipc_master_send_msg(struct ipc_ep *ep, void *data, uint32_t len, void *resp);
bool ipc_master_get_link_status(void);
int32_t ipc_master_init(struct ipc_master_cb_tag *cb);
struct ipc_ep* ipc_master_ep_register(uint32_t ep_idx, ipc_ep_handler_t handler, void *arg);
volatile struct amp_shared_info* ipc_get_amp_shared_info(void);
int32_t ipc_master_indication_handler(struct ipc_msg_desc *desc, void *arg);

#endif
