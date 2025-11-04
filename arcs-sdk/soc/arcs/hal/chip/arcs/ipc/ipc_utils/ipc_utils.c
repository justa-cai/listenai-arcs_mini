/**
 ****************************************************************************************
 *
 * @file ipc_slave.c
 *
 * @brief IPC module.
 *
 * Copyright (C) ListenAI 2023
 *
 ****************************************************************************************
 */

#include <string.h>
#include <stdbool.h>
#include "rtos_al.h"
#include "ls_rtos.h"
#include "platform.h"
#include "ipc.h"
#ifdef CFG_AMP_IPC_HALT_PEER_CORE
#include "ic_lock.h"
#endif

#define IPC_HALT_PEER_CORE_TIMEOUT         80000000   //2s

#if (CFG_AMP_IPC_MASTER == 1)
#define IPC_PEER_FAST_CHAN                 IPC_CHAN_SLAVE_FAST
#else
#define IPC_PEER_FAST_CHAN                 IPC_CHAN_MASTER_FAST
#endif


extern struct ipc_shared_env_tag ipc_shared_env;

#ifdef CFG_AMP_IPC_HALT_BY_PEER_CORE
static rtos_semaphore halt_by_peer_signal;
#endif
#ifdef CFG_AMP_IPC_HALT_PEER_CORE
static IC_Mutex halt_peer_mutex;
#endif

__attribute__((section(CONFIG_ARCS_HAL_IPC_UTILS_IPC_FUNC_SECTION))) void ipc_send_notify(uint32_t event)
{
	ipc_fast_notify(IPC_PEER_FAST_CHAN, event);
}

__attribute__((section(CONFIG_ARCS_HAL_IPC_UTILS_IPC_FUNC_SECTION))) void ipc_set_app_status(uint32_t bit_mask)
{
	ipc_shared_env.ipc_app_status |= bit_mask;
}

__attribute__((section(CONFIG_ARCS_HAL_IPC_UTILS_IPC_FUNC_SECTION))) uint32_t ipc_get_app_status(uint32_t bit_mask)
{
    return (ipc_shared_env.ipc_app_status & bit_mask);
}

__attribute__((section(CONFIG_ARCS_HAL_IPC_UTILS_IPC_FUNC_SECTION))) void ipc_clear_app_status(uint32_t bit_mask)
{
	ipc_shared_env.ipc_app_status &= ~bit_mask;
}

#ifdef CFG_AMP_IPC_HALT_BY_PEER_CORE
void ipc_halt_by_peer(bool isr)
{
    if (halt_by_peer_signal)
        rtos_semaphore_signal(halt_by_peer_signal, isr);
}

__attribute__((weak)) void ipc_utils_before_halt_by_peer_core(void)
{
}

__attribute__((weak)) void ipc_utils_after_resume_by_peer_core(void)
{
}

__attribute__((section(CONFIG_ARCS_HAL_IPC_UTILS_IPC_FUNC_SECTION))) static RTOS_TASK_FCT(ipc_halt_by_peer_task)
{
    while (1)
    {
        rtos_semaphore_wait(halt_by_peer_signal, -1);

        ipc_utils_before_halt_by_peer_core();

        GLOBAL_INT_DISABLE();

        // printf("halt by peer core\n");

        ipc_set_app_status(IPC_APP_STATUS_HALT_PEER_BITS_ACK);
        while(1)
        {
            if (ipc_get_app_status(IPC_APP_STATUS_HALT_PEER_BITS_RESUME))
            {
                break;
            }
        }

        GLOBAL_INT_RESTORE();
        // printf("resumed by peer core\n");
        ipc_utils_after_resume_by_peer_core();
    }
}
#endif

#ifdef CFG_AMP_IPC_HALT_PEER_CORE
__attribute__((section(CONFIG_ARCS_HAL_IPC_UTILS_IPC_FUNC_SECTION))) int32_t ipc_halt_peer_core(void)
{
    uint32_t i = 0;
    int32_t ret = 0;

    if (IC_Mutex_acquire(&halt_peer_mutex) == IC_MUTEX_OK)
    {
        ipc_clear_app_status(IPC_APP_STATUS_HALT_PEER_BITS_ALL);
        ipc_send_notify(IPC_EVT_HALT);
        while(i++ < IPC_HALT_PEER_CORE_TIMEOUT)
        {
            if (ipc_get_app_status(IPC_APP_STATUS_HALT_PEER_BITS_ACK))
            {
                break;
            }
        }

        IC_Mutex_release(&halt_peer_mutex);
        if (i >= IPC_HALT_PEER_CORE_TIMEOUT)
        {
            ret = -1;
            CLOGE("Failed to halt the peer core");
        }
    }
    else
    {
        CLOGE("Failed to acquire mutex");
    }
    return ret;
}

__attribute__((section(CONFIG_ARCS_HAL_IPC_UTILS_IPC_FUNC_SECTION))) int32_t ipc_resume_peer_core(void)
{
    ipc_set_app_status(IPC_APP_STATUS_HALT_PEER_BITS_RESUME);

    return 0;
}
#else
__attribute__((section(CONFIG_ARCS_HAL_IPC_UTILS_IPC_FUNC_SECTION))) int32_t ipc_halt_peer_core(void)
{
    return 0;
}

__attribute__((section(CONFIG_ARCS_HAL_IPC_UTILS_IPC_FUNC_SECTION))) int32_t ipc_resume_peer_core(void)
{
    return 0;
}
#endif

#if defined(CFG_AMP_IPC_HALT_PEER_CORE) || defined(CFG_AMP_IPC_HALT_BY_PEER_CORE)
int32_t ipc_halt_peer_init(void)
{
#ifdef CFG_AMP_IPC_HALT_BY_PEER_CORE
    rtos_semaphore_create(&halt_by_peer_signal, 1, 0);

    rtos_task_create(ipc_halt_by_peer_task, "halt_core", HALT_CORE_TASK, LS_HALT_PEER_TASK_STACK_SIZE,
                     NULL, LS_HALT_PEER_TASK_PRIORITY, NULL);
#endif
#ifdef CFG_AMP_IPC_HALT_PEER_CORE
    IC_Mutex_init(&halt_peer_mutex, IC_MUTEX_SLEEP_WAIT, IC_MUTEX_TYPE_IPC);
#endif
    return 0;
}
#endif