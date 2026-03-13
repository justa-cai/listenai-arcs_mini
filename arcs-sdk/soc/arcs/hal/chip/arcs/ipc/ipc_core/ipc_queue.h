/**
 ****************************************************************************************
 *
 * Copyright (C) ListenAI 2023
 *
 *
 ****************************************************************************************
*/
#ifndef _IPC_QUEUE_H_
#define _IPC_QUEUE_H_

#include "ipc_platform.h"

#define IPC_QUEUE_STATUS_INIT       0
#define IPC_QUEUE_STATUS_RX         1
#define IPC_QUEUE_STATUS_BUSY       2

struct vring_desc
{
    uint32_t addr;
    /* Length. */
    uint16_t len;
    /* The flags as indicated above. */
    uint16_t flags;
};

struct vring_ready
{
//    uint16_t flags;
    uint16_t desc_idx;
};

struct vring_avail
{
//    uint16_t flags;
    uint16_t desc_idx;
};

struct vring_hdr
{
    uint32_t status; /*write pointer index*/
    uint16_t ready_wr_idx; /*write pointer index*/
    uint16_t avail_wr_idx; /*write pointer index*/
    struct vring_desc  *desc;
    struct vring_avail *avail;
    struct vring_ready *ready;
};

struct ipc_queue
{
    uint16_t q_item_num;
    uint16_t q_item_size; /*element size*/
//    uint16_t q_free_cnt;
//    uint16_t q_queued_cnt;
    uint16_t q_avail_rd_idx;/*read pointer index*/
    uint16_t q_ready_rd_idx;/*read pointer index*/
    volatile struct vring_hdr *vring;
};

void *ipc_queue_get_avail_buffer(struct ipc_queue *q, uint16_t *len);
void *ipc_queue_get_ready_buffer(struct ipc_queue *q, uint32_t *len);
int32_t ipc_queue_rx_free(struct ipc_queue *q, uint16_t desc_idx);
int32_t ipc_queue_tx(struct ipc_queue *q, uint16_t desc_idx);
int32_t ipc_queue_full(struct ipc_queue *q);
int32_t ipc_queue_empty(struct ipc_queue *q);
int32_t ipc_queue_status_get(struct ipc_queue *q);
void ipc_queue_status_set(struct ipc_queue *q, uint32_t status);
void ipc_queue_ring_init(volatile struct vring_hdr *vring, volatile void *buf, int32_t item_size, int32_t item_num);
int32_t ipc_queue_init(struct ipc_queue *q, volatile struct vring_hdr *vring);

#endif
