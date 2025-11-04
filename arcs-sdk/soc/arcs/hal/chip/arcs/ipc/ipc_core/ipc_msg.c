/**
 ****************************************************************************************
 *
 * Copyright (C) ListenAI 2023
 *
 ****************************************************************************************
*/

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <rtos_al.h>
#include <ipc_shared.h>
#include <ipc_core.h>
#include <ipc_msg.h>





static struct ipc_msg_mgmt msg_mgmt_env;

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))



#ifdef IPC_DEBUG
#define IPC_STRING(str) #str

const char * ipc_eid_str[] =
{
    IPC_STRING(EP_IND),
    IPC_STRING(EP_MRPC_SRV),
    IPC_STRING(EP_MRPC_CLT),
    IPC_STRING(EP_MRPC_SRV_TEST),
    IPC_STRING(EP_MRPC_CLT_TEST),
};

const char * ipc_chan_master_str[] =
{
    IPC_STRING(CHAN_MASTER_TXCFM),
    IPC_STRING(CHAN_MASTER_RXDESC),
    IPC_STRING(CHAN_MASTER_FAST),
    IPC_STRING(CHAN_MASTER_MSG),
};

const char * ipc_chan_slave_str[] =
{
    IPC_STRING(CHAN_SLAVE_TXDESC),
    IPC_STRING(CHAN_SLAVE_RXCFM),
    IPC_STRING(CHAN_SLAVE_FAST),
    IPC_STRING(CHAN_SLAVE_MSG),
};

void ipc_msg_print_str(char *desc, uint16_t src_id, uint16_t dst_id)
{
    union ipc_eid eid;

    ipc_dbg("%s ", desc);
    if (src_id != -1)
    {
        ipc_dbg("from 0x%04x(", src_id);
        eid.val = src_id;
        if (eid.id.chan < IPC_CHAN_MASTER_MAX)
            ipc_dbg("%s", ipc_chan_master_str[eid.id.chan]);
        else if (eid.id.chan < IPC_CHAN_SLAVE_MAX)
            ipc_dbg("%s", ipc_chan_slave_str[eid.id.chan - IPC_CHAN_SLAVE_TXDESC]);
        else
            ipc_dbg("ERR");

        if (eid.id.ep < IPC_EP_MAX)
            ipc_dbg(" %s)", ipc_eid_str[eid.id.ep]);
        else
            ipc_dbg(" ERR)");
    }

    if (dst_id != -1)
    {
        ipc_dbg(" to 0x%04x(", dst_id);
        eid.val = dst_id;
        if (eid.id.chan < IPC_CHAN_MASTER_MAX)
            ipc_dbg("%s", ipc_chan_master_str[eid.id.chan]);
        else if (eid.id.chan < IPC_CHAN_SLAVE_MAX)
            ipc_dbg("%s", ipc_chan_slave_str[eid.id.chan - IPC_CHAN_SLAVE_TXDESC]);
        else
            ipc_dbg("ERR");

        if (eid.id.ep < IPC_EP_MAX)
            ipc_dbg(" %s)", ipc_eid_str[eid.id.ep]);
        else
            ipc_dbg(" ERR)");
    }
    ipc_dbg("\n");
}
#else
#define ipc_msg_print_str(...)
#endif


static void msg_dump(const struct ipc_msg_block *msgb)
{
    ipc_err("Msg flags:%04x src: 0x%x, dst: 0x%x\n",
           msgb->flags, msgb->msg.hdr.src_id.val, msgb->msg.hdr.dst_id.val);
}

static void msg_complete(struct ipc_msg_mgmt *msg_mgmt, struct ipc_msg_block *msgb)
{
    ipc_dbg("MsgSnd: complete\n");
    msgb->flags |= IPC_MSG_FLAG_DONE;
    if (msgb->flags & IPC_MSG_FLAG_NONBLOCK)
    {
#ifdef IPC_MSG_MALLOC
        msg_mgmt->free(msg_mgmt, msgb);
#endif
    }
    else
    {
        if (IPC_MSG_WAIT_COMPLETE(msgb->flags))
            rtos_task_notify(msgb->complete_param, false);
    }
}

