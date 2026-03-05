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
#include "ipc_shared.h"

extern uint8_t _ipcshram[];
extern struct ipc_shared_env_tag ipc_shared_env;

void ipc_mem_init(int32_t wifi_ipc_mode)
{
    int32_t i, size, *dst;
    struct ipc_shared_env_tag *mem;

    mem = (struct ipc_shared_env_tag*)_ipcshram;
    /*Initialize IPC shared memory*/
    if (wifi_ipc_mode)
    {
        mem->hdr.pattern1 = IPC_PATTERN1;
        mem->hdr.pattern2 = IPC_PATTERN2;
    }
    else
    {
        mem->hdr.pattern1 = 0xFFFFFFFF;
        mem->hdr.pattern2 = 0xFFFFFFFF;
    }
    dst  = (int32_t*)mem;
    dst += 2;

    size = sizeof(struct ipc_shared_env_tag) - 8;
    for (i = 0; i < size; i += 4)
        *dst++ = 0;
    mem->hdr.config_addr = (uint32_t)mem->config;
    mem->hdr.config_len  = sizeof(mem->config);

    ipc_queue_ring_init(&ipc_shared_env.msg_c2a_buf.ring, ipc_shared_env.msg_c2a_buf.items, sizeof(struct ipc_epmsg_c2a_msg), IPC_MSGC2A_BUF_CNT);
    ipc_queue_ring_init(&ipc_shared_env.msg_a2c_buf.ring, ipc_shared_env.msg_a2c_buf.items, sizeof(struct ipc_epmsg_a2c_msg), IPC_MSGA2C_BUF_CNT);
#ifdef CFG_AMP_IPC_WIFI_CHAN
    ipc_queue_ring_init(&ipc_shared_env.rxdesc.ring, ipc_shared_env.rxdesc.items, sizeof(struct ipc_epmsg_rxdesc), IPC_RXDESC_CNT);
    ipc_queue_ring_init(&ipc_shared_env.rxcfm.ring, ipc_shared_env.rxcfm.items, sizeof(struct ipc_epmsg_rxcfm), IPC_RXCFM_CNT);
    ipc_queue_ring_init(&ipc_shared_env.txcfm.ring, ipc_shared_env.txcfm.items, sizeof(struct ipc_epmsg_txcfm), IPC_TXCFM_CNT);
    ipc_queue_ring_init(&ipc_shared_env.txdesc.ring, ipc_shared_env.txdesc.items, sizeof(struct ipc_epmsg_txdesc), IPC_TXDESC_CNT);
#endif
}
