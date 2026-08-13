/**
 ****************************************************************************************
 *
 * @file ipc_master_bt.h
 *
 * @brief BT HCI IPC transport - master/CP side API.
 *
 * Copyright (C) ListenAI 2025
 *
 ****************************************************************************************
 */
#ifndef _IPC_MASTER_BT_H_
#define _IPC_MASTER_BT_H_

#include "ipc_bt_hci.h"

int32_t ipc_master_bt_init(void);
int32_t ipc_master_bt_h2c_send(const uint8_t *data, uint16_t len);
uint8_t ipc_master_bt_free_buf(uint8_t *msg);
void ipc_master_bt_deinit(void);
const struct bt_ipc_stats *ipc_master_bt_get_stats(void);

#endif