static int32_t msg_send(struct ipc_msg_mgmt *msg_mgmt, struct ipc_msg_block *msgb)
{
    int32_t ret = -1;
    struct ipc_ccb *ccb = (struct ipc_ccb*)msg_mgmt->priv;

    if (!ipc_sendto((struct ipc_ccb*)msg_mgmt->priv, &msgb->msg, IPC_MSG_TIMEOUT_MS))
    {
        ret = 0;
#ifdef IPC_SUPPORT_MSGACK
        msg_mgmt->last_msg = (int32_t)msgb;
#endif
    }

    return ret;
}

static int32_t msg_mgmt_queue(struct ipc_msg_mgmt *msg_mgmt, struct ipc_msg_block *msgb)
{
    int32_t ret = IPC_MSG_ENO_OK;
#ifdef IPC_SUPPORT_MSGACK
    bool defer_push = false;
    struct ipc_msg_block *last;
#endif
    if (msg_mgmt->state == IPC_MSG_MGMT_STATE_CRASHED)
    {
        ret = IPC_MSG_ENO_CRASH;
        ipc_err("Cmd queue crashed\n");
        goto Out;
    }

    rtos_mutex_lock(msg_mgmt->lock);
    if (!dl_list_empty(&msg_mgmt->msg_pending))
    {
        if (msg_mgmt->queue_sz == msg_mgmt->max_queue_sz)
        {
            rtos_mutex_unlock(msg_mgmt->lock);
            ret = IPC_MSG_ENO_NOMEM;
            ipc_err("Too many msgs (%ld) already queued\n", msg_mgmt->max_queue_sz);
            goto Out;
        }
#ifdef IPC_SUPPORT_MSGACK
        last = dl_list_entry(msg_mgmt->msg_pending.prev, struct ipc_msg_block, list);
        if (last->flags & (IPC_MSG_FLAG_WAIT_ACK | IPC_MSG_FLAG_WAIT_PUSH))
        {
            msgb->flags |= IPC_MSG_FLAG_WAIT_PUSH;
            defer_push = true;
        }
#endif
    }

#ifdef IPC_SUPPORT_MSGACK
    if (!(msg_mgmt->flag & IPC_MSG_MGMT_FLAG_IGN_ACK))
        msg->flags |= IPC_MSG_FLAG_WAIT_ACK;
    msgb->tkn = msg_mgmt->next_tkn++;
#endif

    if (!(msgb->flags & IPC_MSG_FLAG_NONBLOCK))
        msgb->complete_param = (void*)(rtos_get_task_handle());

    dl_list_add_tail(&msg_mgmt->msg_pending, &msgb->list);
    msg_mgmt->queue_sz++;
    rtos_mutex_unlock(msg_mgmt->lock);

#ifdef IPC_SUPPORT_MSGACK
    if (!defer_push)
#endif
    {
        if (msg_send(msg_mgmt, msgb))
        {
            ipc_err("IPC sending failed\n");
            ret = IPC_MSG_ENO_NORING;
            goto Out;
        }
    }

    /*Wait*/
    if (!(msgb->flags & IPC_MSG_FLAG_NONBLOCK))
    {
        ipc_dbg("MsgSnd: wait 0x%x\n", msgb->msg.hdr.src_id.val);
        if (!rtos_task_wait_notification(IPC_MSG_TIMEOUT_MS))
        {
            ipc_err("MsgSnd: timeout src: 0x%x\n", msgb->msg.hdr.dst_id.val);
            msg_dump(msgb);
            rtos_mutex_lock(msg_mgmt->lock);
            if (!(msgb->flags & IPC_MSG_FLAG_DONE))
            {
                msg_complete(msg_mgmt, msgb);
                ret = IPC_MSG_ENO_TIMEOUT;
            }
            else
            {
               msg_mgmt->state = IPC_MSG_MGMT_STATE_CRASHED;
            }
            rtos_mutex_unlock(msg_mgmt->lock);
        }
        else
        {
            ipc_dbg("MsgSnd: src 0x%x success\n", msgb->msg.hdr.dst_id.val);
        }
    }
    else if (msgb->resp)
    {
        /*Will be released by "msg_complete" later */
        return ret;
    }

Out:
#ifdef IPC_MSG_MALLOC
    msg_mgmt->free(msg_mgmt, msgb);
#else
    rtos_mutex_lock(msg_mgmt->lock);
    dl_list_del(&msgb->list);
    msg_mgmt->queue_sz--;
    rtos_mutex_unlock(msg_mgmt->lock);
#endif
    return ret;
}

