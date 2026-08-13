/**
 ****************************************************************************************
 *
 * @file mrpc_client.h
 *
 *
 * Copyright (C) ListenAI 2024
 *
 ****************************************************************************************
 */
#ifndef _MRPC_CLIENT_H_
#define _MRPC_CLIENT_H_

#include <stdint.h>

struct ipc_ep;

int32_t mrpc_msg_send_from(struct ipc_ep *ep_client, void *data, int32_t len, void *resp, uint32_t timeout_ms);
int32_t mrpc_msg_send(void *data, int32_t len, void *resp);
int32_t mrpc_msg_send_timeout(void *data, int32_t len, void *resp, uint32_t timeout_ms);
struct ipc_ep * mrpc_client_ep_init(uint32_t ipc_chan_local, uint32_t ipc_chan_remote, uint32_t local_eid, uint32_t remote_eid);
struct ipc_ep * mrpc_client_init(uint32_t ipc_chan_local, uint32_t ipc_chan_remote, uint32_t local_eid, uint32_t remote_eid);

#endif
