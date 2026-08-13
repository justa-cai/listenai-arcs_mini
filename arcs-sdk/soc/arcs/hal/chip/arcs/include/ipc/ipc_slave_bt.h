/**
 ****************************************************************************************
 *
 * @file ipc_slave_bt.h
 *
 * @brief BT HCI IPC transport - slave/AP side API.
 *
 * Copyright (C) ListenAI 2025
 *
 ****************************************************************************************
 */
#ifndef _IPC_SLAVE_BT_H_
#define _IPC_SLAVE_BT_H_

#include "ipc_bt_hci.h"

typedef struct bt_ipc_bt_task
{
    uint16_t len;
    uint8_t data[BT_IPC_HCI_BUF_SIZE];
} bt_ipc_bt_task_t;

int32_t ipc_slave_bt_init(void);
int32_t ipc_slave_bt_c2h_send(const uint8_t *data, uint16_t len);
uint8_t ipc_slave_bt_free_buf(uint8_t *msg);
void ipc_slave_bt_deinit(void);
const struct bt_ipc_stats *ipc_slave_bt_get_stats(void);

#endif
