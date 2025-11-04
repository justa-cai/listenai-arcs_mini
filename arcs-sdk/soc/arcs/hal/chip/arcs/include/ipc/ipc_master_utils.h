/**
 ****************************************************************************************
 *
 * @file ipc_master_utils.h
 *
 *
 * Copyright (C) ListenAI 2023
 *
 ****************************************************************************************
 */

#ifndef _IPC_MASTER_UTILS_H_
#define _IPC_MASTER_UTILS_H_

#include "rtos_al.h"
#include "ipc_core.h"
#include "ipc_ep.h"

/**
 * @brief pre-set wifi mac address
 * @note should be called before ipc_wifi_init
 */
void ipc_wifi_mac_pre_set(uint8_t mac_addr[6]);

int32_t ipc_wifi_init(void);
int32_t ipc_indication_handler(struct ipc_msg_desc *desc, void *arg);

#endif