#ifdef IPC_SUPPORT_MSGACK
static int32_t msg_mgmt_llind(struct ipc_msg_mgmt *msg_mgmt, struct ipc_msg_block *msgb)
{
    struct ipc_msg_block *cur, *acked = NULL, *next = NULL;

    rtos_mutex_lock(msg_mgmt->lock);
    dl_list_for_each(cur, &msg_mgmt->msg_pending, struct ipc_msg_block, list) {
        if (!acked) {
            if (cur->tkn == msgb->tkn) {
                if (cur != msgb) {
                    msg_dump(msgb);
                }
                acked = cur;
                continue;
            }
        }
        if (cur->flags & IPC_MSG_FLAG_WAIT_PUSH) {
                next = cur;
                break;
        }
    }
    if (!acked) {
        ipc_err("Error: acked msg not found\n");
    } else {
        ipc_dbg("ACK:%d\n", msgb->id);
        msg->flags &= ~IPC_MSG_FLAG_WAIT_ACK;
        if (IPC_MSG_WAIT_COMPLETE(msgb->flags))
            msg_complete(msg_mgmt, msgb);
    }
    if (next) {
        next->flags &= ~IPC_MSG_FLAG_WAIT_PUSH;
        msg_send(msg_mgmt, next);
    }
    rtos_mutex_unlock(msg_mgmt->lock);

    return 0;
}
#endif

static int32_t msg_mgmt_msgind(struct ipc_msg_mgmt *msg_mgmt, struct ipc_msg_desc *msg)
{
    int32_t found = 0;
    struct ipc_msg_block *pending;

    ipc_msg_print_str("MsgInd", msg->hdr.src_id.val, msg->hdr.dst_id.val);
    rtos_mutex_lock(msg_mgmt->lock);
    dl_list_for_each(pending, &msg_mgmt->msg_pending, struct ipc_msg_block, list)
    {
        ipc_msg_print_str("Pending:", pending->msg.hdr.src_id.val, pending->msg.hdr.dst_id.val);
        if ((pending->msg.hdr.src_id.val == msg->hdr.dst_id.val) && (pending->msg.hdr.dst_id.val == msg->hdr.src_id.val)
            && (pending->flags & IPC_MSG_FLAG_WAIT_CFM))
        {
            ipc_dbg("matched %08x\n", pending->flags);
            found = 1;
            pending->flags &= ~IPC_MSG_FLAG_WAIT_CFM;

            if (msg->hdr.data_len > IPC_MSG_MSG_LEN_MAX)
                msg->hdr.data_len = IPC_MSG_MSG_LEN_MAX;

            ipc_dbg("MsgInd: end\n");
            if (pending->resp && msg->hdr.data_len)
            {
#ifdef IPC_MSG_SEGMENT
                int32_t i = 0;
                uint16_t len = 0;
                void **ptr = &msg->data;

                do
                {
                    memcpy((pending->resp + len), ptr[i], IPC_GET_EPMSG_LEN(ptr[i]));
                    len += IPC_GET_EPMSG_LEN(ptr[i]);
                    ipc_dbg("msg seg[%d]: %d\n", i, len);
                } while ((len < msg->hdr.total_len) && (++i < (IPC_MSG_SEGMENT_MAX)));
#else
                memcpy(pending->resp, msg->data, msg->hdr.data_len);
#endif
            }

            if (IPC_MSG_WAIT_COMPLETE(pending->flags))
            {
                msg_complete(msg_mgmt, pending);
            }
            break;
        }
#ifdef IPC_DEBUG
        else if ((pending->msg.hdr.src_id.val == msg->hdr.dst_id.val) && (pending->msg.hdr.dst_id.val == msg->hdr.src_id.val))
        {
            ipc_dbg("MsgInd: err status dst 0x%x flags 0x%x\n", pending->msg.hdr.dst_id.val, pending->flags);
        }
#endif
    }
    rtos_mutex_unlock(msg_mgmt->lock);

#ifdef IPC_DEBUG
    if (!found)
    {
        ipc_msg_print_str("MsgInd: forward", -1, msg->hdr.dst_id.val);
    }
#endif

    return found;
}

