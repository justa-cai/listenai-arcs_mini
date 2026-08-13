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

#include <stdint.h>

#include "ipc_wifi_shared.h"

struct ipc_slave_wifi_ops
{
    int32_t (*tx)(void *ecb, void *param);
    int32_t (*rx_cfm)(void *ecb, void *param);
};

int32_t ipc_slave_wifi_init(const struct ipc_slave_wifi_ops *ops);
int32_t ipc_slave_wifi_rxbuf_check(void);
int32_t ipc_slave_wifi_rxdesc_push(void *data, int32_t size);
int32_t ipc_slave_wifi_txcfm_push(void *data, int32_t size);

#endif
