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
#include "core_feature_base.h"
#include "ipc_shared.h"
#include "ipc_mem.h"
#include "ipc_queue.h"


extern struct ipc_shared_env_tag ipc_shared_env;

static bool ipc_mem_ring_valid(volatile struct vring_hdr *ring,
                               volatile struct vring_desc *desc,
                               volatile struct vring_avail *avail,
                               volatile struct vring_ready *ready,
                               volatile void *items,
                               uint32_t item_size,
                               uint32_t item_num)
{
    uintptr_t item_addr = (uintptr_t)items;

    if ((ring->desc != desc) || (ring->avail != avail) || (ring->ready != ready))
        return false;

    if ((item_num < 2) || (ring->avail_wr_idx != item_num))
        return false;

    if ((desc[0].addr != item_addr) || (desc[1].addr != item_addr + item_size))
        return false;

    if ((desc[0].len != item_size) || (desc[1].len != item_size))
        return false;

    return true;
}

static int32_t ipc_mem_init_region(struct ipc_shared_env_tag *mem)
{
    if (mem == NULL)
        return -1;

    memset(mem, 0, sizeof(struct ipc_shared_env_tag));

    ipc_queue_ring_init(&mem->master_msg_buf.ring, mem->master_msg_buf.items, sizeof(struct ipc_epmsg_master_msg), IPC_MASTER_MSG_BUF_CNT);
    ipc_queue_ring_init(&mem->slave_msg_buf.ring, mem->slave_msg_buf.items, sizeof(struct ipc_epmsg_slave_msg), IPC_SLAVE_MSG_BUF_CNT);
#ifdef CFG_AMP_IPC_WIFI_CHAN
    ipc_queue_ring_init(&mem->wifi.rxdesc.ring, mem->wifi.rxdesc.items, sizeof(struct ipc_wifi_epmsg_rxdesc), IPC_WIFI_RXDESC_CNT);
    ipc_queue_ring_init(&mem->wifi.rxcfm.ring, mem->wifi.rxcfm.items, sizeof(struct ipc_wifi_epmsg_rxcfm), IPC_WIFI_RXCFM_CNT);
    ipc_queue_ring_init(&mem->wifi.txcfm.ring, mem->wifi.txcfm.items, sizeof(struct ipc_wifi_epmsg_txcfm), IPC_WIFI_TXCFM_CNT);
    ipc_queue_ring_init(&mem->wifi.txdesc.ring, mem->wifi.txdesc.items, sizeof(struct ipc_wifi_epmsg_txdesc), IPC_WIFI_TXDESC_CNT);
#endif
#ifdef CFG_AMP_IPC_BT_CHAN
    ipc_queue_ring_init(&mem->bt.h2c.ring, mem->bt.h2c.items, sizeof(struct bt_ipc_epmsg), IPC_BT_H2C_CNT);
    ipc_queue_ring_init(&mem->bt.c2h.ring, mem->bt.c2h.items, sizeof(struct bt_ipc_epmsg), IPC_BT_C2H_CNT);
#endif

    /* Publish the patterns last so the peer never accepts a partial layout. */
    mem->hdr.config_addr = (uint32_t)mem->config;
    mem->hdr.config_len  = sizeof(mem->config);
    __RWMB();
    mem->hdr.pattern1 = IPC_PATTERN1;
    mem->hdr.pattern2 = IPC_PATTERN2;
    __RWMB();

    return 0;
}

void ipc_mem_init(void)
{
    ipc_mem_init_region(&ipc_shared_env);
}

int32_t ipc_mem_validate(void)
{
    volatile struct ipc_shared_env_tag *mem = &ipc_shared_env;

    if ((mem->hdr.pattern1 == 0) || (mem->hdr.pattern2 == 0))
        return IPC_ERR_NOT_READY;

    if ((mem->hdr.pattern1 != IPC_PATTERN1) || (mem->hdr.pattern2 != IPC_PATTERN2))
        return IPC_ERR_INVALID;

    __RWMB();
    if ((mem->hdr.config_addr != (uint32_t)mem->config) ||
        (mem->hdr.config_len != sizeof(mem->config)))
        return IPC_ERR_INVALID;

    if (!ipc_mem_ring_valid(&mem->master_msg_buf.ring,
                            mem->master_msg_buf.desc,
                            mem->master_msg_buf.avail,
                            mem->master_msg_buf.ready,
                            mem->master_msg_buf.items,
                            sizeof(struct ipc_epmsg_master_msg),
                            IPC_MASTER_MSG_BUF_CNT))
        return IPC_ERR_INVALID;

    if (!ipc_mem_ring_valid(&mem->slave_msg_buf.ring,
                            mem->slave_msg_buf.desc,
                            mem->slave_msg_buf.avail,
                            mem->slave_msg_buf.ready,
                            mem->slave_msg_buf.items,
                            sizeof(struct ipc_epmsg_slave_msg),
                            IPC_SLAVE_MSG_BUF_CNT))
        return IPC_ERR_INVALID;

    return IPC_ERR_OK;
}