static void msg_mgmt_print(struct ipc_msg_mgmt *msg_mgmt)
{
    struct ipc_msg_block *cur;

    rtos_mutex_lock(msg_mgmt->lock);
    ipc_dbg("q_sz/max: %2ld / %2ld\n",
             msg_mgmt->queue_sz, msg_mgmt->max_queue_sz);
    dl_list_for_each(cur, &msg_mgmt->msg_pending, struct ipc_msg_block, list) {
        msg_dump(cur);
    }
    rtos_mutex_unlock(msg_mgmt->lock);
}

static void msg_mgmt_drain(struct ipc_msg_mgmt *msg_mgmt)
{
    struct ipc_msg_block *cur, *nxt;

    rtos_mutex_lock(msg_mgmt->lock);
    dl_list_for_each_safe(cur, nxt, &msg_mgmt->msg_pending, struct ipc_msg_block, list) {
        dl_list_del(&cur->list);
        msg_mgmt->queue_sz--;
        if (!(cur->flags & IPC_MSG_FLAG_NONBLOCK))
            rtos_task_notify(cur->complete_param, false);
    }
    rtos_mutex_unlock(msg_mgmt->lock);
}
#ifdef IPC_MSG_MALLOC
static struct ipc_msg_block* msg_mgmt_alloc(struct ipc_msg_mgmt *mgmt)
{
    struct ipc_msg_block *msg = NULL;

    rtos_mutex_lock(mgmt->lock);
    msg = dl_list_first(&mgmt->msg_pool, struct ipc_msg_block, list);
    if (msg)
    {
        dl_list_del(&msg->list);
        memset(msg, 0, sizeof(struct ipc_msg_block));
    }
    rtos_mutex_unlock(mgmt->lock);
    ipc_dbg("MsgAlloc: %p\n", msg);

    return msg;
}

static void msg_mgmt_free(struct ipc_msg_mgmt *mgmt, struct ipc_msg_block *msgb)
{
    if (msgb)
    {
        rtos_mutex_lock(mgmt->lock);
        dl_list_del(&msgb->list);
        mgmt->queue_sz--;
#ifdef IPC_MSG_MALLOC
        dl_list_add(&mgmt->msg_pool, &msgb->list);
#endif
        rtos_mutex_unlock(mgmt->lock);
        ipc_dbg("MsgFree: %p\n", msgb);
    }
}
#endif
struct ipc_msg_mgmt* ipc_msg_mgmt_init(void *priv, uint32_t pool_size)
{
    uint32_t i;
    struct ipc_msg_block *msg  = NULL;
    struct dl_list *pool = NULL;
    struct dl_list *item = NULL;

    dl_list_init(&msg_mgmt_env.msg_pending);
#ifdef IPC_MSG_MALLOC
    dl_list_init(&msg_mgmt_env.msg_pool);
#endif
    rtos_mutex_create(&msg_mgmt_env.lock);
    msg_mgmt_env.max_queue_sz = pool_size;
    msg_mgmt_env.queue  = msg_mgmt_queue;
    msg_mgmt_env.print  = msg_mgmt_print;
//    msg_mgmt_env.drain  = msg_mgmt_drain;
#ifdef IPC_SUPPORT_MSGACK
    msg_mgmt_env.llind  = msg_mgmt_llind;
#endif
    msg_mgmt_env.msgind = msg_mgmt_msgind;
#ifdef IPC_MSG_MALLOC
    msg_mgmt_env.alloc  = msg_mgmt_alloc;
    msg_mgmt_env.free   = msg_mgmt_free;
#endif
    msg_mgmt_env.priv   = priv;

#ifdef IPC_MSG_MALLOC
    msg_mgmt_env.msg_mem = rtos_malloc(sizeof(struct ipc_msg_block) * pool_size);
    msg = (struct ipc_msg_block*)(msg_mgmt_env.msg_mem);
    if (msg == NULL)
    {
        ipc_err("%s: failed to malloc\n",__func__);
        return NULL;
    }

