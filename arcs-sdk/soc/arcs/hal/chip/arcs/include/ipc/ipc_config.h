/**
 ****************************************************************************************
 *
 * @file ipc_config.h
 *
 *
 * Copyright (C) ListenAI 2024
 *
 ****************************************************************************************
 */

#ifndef _IPC_CONFIG_H_
#define _IPC_CONFIG_H_

enum ipc_config_id
{
    IPC_CFG_END = 0,
    IPC_CFG_MAC_ADDR,
    IPC_CFG_MAX,
};

struct ipc_config_item
{
    uint16_t id;
    uint16_t len;
    uint8_t data[];
};

struct ipc_config
{
    uint8_t mac_addr[6];
};

#endif
