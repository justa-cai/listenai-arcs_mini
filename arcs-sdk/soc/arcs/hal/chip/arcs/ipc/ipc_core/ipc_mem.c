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

static bool ipc_mem_is_initialized(const struct ipc_shared_env_tag *mem, int32_t wifi_ipc_mode)
{
    uint32_t pattern1;
    uint32_t pattern2;

    if (wifi_ipc_mode)
    {
        pattern1 = IPC_PATTERN1;
        pattern2 = IPC_PATTERN2;
    }
    else
    {
        pattern1 = 0xFFFFFFFF;
        pattern2 = 0xFFFFFFFF;
    }

    return (mem->hdr.pattern1 == pattern1) &&
           (mem->hdr.pattern2 == pattern2) &&
           (mem->hdr.config_addr == (uint32_t)mem->config) &&
           (mem->hdr.config_len == sizeof(mem->config));
}

void ipc_mem_init(int32_t wifi_ipc_mode)
{
    int32_t i, size, *dst;
    struct ipc_shared_env_tag *mem;
    uint32_t pattern1;
    uint32_t pattern2;

    mem = (struct ipc_shared_env_tag*)_ipcshram;

#ifdef CFG_AMP_IPC_MASTER
    /*
     * AP/slave boots first and owns shared IPC memory initialization.
     * Master reuses the existing layout so early slave-to-master log messages
     * are not cleared before the master attaches.
     */
    if (ipc_mem_is_initialized(mem, wifi_ipc_mode))
        return;
#endif

    if (wifi_ipc_mode)
    {
        pattern1 = IPC_PATTERN1;
        pattern2 = IPC_PATTERN2;
    }
    else
    {
        pattern1 = 0xFFFFFFFF;
        pattern2 = 0xFFFFFFFF;
    }

    /* Initialize IPC shared memory before publishing the ready header. */
    dst = (int32_t*)mem;
    size = sizeof(struct ipc_shared_env_tag);
    for (i = 0; i < size; i += 4)
        *dst++ = 0;

    ipc_queue_ring_init(&mem->msg_c2a_buf.ring, mem->msg_c2a_buf.items, sizeof(struct ipc_epmsg_c2a_msg), IPC_MSGC2A_BUF_CNT);
    ipc_queue_ring_init(&mem->msg_a2c_buf.ring, mem->msg_a2c_buf.items, sizeof(struct ipc_epmsg_a2c_msg), IPC_MSGA2C_BUF_CNT);
#ifdef CFG_AMP_IPC_WIFI_CHAN
    ipc_queue_ring_init(&mem->rxdesc.ring, mem->rxdesc.items, sizeof(struct ipc_epmsg_rxdesc), IPC_RXDESC_CNT);
    ipc_queue_ring_init(&mem->rxcfm.ring, mem->rxcfm.items, sizeof(struct ipc_epmsg_rxcfm), IPC_RXCFM_CNT);
    ipc_queue_ring_init(&mem->txcfm.ring, mem->txcfm.items, sizeof(struct ipc_epmsg_txcfm), IPC_TXCFM_CNT);
    ipc_queue_ring_init(&mem->txdesc.ring, mem->txdesc.items, sizeof(struct ipc_epmsg_txdesc), IPC_TXDESC_CNT);
#endif

    mem->hdr.config_addr = (uint32_t)mem->config;
    mem->hdr.config_len  = sizeof(mem->config);
    __asm__ volatile ("fence iorw,iorw" : : : "memory");
    mem->hdr.pattern1 = pattern1;
    mem->hdr.pattern2 = pattern2;
}
