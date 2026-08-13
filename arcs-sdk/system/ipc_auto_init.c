/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sys_init.h"

#if CONFIG_ARCS_HAL_IPC
#include <stdbool.h>
#include <stdio.h>

#include "ic_lock.h"
#include "ipc_shared.h"
#include "systick.h"

#if defined(CFG_AMP_IPC_SLAVE)
#include "ipc_slave.h"
#elif defined(CFG_AMP_IPC_MASTER)
#include "ipc_master.h"
#endif

static bool s_ipc_auto_inited;

#define IPC_SHARED_READY_TIMEOUT_MS 500U

__attribute__((weak)) void ipc_auto_init_failure_handler(int error)
{
    (void)error;
}

#if defined(CFG_AMP_IPC_MASTER)
static int ipc_wait_for_shared_memory(void)
{
    int ret = IPC_ERR_NOT_READY;

    for (uint32_t elapsed_ms = 0; elapsed_ms < IPC_SHARED_READY_TIMEOUT_MS; elapsed_ms++) {
        ret = ipc_mem_validate();
        if (ret == IPC_ERR_OK) {
            return IPC_ERR_OK;
        }
        if (ret != IPC_ERR_NOT_READY) {
            return ret;
        }
        SysTick_Delay_Ms(1);
    }

    return IPC_ERR_TIMEOUT;
}
#endif

static int ipc_auto_init(void)
{
    int ret = 0;

    if (s_ipc_auto_inited) {
        return 0;
    }

#if CONFIG_ARCS_AP_CORE
    ipc_mem_init();
#endif
    ic_lock_init();

#if defined(CFG_AMP_IPC_SLAVE)
    ret = ipc_slave_init();
#elif defined(CFG_AMP_IPC_MASTER)
    ret = ipc_wait_for_shared_memory();
    if (ret == 0) {
        ret = ipc_master_init();
    }
#endif

    if (ret == 0) {
        s_ipc_auto_inited = true;
    } else {
        printf("IPC auto init failed: %d\n", ret);
        ipc_auto_init_failure_handler(ret);
    }

    return ret;
}

SYS_INIT(ipc_auto_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, SYS_INIT_SUB_PRIORITY_MIN);
#endif
