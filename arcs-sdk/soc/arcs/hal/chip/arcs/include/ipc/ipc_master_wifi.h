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

#include <stdint.h>

#include "ipc_wifi_shared.h"

struct ipc_master_wifi_ops
{
    void (*tx_data_cfm)(void *data, uint32_t status);
    void (*rx_data)(void *data);
};

int32_t ipc_master_wifi_init(const struct ipc_master_wifi_ops *ops);
int32_t ipc_master_wifi_rxcfm_push(void *data, uint32_t size);
int32_t ipc_master_wifi_tx_push(void *data, uint32_t size);

#endif