    pool = &msg_mgmt_env.msg_pool;
    for (i = 0; i < pool_size; i++)
    {
        item = &msg[i].list;
        dl_list_add(pool, item);
    }
#endif
    return &msg_mgmt_env;
}

/**
 *
 */
void ipc_msg_mgmt_deinit(struct ipc_msg_mgmt *msg_mgmt)
{
    msg_mgmt->print(msg_mgmt);
    msg_mgmt->drain(msg_mgmt);
    msg_mgmt->print(msg_mgmt);
#ifdef IPC_MSG_MALLOC
    if (msg_mgmt->msg_mem)
        rtos_free(msg_mgmt->msg_mem);
#endif
    memset(msg_mgmt, 0, sizeof(*msg_mgmt));
}

int32_t ipc_msg_send(struct ipc_ep *ep, void *data, uint32_t len, void *resp)
{
#ifdef IPC_MSG_MALLOC
    struct ipc_msg_block *msgb = NULL;
#else
    struct ipc_msg_block msg_block;
    struct ipc_msg_block *msgb = &msg_block;
#endif
    struct ipc_msg_desc msg;

    msg.hdr.dst_id   = ep->remote_eid;
    msg.hdr.src_id   = ep->local_eid;
    msg.hdr.data_len = len;
    msg.data = data;
#ifdef IPC_MSG_MALLOC
    msgb = msg_mgmt_env.alloc(&msg_mgmt_env);
    if (msgb == NULL)
    {
        ipc_err("Failed to malloc msg\n");
        return -1;
    }
#endif

    msgb->msg  = msg;
    msgb->resp = resp;
    msgb->complete_param = NULL;

    if (resp)
        msgb->flags = IPC_MSG_FLAG_WAIT_CFM;
    else
        msgb->flags = IPC_MSG_FLAG_NONBLOCK;

#ifdef IPC_DEBUG
    ipc_msg_print_str("MsgSnd", msg.hdr.src_id.val, msg.hdr.dst_id.val);
#endif
    return msg_mgmt_env.queue(&msg_mgmt_env, msgb);
}

int32_t ipc_msg_process(void *msg)
{
    return msg_mgmt_env.msgind(&msg_mgmt_env, msg);
}

void ipc_msg_print(void)
{
    msg_mgmt_env.print(&msg_mgmt_env);
}

/*
 * ipc_msg_release  - 当应用从ipc ring buffer中接收到一条消息后，还会占用ring buffer。处理完消息后将buffer重新放入ipc ring中
 *
 * @param chan      - 接收消息的ipc channel
 * @param msg       - 消息指针
 *
 * @return       - 无
 *
 */
void ipc_msg_release(uint32_t chan, void *msg)
{
    struct ipc_ccb *ccb;

    ccb = ipc_get_ccb(chan, CHAN_LOCAL);
    if (ccb)
        ipc_free_rbuffer(ccb, msg, 0);
}

/*
 * ipc_msg_reply    - 接收到一条消息后，回复一条消息。用于接收到一条消息后，记录src_id和dst_id，然后直接回复，不用去判断channel和ep了
 *
 * @param dst_id    - 消息接收端的id，包含了channel id和ep id，来自接收消息
 * @param src_id    - 消息放送端的id，包含了channel id和ep id，来自接收消息
 * @param len       - 消息长度
 * @param data      - 消息指针
 *
 * @return          - 0 成功，1 失败
 *
 */
int32_t ipc_msg_reply(uint16_t dst_id, uint16_t src_id, int32_t len, void *data)
{
    int32_t ret = 0;
    struct ipc_msg_desc desc;
    struct ipc_ccb *ccb;

    ccb = ipc_get_ccb((uint32_t)(((union ipc_eid)dst_id).id.chan), CHAN_REMOTE);
    if (ccb)
    {
        desc.hdr.dst_id   = (union ipc_eid)dst_id;
        desc.hdr.src_id   = (union ipc_eid)src_id;
        desc.hdr.data_len = len;
        desc.data = data;
        ipc_dbg("%s len %d\n", __func__, len);
        if (ipc_sendto(ccb, &desc, IPC_TIMEOUT) != IPC_ERR_OK)
            ret = -1;
    }

    return ret;
}
