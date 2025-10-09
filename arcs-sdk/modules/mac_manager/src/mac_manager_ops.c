/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mac_manager_ops.h"
#include "port/mem/sys_heap/mac_manager_mem_ops_sys_heap.h"

#ifdef CONFIG_MAC_MANAGER_MAC_IN_LISA_KV
#include "port/content/lisa_kv/mac_manager_content_ops_lisa_kv.h"
#endif

#ifdef CONFIG_MAC_MANAGER_MAC_BY_CHIP_ID
#include "port/content/chip_id/mac_manager_content_ops_chip_id.h"
#endif

static mac_manager_ops_t g_mac_manager_ops = {
    .mem_ops = &mac_manager_mem_ops_sys_heap,
#ifdef CONFIG_MAC_MANAGER_MAC_IN_LISA_KV
    .content_ops = &mac_manager_content_ops_lisa_kv,
#elif defined(CONFIG_MAC_MANAGER_MAC_BY_CHIP_ID)
    .content_ops = &mac_manager_content_ops_chip_id,
#else
    .content_ops = NULL,
#endif
};

mac_manager_ops_t* mac_manager_ops_get(void)
{
    return &g_mac_manager_ops;
}