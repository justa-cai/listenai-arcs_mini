/**
 ****************************************************************************************
 *
 * @file ipc_utils.c
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
#include "log_print.h"
#include "ipc.h"
#include "ipc_utils.h"
#ifdef CFG_AMP_IPC_HALT_PEER_CORE
#include "ic_lock.h"
#endif
#include "amp_shared.h"
#include "vrtc.h"
#ifdef CFG_AMP_IPC
#include "ipc_msg.h"
#endif
#ifdef CFG_AMP_IPC_INDICATION
#include "ipc_core.h"
#endif

#define IPC_HALT_PEER_CORE_TIMEOUT         10000000   //10s

#ifdef CFG_AMP_IPC_HALT_BY_PEER_CORE
static rtos_semaphore halt_by_peer_signal;
#endif
#ifdef CFG_AMP_IPC_HALT_PEER_CORE
static IC_Mutex halt_peer_mutex;
static rtos_task_handle halt_peer_owner;
static uint32_t halt_peer_nest_count;
static bool halt_peer_initialized;
#endif

#ifdef CFG_AMP_IPC
void ipc_set_app_status(uint32_t bit_mask)
{
    amp_app_status_set(bit_mask);
}

uint32_t ipc_get_app_status(uint32_t bit_mask)
{
    return amp_app_status_get(bit_mask);
}

void ipc_clear_app_status(uint32_t bit_mask)
{
    amp_app_status_clear(bit_mask);
}
#endif

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

_EXT_RAM static RTOS_TASK_FCT(ipc_halt_by_peer_task)
{
    while (1)
    {
        rtos_semaphore_wait(halt_by_peer_signal, -1);

        ipc_utils_before_halt_by_peer_core();

        GLOBAL_INT_DISABLE();
        ipc_set_app_status(AMP_APP_STATUS_HALT_PEER_ACK);
        __SMP_RWMB();

        while(1)
        {
            if (ipc_get_app_status(AMP_APP_STATUS_HALT_PEER_RESUME))
            {
                break;
            }
        }

        ipc_set_app_status(AMP_APP_STATUS_HALT_PEER_RESUME_ACK);
        __SMP_RWMB();
        GLOBAL_INT_RESTORE();

        ipc_utils_after_resume_by_peer_core();
    }
}
#endif

#ifdef CFG_AMP_IPC_HALT_PEER_CORE
int32_t ipc_halt_peer_core(void)
{
    uint32_t start;
    int32_t ret = 0;
    rtos_task_handle curr_task = rtos_get_task_handle();

    if (!halt_peer_initialized)
        return -1;

    GLOBAL_INT_DISABLE();
    if ((halt_peer_owner == curr_task) && (halt_peer_nest_count > 0))
    {
        halt_peer_nest_count++;
        GLOBAL_INT_RESTORE();
        return 0;
    }
    GLOBAL_INT_RESTORE();

    if (IC_Mutex_acquire(&halt_peer_mutex) == IC_MUTEX_OK)
    {
        ipc_clear_app_status(AMP_APP_STATUS_HALT_PEER_ALL);
        __SMP_RWMB();
        ipc_send_signal(IPC_SIG_HALT);

        start = (uint32_t)SysTimer_GetLoadValue();
        do
        {
            if (ipc_get_app_status(AMP_APP_STATUS_HALT_PEER_ACK))
                break;
        } while ((uint32_t)((uint32_t)SysTimer_GetLoadValue() - start) < IPC_HALT_PEER_CORE_TIMEOUT);

        if (!ipc_get_app_status(AMP_APP_STATUS_HALT_PEER_ACK))
        {
            ret = -1;
            IC_Mutex_release(&halt_peer_mutex);
            CLOGE("Failed to halt the peer core");
        }
        else
        {
            GLOBAL_INT_DISABLE();
            halt_peer_owner = curr_task;
            halt_peer_nest_count = 1;
            GLOBAL_INT_RESTORE();
        }
    }
    else
    {
        ret = -1;
        CLOGE("Failed to acquire mutex");
    }

    return ret;
}

int32_t ipc_resume_peer_core(void)
{
    int32_t ret = 0;
    uint32_t start;
    rtos_task_handle curr_task = rtos_get_task_handle();

    GLOBAL_INT_DISABLE();
    if ((halt_peer_owner != curr_task) || (halt_peer_nest_count == 0))
    {
        GLOBAL_INT_RESTORE();
        CLOGE("Halt mutex is not owned");
        return -1;
    }

    if (halt_peer_nest_count > 1)
    {
        halt_peer_nest_count--;
        GLOBAL_INT_RESTORE();
        return 0;
    }
    GLOBAL_INT_RESTORE();

    ipc_set_app_status(AMP_APP_STATUS_HALT_PEER_RESUME);
    __SMP_RWMB();

    start = (uint32_t)SysTimer_GetLoadValue();
    do
    {
        if (ipc_get_app_status(AMP_APP_STATUS_HALT_PEER_RESUME_ACK))
            break;
    } while ((uint32_t)((uint32_t)SysTimer_GetLoadValue() - start) < IPC_HALT_PEER_CORE_TIMEOUT);

    if (!ipc_get_app_status(AMP_APP_STATUS_HALT_PEER_RESUME_ACK))
    {
        ret = -1;
        CLOGE("Failed to get RESUME_ACK");
    }
    __SMP_RWMB();
    GLOBAL_INT_DISABLE();
    halt_peer_owner = NULL;
    halt_peer_nest_count = 0;
    GLOBAL_INT_RESTORE();
    IC_Mutex_release(&halt_peer_mutex);

    return ret;
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
    halt_peer_owner = NULL;
    halt_peer_nest_count = 0;
    halt_peer_initialized = true;
#endif
    return 0;
}
#endif

#ifdef CFG_AMP_IPC
volatile struct amp_shared_info* ipc_get_shared_info(void)
{
    return amp_shared_get();
}

int32_t ipc_peer_msg_push(uint32_t ep_idx, int32_t len, void *data)
{
#if defined(CFG_AMP_IPC_MASTER) && defined(CFG_AMP_IPC_SLAVE)
#error "Exactly one IPC role must be selected"
#elif defined(CFG_AMP_IPC_MASTER)
    return ipc_msg_push_to(IPC_CHAN_MASTER_MSG, IPC_CHAN_SLAVE_MSG, ep_idx, len, data);
#elif defined(CFG_AMP_IPC_SLAVE)
    return ipc_msg_push_to(IPC_CHAN_SLAVE_MSG, IPC_CHAN_MASTER_MSG, ep_idx, len, data);
#else
#error "CFG_AMP_IPC_MASTER or CFG_AMP_IPC_SLAVE must be defined"
#endif
}
#endif

#ifdef CFG_AMP_IPC_INDICATION
#define ipc_dbg_output_string(buf, len)   ipc_print("%s", buf)

static void ipc_event_handler(struct cfg_ind_event *event)
{
    ls_event_post(event->module_id, event->event_id, event->event_data,
                  (size_t)event->event_data_size, LS_NEVER_TIMEOUT, false);
}

static bool ipc_ind_msg_valid(struct ipc_msg_desc *desc)
{
    return ((desc != NULL) && (desc->data != NULL) &&
            (desc->hdr.data_len >= sizeof(struct ipc_msg_hdr)));
}

static void ipc_print_handler(struct ipc_msg_hdr *msg)
{
    uint32_t len = msg->len;
    const char *buf = (const char *)msg->data;

    if (len >= IPC_MASTER_MSG_BUF_SIZE - sizeof(struct ipc_msg_hdr))
        len = IPC_MASTER_MSG_BUF_SIZE - sizeof(struct ipc_msg_hdr);

    if (len)
        ipc_dbg_output_string(buf, len);
}

int32_t ipc_indication_handler(struct ipc_msg_desc *desc, void *arg)
{
    struct ipc_msg_hdr *msg;

    (void)arg;

    if (!ipc_ind_msg_valid(desc))
        return IPC_MSG_RELEASE;

    msg = (struct ipc_msg_hdr*)desc->data;

    switch (msg->id)
    {
        case IPC_IND_EVENT:
            ipc_event_handler((struct cfg_ind_event*)msg);
            break;
        case IPC_IND_PRINT:
            ipc_print_handler(msg);
            break;
        default:
            break;
    }

    return IPC_MSG_RELEASE;
}
#endif
