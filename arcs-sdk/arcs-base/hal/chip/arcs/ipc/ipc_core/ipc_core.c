/**
 ****************************************************************************************
 *
 * Copyright (C) ListenAI 2024
 *
 *
 ****************************************************************************************
*/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <rtos_al.h>
#include "utils_math.h"
#include "ipc_core.h"
#include "ipc_queue.h"
#include "ipc_platform.h"
#include "ipc_ep.h"



static struct ipc_ccb *ipc_isr_eps[IPC_IRQ_MAX_NUM];

static struct ipc_instance ipc_dev;

inline static void ipc_lock(ipc_lock_t lock)
{
    if (lock)
    {
        IPC_CHAN_LOCK(lock);
    }
}

inline static void ipc_unlock(ipc_lock_t lock)
{
    if (lock)
    {
        IPC_CHAN_UNLOCK(lock);
    }
}

/*
 * @brief
 * When the remote endpoint failed to read data, it may enter a suspend state.
 * After sending, we query whether the remote endpoint needs an interrupt to wake up the task.
 *
 *
 */
static bool ipc_chan_need_notify(struct ipc_ccb *ccb)
{
    return (ipc_queue_status_get(ccb->vq) == IPC_QUEUE_STATUS_RX);
}

void ipc_status_set(struct ipc_ccb *ccb, uint32_t status)
{
    ipc_queue_status_set(ccb->vq, status);
}

static int32_t ipc_wait_rx_event(struct ipc_ccb *ccb, uint32_t timeout)
{
    int32_t ret = 1;

    ipc_queue_status_set(ccb->vq, IPC_QUEUE_STATUS_RX);
    ipc_dbg("%s: %d\n", __func__, ccb->chan);

    if (ipc_queue_empty(ccb->vq))
    {
        ret = ipc_platform_wait_event(&ccb->priv->cb_param, timeout);
        ret = ret > 0 ? 1:0;
        ipc_queue_status_set(ccb->vq, IPC_QUEUE_STATUS_BUSY);
    }

    return ret;
}

IPC_FUNC_ATTR static int8_t ipc_irq_handler(uint32_t event, uint32_t status)
{
    int32_t chan;
    struct ipc_ccb *ccb;

#ifdef IPC_STATS
    ipc_dev.rx_irq_cnt++;
#endif

    ipc_info("%s: 0x%08x\n",__func__, status);
    status = IPC_PLATFORM_IRQ_STATUS(status);
    while (status)
    {
        chan = 31 - co_clz(status);
        ccb  = ipc_isr_eps[chan];

        if (ccb)
        {
            if (ccb->flags & (IPC_CHAN_FLAGS_USER_MODE | IPC_CHAN_FLAGS_FAST))
            {
                /*Let user task to process the data in user tasks*/
                if (ccb->priv->callback)
                {
                    ipc_info("%s: cb:%p\n",__func__,ccb->priv->callback);
                    ccb->priv->callback(ccb, ccb->priv->cb_param);
                }
            }
            else
            {
                int32_t ret;
                struct ipc_msg_desc desc;
                struct ipc_epmsg *msg;
#ifdef IPC_MSG_SEGMENT
                int32_t i;
                struct ipc_epmsg *next;
#endif

                ipc_info("%s: cb:%p\n",__func__,ccb->priv->callback);
                /*Read data from the queue and call the user callback*/
                while ((msg = (struct ipc_epmsg*)ipc_queue_get_ready_buffer(ccb->vq, NULL)))
                {
                    ret = IPC_MSG_RELEASE;
                    desc.hdr.dst_id   = msg->hdr.dst_id;
                    desc.hdr.src_id   = msg->hdr.src_id;
                    desc.hdr.data_len = msg->hdr.len;
                    desc.data = msg->data;
#ifdef IPC_MSG_SEGMENT
                    if (desc.hdr.dst_id.val != IPC_INVALID_MSG_ID)
#endif
                    {
#ifdef IPC_MSG_SEGMENT
                        i = 0;
                        desc.hdr.total_len = msg->hdr.len;
                        desc.seg[0] = NULL;
                        next = msg->hdr.next;
                        while (next && (i < IPC_MSG_SEGMENT_MAX - 1))
                        {
                            desc.seg[i++] = next->data;
                            desc.hdr.total_len += next->hdr.len;
                            next = next->hdr.next;
                        }
#endif
                        ret = ccb->priv->callback(ccb, &desc);
                        IPC_STAT(ccb, rx, rx_ok)
                    }
                    if (ret == IPC_MSG_RELEASE)
                    {
#ifdef IPC_MSG_SEGMENT
                        do
                        {
                            next = msg->hdr.next;
#endif
                            ipc_queue_rx_free(ccb->vq, msg->hdr.desc_idx);
#ifdef IPC_MSG_SEGMENT
                            msg = next;
                        } while (msg);
#endif
                    }
                }
            }
        }

        status &= ~(1 << chan);
    }

    return 0;
}

