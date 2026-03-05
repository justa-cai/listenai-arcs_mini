/**
 ****************************************************************************************
 *
 * @file ipc_utils.h
 *
 * @brief IPC module.
 *
 * Copyright (C) ListenAI 2023
 *
 ****************************************************************************************
 */
#ifndef _IPC_UTILS_H_
#define _IPC_UTILS_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdint.h>
#include <stdbool.h>
#include "ipc_shared.h"
#include "ipc_core.h"
#include "ls_event.h"

#define CFG_IND_EVENT_LEN_MAX      64

enum {
    NET_EVENT_UP,   /*net up*/
    NET_EVENT_DOWN, /*net down*/
    NET_EVENT_DHCP_START,
    NET_EVENT_DHCP_STOP,
    NET_EVENT_DHCP_RELEASE
};

enum {
    IPC_IND_NETIF, /*netif*/
    IPC_IND_PRINT,    /*print msg, err ind*/
    IPC_IND_EVENT, /*system event*/
};

struct cfg_ind_netif
{
    struct ipc_msg_hdr hdr;
    /// Vif idx
    uint16_t fvif_idx;
    /// event code
    uint16_t evt;
};

struct cfg_ind_event
{
    struct ipc_msg_hdr hdr;
    event_module_t module_id;
    int32_t event_id;
    int32_t event_data_size;
    uint8_t event_data[CFG_IND_EVENT_LEN_MAX];
};

void ipc_send_notify(uint32_t event);
void ipc_set_app_status(uint32_t bit_mask);
uint32_t ipc_get_app_status(uint32_t bit_mask);
void ipc_clear_app_status(uint32_t bit_mask);
void ipc_halt_by_peer(bool isr);
int32_t ipc_halt_peer_init(void);
int32_t ipc_halt_peer_core(void);
int32_t ipc_resume_peer_core(void);
volatile struct amp_shared_info* ipc_get_shared_info(void);

#endif
