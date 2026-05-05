/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sys_init.h"

#if CONFIG_ARCS_HAL_IPC_PRINT && !CONFIG_WIFI_LWIP_DIFF_CORE
#include "ic_lock.h"

#if defined(CFG_AMP_IPC_SLAVE)
#include "ipc_slave.h"

static int ipc_slave_print_auto_init(void)
{
    ic_lock_init();
    ipc_mem_init(0);
    return ipc_slave_init(NULL);
}

SYS_INIT(ipc_slave_print_auto_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 0);
#elif defined(CFG_AMP_IPC_MASTER)
#include "ipc_master.h"

static int ipc_master_print_auto_init(void)
{
    ic_lock_init();
    ipc_mem_init(0);
    return ipc_master_init(NULL);
}

SYS_INIT(ipc_master_print_auto_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 5);
#endif

#endif
