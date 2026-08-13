/**
 ****************************************************************************************
 *
 * @file ipc_bt_shared.h
 *
 * @brief BT IPC shared-memory data plane layout.
 *
 * Copyright (C) ListenAI 2026
 *
 ****************************************************************************************
 */

#ifndef _IPC_BT_SHARED_H_
#define _IPC_BT_SHARED_H_

#include <stdint.h>
#include "ipc_types.h"
#include "ipc_bt_hci.h"

#define IPC_BT_H2C_CNT                  4
#define IPC_BT_C2H_CNT                  4

struct bt_ipc_hci_pkt
{
    uint16_t len;
    uint8_t data[BT_IPC_HCI_BUF_SIZE];
};

struct bt_ipc_epmsg
{
    struct ipc_epmsg ephdr;
    struct bt_ipc_hci_pkt buf;
};

struct ipc_bt_h2c_tag
{
    struct vring_hdr   ring;
    struct vring_desc  desc[IPC_BT_H2C_CNT];
    struct vring_avail avail[IPC_BT_H2C_CNT];
    struct vring_ready ready[IPC_BT_H2C_CNT];
    struct bt_ipc_epmsg items[IPC_BT_H2C_CNT];
};

struct ipc_bt_c2h_tag
{
    struct vring_hdr   ring;
    struct vring_desc  desc[IPC_BT_C2H_CNT];
    struct vring_avail avail[IPC_BT_C2H_CNT];
    struct vring_ready ready[IPC_BT_C2H_CNT];
    struct bt_ipc_epmsg items[IPC_BT_C2H_CNT];
};

struct ipc_bt_shared_env
{
    struct ipc_bt_h2c_tag h2c;
    struct ipc_bt_c2h_tag c2h;
};


#endif