IPC_FUNC_ATTR uint8_t* ipc_get_rbuffer(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, uint32_t timeout)
{
    struct ipc_epmsg *msg;

    msg = (struct ipc_epmsg*)ipc_queue_get_ready_buffer(ccb->vq, NULL);
    if (msg == NULL)
    {
        if (timeout == 0)
        {
            return NULL;
        }
        IPC_STAT(ccb, rx, rx_retry)
        ipc_lock(ccb->lock_for_ready);
        if (ipc_wait_rx_event(ccb, timeout))
            msg = (struct ipc_epmsg*)ipc_queue_get_ready_buffer(ccb->vq, NULL);
        ipc_unlock(ccb->lock_for_ready);
    }

    if (msg)
    {
#ifdef IPC_MSG_SEGMENT
        struct ipc_epmsg *next = msg;
#endif
        if (desc)
        {
#ifdef IPC_MSG_SEGMENT
            int32_t i = 0;
            void **ptr = &desc->data;
#endif

            desc->hdr.dst_id   = msg->hdr.dst_id;
            desc->hdr.src_id   = msg->hdr.src_id;
            desc->hdr.data_len = msg->hdr.len;

#ifdef IPC_MSG_SEGMENT
            desc->hdr.total_len = 0;
            desc->seg[0] = NULL;
            do
            {
                ipc_dbg("%s msg %p data %p idx %d\n", __func__, next, next->data, next->hdr.desc_idx);

                ptr[i] = next->data;
                desc->hdr.total_len += next->hdr.len;
                next   = next->hdr.next;
            } while ((++i < IPC_MSG_SEGMENT_MAX) && next);
            if (next != NULL)
            {
                ipc_err("Err: invalid segment\n");
            }
#else
            desc->data = msg->data;
#endif
        }
        IPC_STAT(ccb, rx, rx_ok);
        return (msg->data);
    }
    else
    {
        return NULL;
    }
}

int32_t ipc_free_rbuffer(struct ipc_ccb *ccb, uint8_t *buffer, int32_t size)
{
    struct ipc_epmsg *msg;

    msg = EPMSG_FROM_BUF(buffer);
#ifdef IPC_MSG_SEGMENT
    do
    {
#endif
        ipc_dbg("free: idx %d\n", msg->hdr.desc_idx);
        ipc_queue_rx_free(ccb->vq, msg->hdr.desc_idx);
#ifdef IPC_MSG_SEGMENT
        msg = msg->hdr.next;
    } while (msg);
#endif

    return 0;
}

int32_t ipc_recvfrom(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, uint32_t timeout)
{
    int32_t ret = IPC_ERR_INVALID;
    uint32_t size = 0;
    struct ipc_epmsg *msg;

    if (ccb)
    {
        msg = (struct ipc_epmsg*)ipc_queue_get_ready_buffer(ccb->vq, &size);
        if (msg == NULL)
        {
            if (timeout == 0)
            {
                return IPC_ERR_NO_MEM;
            }
            IPC_STAT(ccb, rx, rx_retry)
            ipc_lock(ccb->lock_for_ready);
            if (ipc_wait_rx_event(ccb, timeout))
                msg = (struct ipc_epmsg*)ipc_queue_get_ready_buffer(ccb->vq, &size);
            ipc_unlock(ccb->lock_for_ready);
        }

        if (msg)
        {
            size = (size < msg->hdr.len)? size : msg->hdr.len;
            memcpy(desc->data, msg->data, size);
            desc->hdr.dst_id = msg->hdr.dst_id;
            desc->hdr.dst_id = msg->hdr.src_id;
            desc->hdr.data_len = size;
            ipc_queue_rx_free(ccb->vq, msg->hdr.desc_idx);

            ret = IPC_ERR_OK;
            IPC_STAT(ccb, rx, rx_ok)
        }
    }

    return ret;
}

