/**
 ****************************************************************************************
 *
 * @file ipc_master_wifi.h
 *
 *
 * Copyright (C) ListenAI 2025
 *
 ****************************************************************************************
 */

#ifndef _IPC_MASTER_WIFI_H_
#define _IPC_MASTER_WIFI_H_

#include "rtos_al.h"
#include "ipc_core.h"
#include "ipc_ep.h"

struct ipc_master_env_tag;

int32_t ipc_master_wifi_init(void);
int32_t ipc_master_wifi_init_rx_chan(struct ipc_master_env_tag *ipc_env);
int32_t ipc_master_wifi_init_tx_chan(struct ipc_master_env_tag *ipc_env);
int32_t ipc_master_wifi_rxcfm_push(void *data, uint32_t size);
int32_t ipc_master_wifi_tx_push(void *data, uint32_t size);

#endif
