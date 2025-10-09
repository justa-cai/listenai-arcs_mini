/**
 ****************************************************************************************
 *
 * Copyright (C) ListenAI 2024
 *
 *
 ****************************************************************************************
*/
#ifndef _IPC_EPT_H_
#define _IPC_EPT_H_


typedef int32_t (*ipc_ep_handler_t)(struct ipc_msg_desc *msg, void *arg);
struct ipc_ep
{
    struct dl_list list;
    union ipc_eid  local_eid;
    union ipc_eid  remote_eid;
    ipc_ep_handler_t handler;
    rtos_mutex lock;
    void *arg;
};

struct ipc_ep* ipc_ep_register(uint32_t ipc_chan_local,  uint32_t ipc_chan_remote, uint32_t local_eid, uint32_t remote_eid, ipc_ep_handler_t handler, void *arg);
void ipc_ep_unregister(struct ipc_ep *ep);
int32_t ipc_ep_process(void *ecb, struct ipc_msg_desc *desc);
int32_t ipc_ep_queue_rx_cb(struct ipc_msg_desc *desc, void *arg);


#endif
