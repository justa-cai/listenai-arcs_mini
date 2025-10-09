/**
 ****************************************************************************************
 *
 * Copyright (C) ListenAI 2024
 *
 ****************************************************************************************
*/
#ifndef _IPC_MSG_H__
#define _IPC_MSG_H__

#include "rtos_def.h"
#include "co_dl_list.h"
#include "ipc_core.h"
#include "ipc_ep.h"

#define IPC_MSG_TIMEOUT_MS         10000


#define IPC_MSG_FLAG_NONBLOCK      CO_BIT(0)
#define IPC_MSG_FLAG_REQ_CFM       CO_BIT(1)
#define IPC_MSG_FLAG_WAIT_PUSH     CO_BIT(2)
#define IPC_MSG_FLAG_WAIT_ACK      CO_BIT(3)
#define IPC_MSG_FLAG_WAIT_CFM      CO_BIT(4)
#define IPC_MSG_FLAG_DONE          CO_BIT(5)

#define IPC_MSG_MGMT_FLAG_IGN_ACK   CO_BIT(0)

#define IPC_MSG_ENO_OK             0
#define IPC_MSG_ENO_ERR            1
#define IPC_MSG_ENO_NOMEM          2
#define IPC_MSG_ENO_NORING         3
#define IPC_MSG_ENO_TIMEOUT        4
#define IPC_MSG_ENO_CRASH          5
#define IPC_MSG_ENO_BUSY           6

/* ATM IPC design makes it possible to get the CFM before the ACK,
 * otherwise this could have simply been a state enum */
#define IPC_MSG_WAIT_COMPLETE(flags) \
    (!(flags & (IPC_MSG_FLAG_WAIT_ACK | IPC_MSG_FLAG_WAIT_CFM)))

#define IPC_MSG_MSG_LEN_MAX        IPC_C2A_MSG_BUF_SIZE

#define IPC_MSG_POOL_SIZE          4

enum ipc_msg_mgmt_state {
    IPC_MSG_MGMT_STATE_INITED,
    IPC_MSG_MGMT_STATE_DEINIT,
    IPC_MSG_MGMT_STATE_CRASHED,
};

struct ipc_msg_block {
    struct dl_list list;
    struct ipc_msg_desc msg;
    void *resp;
#ifdef IPC_SUPPORT_MSGACK
    uint32_t tkn;
#endif
    uint32_t flags;

    void *complete_param;
};

struct ipc_msg_mgmt {
    enum ipc_msg_mgmt_state state;
    rtos_mutex lock;
#ifdef IPC_SUPPORT_MSGACK
    uint32_t next_tkn;
#endif
    uint32_t queue_sz;
    uint32_t max_queue_sz;
    uint32_t flag;

#ifdef IPC_SUPPORT_MSGACK
    void *last_msg;
#endif
    void *priv;
#ifdef IPC_MSG_MALLOC
    void *msg_mem;
    struct dl_list msg_pool;
#endif
    struct dl_list msg_pending;

#ifdef IPC_MSG_MALLOC
    struct ipc_msg_block* (*alloc)(struct ipc_msg_mgmt*);
    void (*free)(struct ipc_msg_mgmt*, struct ipc_msg_block*);
#endif
    int32_t (*queue)(struct ipc_msg_mgmt*, struct ipc_msg_block*);
    int32_t (*llind)(struct ipc_msg_mgmt*, struct ipc_msg_block*);
    int32_t (*msgind)(struct ipc_msg_mgmt*, struct ipc_msg_desc*);
    void (*print)(struct ipc_msg_mgmt*);
    void (*drain)(struct ipc_msg_mgmt*);
};

struct ipc_msg_mgmt* ipc_msg_mgmt_init(void *priv, uint32_t pool_size);
int32_t ipc_msg_send(struct ipc_ep *ep, void *data, uint32_t len, void *resp);
int32_t ipc_msg_process(void *msg);
void ipc_msg_print(void);
int32_t ipc_msg_reply(uint16_t dst_id, uint16_t src_id, int32_t len, void *data);
void ipc_msg_release(uint32_t chan, void *msg);

#endif