IPC_FUNC_ATTR uint8_t *ipc_get_tbuffer(struct ipc_ccb *ccb, uint16_t *size, uint32_t timeout)
{
    struct ipc_epmsg *msg;
    uint32_t delay, time_count;

    msg = (struct ipc_epmsg*)ipc_queue_get_avail_buffer(ccb->vq, size);

    if (msg == NULL)
    {
        if (timeout == 0)
        {
            return NULL;
        }
        IPC_STAT(ccb, tx, tx_retry)
        delay = IPC_POLL_SHORT_INTERVAL_MS;
        time_count = 0;
        do
        {
            if ((msg = (struct ipc_epmsg*)ipc_queue_get_avail_buffer(ccb->vq, size)))
                break;
            if (time_count > 2)
                delay = IPC_POLL_LONG_INTERVAL_MS;
            rtos_delay(delay);
            time_count += delay;
        } while (((uint32_t)time_count < (uint32_t)timeout));
        ipc_dbg("tcount:%d\n", time_count);
    }

    if (msg)
    {
#ifdef IPC_MSG_SEGMENT
        msg->hdr.next = NULL;
#endif
        if (size)
            *size -= sizeof(struct ipc_epmsg_hdr);
        return msg->data;
    }
    else
    {
        return NULL;
    }
}

int32_t ipc_send_tbuffer(struct ipc_ccb *ccb, struct ipc_msg_desc *desc)
{
    struct ipc_epmsg *msg;

    if (ccb)
    {
        msg = EPMSG_FROM_BUF(desc->data);
        msg->hdr.dst_id = desc->hdr.dst_id;
        msg->hdr.src_id = desc->hdr.src_id;
        msg->hdr.len    = desc->hdr.data_len;
        ipc_queue_tx(ccb->vq, msg->hdr.desc_idx);

        if (ipc_chan_need_notify(ccb))
        {
            ipc_platform_notify(IPC_GET_LINK_ID_FROM_CHAN(ccb->chan), IPC_GET_IRQ_ID_FROM_CHAN(ccb->chan));
            IPC_STAT(ccb, tx, send_notify);
        }
        else
        {
            ipc_dbg("%s: no notify %08x\n",__func__, ipc_queue_status_get(ccb->vq));
        }

        IPC_STAT(ccb, tx, tx_ok);
        ipc_dbg("%s 0x%08x\n",__func__, ipc_queue_status_get(ccb->vq));
    }

    return IPC_ERR_OK;
}

struct ipc_ccb *ipc_get_ccb(uint32_t chan, int32_t type)
{
    struct ipc_ccb *ccb = NULL, *cur;
    struct dl_list *ccb_list;

    if (type == CHAN_LOCAL)
        ccb_list = &ipc_dev.local_ccb;
    else
        ccb_list = &ipc_dev.remote_ccb;
    dl_list_for_each(cur, ccb_list, struct ipc_ccb, list)
    {
        if ((cur->chan == (uint8_t)chan))
        {
            ccb = cur;
            break;
        }
    }

    return ccb;
}

/*
 * ipc_send      - Send data to remote endpoint
 *
 * @param ccb    - Local endpoint
 * @dst          - Romote endpoint address
 * @timeout      - Timeout, 0 forever
 *
 * @return       - Success or not
 *
 */
