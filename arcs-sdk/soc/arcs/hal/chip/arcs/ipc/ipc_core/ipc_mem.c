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
}
