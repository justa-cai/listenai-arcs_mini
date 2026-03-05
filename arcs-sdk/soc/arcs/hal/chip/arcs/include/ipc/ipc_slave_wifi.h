/**
 ****************************************************************************************
 *
 * @file ipc_slave_wifi.h
 *
 *
 * Copyright (C) ListenAI 2025
 *
 ****************************************************************************************
 */

#ifndef _IPC_SLAVE_WIFI_H_
#define _IPC_SLAVE_WIFI_H_

#include "rtos_al.h"
#include "ipc_core.h"
#include "ipc_ep.h"

struct ipc_slave_env_tag;
int32_t ipc_slave_wifi_init_rx_data_chan(struct ipc_slave_env_tag *ipc_env);
int32_t ipc_slave_wifi_init_tx_data_chan(struct ipc_slave_env_tag *ipc_env);
int32_t ipc_slave_wifi_rxbuf_check(void);
int32_t ipc_slave_wifi_rxdesc_push(void *data, int32_t size);
int32_t ipc_slave_wifi_txcfm_push(void *data, int32_t size);

#endif