IPC_FUNC_ATTR int32_t ipc_send(struct ipc_ccb *ccb, void *data, int32_t size, uint32_t timeout)
{
    int32_t ret = IPC_ERR_INVALID;
    struct ipc_msg_desc desc = {0};

    desc.hdr.data_len = size;
    desc.data = data;

    ret = ipc_sendto(ccb, &desc, timeout);

    return ret;
}
#ifndef IPC_MSG_SEGMENT
IPC_FUNC_ATTR int32_t ipc_sendto(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, uint32_t timeout)
{
    int8_t *buffer;
    int32_t ret = IPC_ERR_INVALID;
    uint32_t delay, time_count;
    uint16_t len = 0;
    struct ipc_epmsg *msg;

    ipc_dbg("%s: ccb:%p size:%d\n",__func__, ccb, desc->hdr.data_len);
    if (ccb)
    {
        if (!ccb->vq)
            return IPC_ERR_INVALID;

        if (desc->hdr.data_len > (ccb->vq->q_item_size - sizeof(struct ipc_epmsg)))
            return IPC_ERR_BUFF_SIZE;

        msg = (struct ipc_epmsg*)ipc_queue_get_avail_buffer(ccb->vq, &len);

        if (msg == NULL)
        {
            if (timeout == 0)
            {
                return IPC_ERR_NO_MEM;
            }
            IPC_STAT(ccb, tx, tx_retry)
            ipc_dbg("ipc retry\n");

            delay = IPC_POLL_SHORT_INTERVAL_MS;
            time_count = 0;
            do
            {
                if ((msg = (struct ipc_epmsg*)ipc_queue_get_avail_buffer(ccb->vq, &len)))
                    break;
                rtos_delay(delay);
                time_count += delay;
                if (time_count > 2)
                    delay = IPC_POLL_LONG_INTERVAL_MS;
            } while (((uint32_t)time_count < (uint32_t)timeout));
            ipc_dbg("tcount:%d\n", time_count);
        }
        ipc_dbg("%s: msg:%p, len:%d\n",__func__, msg, len);

        if (msg)
        {
            msg->hdr.dst_id = desc->hdr.dst_id;
            msg->hdr.src_id = desc->hdr.src_id;
            msg->hdr.len    = desc->hdr.data_len;
            memcpy(msg->data, desc->data, desc->hdr.data_len);
            ipc_queue_tx(ccb->vq, msg->hdr.desc_idx);
            if (ipc_chan_need_notify(ccb))
            {
                ipc_platform_notify(IPC_GET_LINK_ID_FROM_CHAN(ccb->chan), IPC_GET_IRQ_ID_FROM_CHAN(ccb->chan));
                IPC_STAT(ccb, tx, send_notify)
            }
            else
            {
                ipc_dbg("%s: no notify %08x\n",__func__, ipc_queue_status_get(ccb->vq));
            }

            IPC_STAT(ccb, tx, tx_ok)
            ret = IPC_ERR_OK;
        }
        else
        {
            ret = IPC_ERR_TIMEOUT;
            ipc_err("ipc failed %d\n", ipc_buf_full(ccb));
        }
    }

    return ret;
}
#else
IPC_FUNC_ATTR int32_t ipc_sendto(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, uint32_t timeout)
{
    int8_t *buffer;
    int32_t ret = IPC_ERR_INVALID, seg_num;
    uint32_t delay, time_count, alloc_failed;
    uint16_t remain, buffer_len;
    struct ipc_epmsg *msg[IPC_MSG_SEGMENT_MAX];

    ipc_dbg("%s: ccb:%p size:%d\n",__func__, ccb, desc->hdr.data_len);
    if (ccb)
    {
        if (!ccb->vq)
            return IPC_ERR_INVALID;

        if (desc->hdr.data_len > (ccb->vq->q_item_size - sizeof(struct ipc_epmsg_hdr)) * IPC_MSG_SEGMENT_MAX)
            return IPC_ERR_BUFF_SIZE;

        buffer_len = ccb->vq->q_item_size - sizeof(struct ipc_epmsg_hdr);
        remain  = desc->hdr.data_len;
        seg_num = 0;
        delay   = IPC_POLL_SHORT_INTERVAL_MS;
        time_count   = 0;
        alloc_failed = 0;

        do
        {
            if ((msg[seg_num] = (struct ipc_epmsg*)ipc_queue_get_avail_buffer(ccb->vq, NULL)))
            {
                if (seg_num)
                    msg[seg_num - 1]->hdr.next = msg[seg_num];
                msg[seg_num]->hdr.next = NULL;
                seg_num++;

                if (remain <= buffer_len)
                {
                    remain = 0;
                    break;
                }
                else
                {
                    remain -= buffer_len;
                }
            }
            else if (timeout)
            {
                rtos_delay(delay);
                time_count += delay;
                if (time_count > 2)
                    delay = IPC_POLL_LONG_INTERVAL_MS;
            }
            else
            {
                return IPC_ERR_NO_MEM;
            }
        } while ((uint32_t)time_count < (uint32_t)timeout);

        if (time_count)
        {
            IPC_STAT(ccb, tx, tx_retry);
            ipc_dbg("tx retry:%d\n", time_count);
        }

        if (remain)
        {
            ret = IPC_ERR_TIMEOUT;
            alloc_failed = 1;
        }
        /*可能分配成功，也没有分配足够的msg*/
        if (seg_num)
        {
            void *tmp;
            int32_t i;
            struct ipc_epmsg *ptr;

            tmp    = desc->data;
            remain = desc->hdr.data_len;
            for (i = 0; i < seg_num; i++)
            {
                ptr = msg[i];
                if (!alloc_failed)
                {
                    ptr->hdr.dst_id = desc->hdr.dst_id;
                    ptr->hdr.src_id = desc->hdr.src_id;
                    /*ipc_epmsg->hdr.len是本包长度*/
                    ipc_dbg("alloc %d of %d msg %p data %p idx %d\n", i, seg_num, ptr, ptr->data, ptr->hdr.desc_idx);

                    if (remain <= buffer_len)
                    {
                        ptr->hdr.len = remain;
                        memcpy(ptr->data, tmp, remain);
                        break;
                    }
                    else
                    {
                        memcpy(ptr->data, tmp, buffer_len);
                        ptr->hdr.len = buffer_len;
                        remain -= buffer_len;
                        tmp    += buffer_len;
                    }
                }
                else
                {
                    /*如果没有分配到足够的buffer，但是发送方无法把已经分配的msg挂到ready链上，只能发送出去让接收方去释放*/
                    ptr->hdr.dst_id.val = IPC_INVALID_MSG_ID;
                    ptr->hdr.src_id.val = IPC_INVALID_MSG_ID;
                    ptr->hdr.len        = 0;
                }
            }

            /*只把第一个msg挂到ready链上，接收方根据next指针获取后面的msg*/
            ipc_queue_tx(ccb->vq, msg[0]->hdr.desc_idx);
            if (ipc_chan_need_notify(ccb))
            {
                ipc_platform_notify(IPC_GET_LINK_ID_FROM_CHAN(ccb->chan), IPC_GET_IRQ_ID_FROM_CHAN(ccb->chan));
                IPC_STAT(ccb, tx, send_notify)
            }
            else
            {
                ipc_dbg("%s: no notify %08x\n",__func__, ipc_queue_status_get(ccb->vq));
            }
            if (!alloc_failed)
            {
                IPC_STAT(ccb, tx, tx_ok);
                ret = IPC_ERR_OK;
            }
        }

        if (alloc_failed)
        {
            ret = IPC_ERR_TIMEOUT;
            ipc_err("ipc failed %d\n", ipc_buf_full(ccb));
        }
    }

    return ret;
}
#endif
int32_t ipc_buf_full(struct ipc_ccb *ccb)
{
    return ipc_queue_full(ccb->vq);
}

