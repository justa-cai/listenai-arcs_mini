/**
 ****************************************************************************************
 *
 * Copyright (C) ListenAI 2024
 *
 ****************************************************************************************
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ipc_queue.h>
#include <ipc_platform.h>
#include <ipc_core.h>



IPC_FUNC_ATTR static void vring_update_ready(struct ipc_queue *q, uint16_t desc_idx)
{
    uint16_t ready_idx;

    GLOBAL_INT_DISABLE();
    ready_idx = (uint16_t)(q->vring->ready_wr_idx & ((uint16_t)(q->q_item_num - 1U)));
    q->vring->ready[ready_idx].desc_idx = desc_idx;
    //__RWMB();
    q->vring->ready_wr_idx++;
    GLOBAL_INT_RESTORE();
}

IPC_FUNC_ATTR static void vring_update_avail(struct ipc_queue *q, uint16_t desc_idx)
{
    uint16_t avail_idx;

    GLOBAL_INT_DISABLE();
    avail_idx = (uint16_t)(q->vring->avail_wr_idx & ((uint16_t)(q->q_item_num - 1U)));
    q->vring->avail[avail_idx].desc_idx = desc_idx;
    //__RWMB();
    q->vring->avail_wr_idx++;
    GLOBAL_INT_RESTORE();
}

/*
 * ipc_queue_get_avail_buffer - Get a free buffer for tx
 *
 * @return                    - The available buffer address
 */
IPC_FUNC_ATTR void *ipc_queue_get_avail_buffer(struct ipc_queue *q, uint16_t *len)
{
    uint16_t desc_idx, avail_idx;
    struct vring_desc  *desc;

    GLOBAL_INT_DISABLE();
    if (q->q_avail_rd_idx != q->vring->avail_wr_idx)
    {
        avail_idx = (uint16_t)(q->q_avail_rd_idx & ((uint16_t)(q->q_item_num - 1U)));
        q->q_avail_rd_idx++;
        desc_idx = q->vring->avail[avail_idx].desc_idx;
        GLOBAL_INT_RESTORE();
        desc     = &q->vring->desc[desc_idx];

        if (len != NULL)
            *len = desc->len;
    }
    else
    {
        GLOBAL_INT_RESTORE();
        return NULL;
    }

    return (void*)(desc->addr);
}

/*
 * vring_get_ready_buffer    - Get a ready buffer for rx
 *
 * @return                   - The ready buffer address
 */
IPC_FUNC_ATTR void *ipc_queue_get_ready_buffer(struct ipc_queue *q, uint32_t *len)
{
    uint16_t desc_idx, ready_idx;
    struct vring_desc *desc;

    GLOBAL_INT_DISABLE();
    if (q->q_ready_rd_idx != q->vring->ready_wr_idx)
    {
        ready_idx = (uint16_t)(q->q_ready_rd_idx & ((uint16_t)(q->q_item_num - 1U)));
        desc_idx  = q->vring->ready[ready_idx].desc_idx;
        q->q_ready_rd_idx++;
        GLOBAL_INT_RESTORE();
        desc = &q->vring->desc[desc_idx];

        if (len != NULL)
            *len = desc->len;
    }
    else
    {
        GLOBAL_INT_RESTORE();
        return NULL;
    }

    return (void*)(desc->addr);
}

IPC_FUNC_ATTR int32_t ipc_queue_rx_free(struct ipc_queue *q, uint16_t desc_idx)
{
    if (desc_idx < q->q_item_num)
    {
        vring_update_avail(q, desc_idx);
    }
    else
    {
        ipc_err("Invalid desc idx:%d\n", desc_idx);
    }

    return 0;
}

IPC_FUNC_ATTR int32_t ipc_queue_tx(struct ipc_queue *q, uint16_t desc_idx)
{
    if (desc_idx < q->q_item_num)
    {
        vring_update_ready(q, desc_idx);
    }
    else
    {
        ipc_err("Invalid desc idx:%d\n", desc_idx);
    }

    return 0;
}

int32_t ipc_queue_full(struct ipc_queue *q)
{
    return (q->q_avail_rd_idx == q->vring->avail_wr_idx);
}

int32_t ipc_queue_empty(struct ipc_queue *q)
{
    return (q->q_ready_rd_idx == q->vring->ready_wr_idx);
}

int32_t ipc_queue_status_get(struct ipc_queue *q)
{
    return q->vring->status;
}

void ipc_queue_status_set(struct ipc_queue *q, uint32_t status)
{
    q->vring->status = status;
}

int32_t ipc_queue_init(struct ipc_queue *q, volatile struct vring_hdr *vring)
{
    if (vring == NULL || vring->desc == NULL)
    {
        return -1;
    }

    q->vring = vring;
    q->q_avail_rd_idx = 0;
    q->q_ready_rd_idx = 0;
    q->q_item_num     = vring->avail_wr_idx;
    q->q_item_size    = vring->desc[1].addr - vring->desc[0].addr;

    return 0;
}

void ipc_queue_ring_init(volatile struct vring_hdr *vring, volatile void *buf, int32_t item_size, int32_t item_num)
{
    int32_t i;
    struct vring_desc  *desc;
    struct vring_avail *avail;
    struct vring_ready *ready;
    struct ipc_epmsg_hdr *epmsg_hdr;

    if ((item_num < 2) || (item_num & (item_num - 1)))
    {
        ipc_err("IPC: invalid param");
        return;
    }
    desc  = (struct vring_desc*)(vring + 1);
    avail = (struct vring_avail*)((uint8_t*)desc + sizeof(struct vring_desc) * item_num);
    ready = (struct vring_ready*)((uint8_t*)avail + sizeof(struct vring_avail) * item_num);
    vring->desc  = desc;
    vring->avail = avail;
    vring->ready = ready;
    vring->avail_wr_idx = item_num;
    vring->ready_wr_idx = 0;

    for (i = 0; i < item_num; i++)
    {
        epmsg_hdr = (struct ipc_epmsg_hdr*)(buf + i * item_size);
        epmsg_hdr->desc_idx  = i;
        vring->desc[i].addr  = (uint32_t)epmsg_hdr;
        vring->desc[i].len   = item_size;
        vring->avail[i].desc_idx = i;
    }
}