/*
 * ipc_shared_queue_init  - Init IPC memory
 *
 * @param size_info       - The unit num and unit size of the ring
 *                          the unit num must be power of two (2, 4, ...).
 * @return                - struct ipc_ccb_t
 *
 */
struct ipc_queue* ipc_shared_queue_init(bool master, volatile struct vring_hdr *vring, volatile void *buf, int32_t item_size, int32_t item_num)
{
    struct ipc_queue *queue;

    if ((item_num < 2) || (item_num & (item_num-1)))
        return NULL;

    queue = (struct ipc_queue*)rtos_malloc(sizeof(struct ipc_queue));
    if (queue)
        ipc_queue_init(master, queue, vring, buf, item_size, item_num);

    return queue;
}

uint32_t ipc_get_fast_notify_status(void)
{
    return ipc_dev.local_status->fast_notify_status;
}

void ipc_fast_notify(uint32_t chan, int32_t event)
{
    if (IPC_GET_LINK_ID_FROM_CHAN(chan) == CORE_PEER)
        ipc_dev.remote_status->fast_notify_status |= event;
    else
        ipc_dev.local_status->fast_notify_status  |= event;

    ipc_platform_notify(IPC_GET_LINK_ID_FROM_CHAN(chan), IPC_GET_IRQ_ID_FROM_CHAN(chan));
    ipc_info("%s port 0x%x, 0x%x\n",__func__, chan, event);
}

struct ipc_ccb* ipc_chan_create(char *name, int32_t chan, struct ipc_queue *vq, ipc_chan_callback_t cb, void *cb_param, uint32_t flags)
{
    struct ipc_ccb *ccb;
    struct ipc_chan_priv *priv;
    int32_t irq = IPC_GET_IRQ_ID_FROM_CHAN(chan);

    if (irq >= IPC_IRQ_MAX_NUM)
        return NULL;

    ccb = rtos_malloc(sizeof(struct ipc_ccb));
    if (ccb == NULL)
        return NULL;
    ipc_dbg("%s %p 0x%x\n",__func__, ccb, chan);
    memset(ccb, 0, sizeof(struct ipc_ccb));
#ifdef IPC_STATS
    strncpy(ccb->name, name, IPC_CCB_NAME_SIZE);
#endif
    ccb->flags = flags;
    ccb->chan  = chan;
    if (vq)
    {
        ccb->vq = vq;
        if (!(flags & IPC_CHAN_FLAGS_NO_LOCK))
        {
            IPC_CHAN_INIT(ccb->lock_for_ready);
            //IPC_CHAN_INIT(ccb->lock_for_avail);
        }
    }

    if (!(flags & IPC_CHAN_FLAGS_REMOTE))
    {
        priv = rtos_malloc(sizeof(struct ipc_chan_priv));
        if (priv == NULL)
        {
            rtos_free(ccb);
            return NULL;
        }

        priv->callback   = cb;
        priv->cb_param   = cb_param;
        ccb->priv        = priv;
        ipc_isr_eps[irq] = ccb;
        dl_list_init(&ccb->priv->epts);
        dl_list_add(&ipc_dev.local_ccb, &ccb->list);
    }
    else
    {
        dl_list_add(&ipc_dev.remote_ccb, &ccb->list);
    }
#ifdef IPC_STATS
    if (vq)
    {
        ccb->stats.q_num  = vq->q_item_num;
        ccb->stats.q_size = vq->q_item_size;
    }
#endif

    return ccb;
}

union ipc_eid ipc_get_eid(uint32_t chan, uint32_t idx)
{
    union ipc_eid msg_id;

    msg_id.id.chan = (uint8_t)chan;
    msg_id.id.ep   = (uint8_t)idx;

    return msg_id;
}

int32_t ipc_chan_destory(struct ipc_ccb *ccb)
{
	struct ipc_ep *cur;

    if (ccb)
    {
        dl_list_del(&ccb->list);
        dl_list_for_each(cur, &ccb->priv->epts, struct ipc_ep, list)
        {
            dl_list_del(&cur->list);
            rtos_free(cur);
        }
        if (!(ccb->flags & IPC_CHAN_FLAGS_NO_LOCK))
        {
            //IPC_CHAN_DEINIT(ccb->lock_for_avail);
            IPC_CHAN_DEINIT(ccb->lock_for_ready);
        }
        rtos_free(ccb->vq);
        rtos_free(ccb->priv);
        rtos_free(ccb);
    }

    return 0;
}

#ifdef IPC_STATS
struct ipc_instance* ipc_get_ep_dump(void)
{
	int32_t i = 0;
    struct ipc_ccb *ccb;
    struct dl_list *hdr[2] = {&ipc_dev.local_ccb, &ipc_dev.remote_ccb};

    while (i < 2)
    {
        dl_list_for_each(ccb, hdr[i], struct ipc_ccb, list)
        {
            if (!(ccb->flags & IPC_CHAN_FLAGS_FAST))
            {
                if (ccb->flags & IPC_CHAN_FLAGS_REMOTE)
                {
                    ccb->stats.tx.txq_avail_wr_idx = ccb->vq->vring->avail_wr_idx;
                    ccb->stats.tx.txq_ready_wr_idx = ccb->vq->vring->ready_wr_idx;
                    ccb->stats.tx.txq_avail_rd_idx = ccb->vq->q_avail_rd_idx;
                    ccb->stats.tx.txq_ready_rd_idx = ccb->vq->q_ready_rd_idx;
                }
                else
                {
                    ccb->stats.rx.rxq_avail_wr_idx = ccb->vq->vring->avail_wr_idx;
                    ccb->stats.rx.rxq_ready_wr_idx = ccb->vq->vring->ready_wr_idx;
                    ccb->stats.rx.rxq_ready_rd_idx = ccb->vq->q_ready_rd_idx;
                    ccb->stats.rx.rxq_avail_rd_idx = ccb->vq->q_avail_rd_idx;
                }
            }
        }
        i++;
    }

    return &ipc_dev;
}
#endif
void ipc_init(uint32_t link_id, volatile struct ipc_status *local, volatile struct ipc_status *remote)
{
	ipc_dev.link_id = link_id;
    ipc_dev.local_status  = local;
    ipc_dev.remote_status = remote;
    dl_list_init(&ipc_dev.local_ccb);
    dl_list_init(&ipc_dev.remote_ccb);
    ipc_platform_init(ipc_irq_handler);
}
