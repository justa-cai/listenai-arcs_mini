/**
 ****************************************************************************************
 *
 * Copyright (C) ListenAI 2023
 *
 ****************************************************************************************
*/

#include <stdint.h>
#include <string.h>
#include "Driver_MBX.h"
#include "ipc_core.h"
#include "rtos_al.h"

#define IPC_MAILBOX_DEV     NULL

#define CSK_MBX_TRRIGER_NOTIFY 0

void ipc_platform_init(int8_t (*handle)(uint32_t event, uint32_t status))
{
    MBX2_Initialize(IPC_MAILBOX_DEV, handle);
}
#if 0
void ipc_platform_irq_enable(int32_t chan)
{
#if defined(CONFIG_HARTID) && (CONFIG_HARTID==0)
    MBX_Control(IPC_MAILBOX_DEV, CSK_MBX_AP_CTRL_ENABLE_IRQ, NULL);
#else
    MBX_Control(IPC_MAILBOX_DEV, CSK_MBX_CP_CTRL_ENABLE_IRQ, NULL);
#endif
}

void ipc_platform_irq_disable(int32_t chan)
{
#if defined(CONFIG_HARTID) && (CONFIG_HARTID==0)
    MBX_Control(IPC_MAILBOX_DEV, CSK_MBX_AP_CTRL_DISABLE_IRQ, NULL);
#else
    MBX_Control(IPC_MAILBOX_DEV, CSK_MBX_CP_CTRL_DISABLE_IRQ, NULL);
#endif
}
#endif
void ipc_platform_irq_trigger(uint32_t core_id, uint32_t irq)
{
    MBX_Trigger(IPC_MAILBOX_DEV, (irq + IPC_PLATFORM_IRQ_OFFSET));
}

void* ipc_platform_get_task_handle(void)
{
    return (void*)(rtos_get_task_handle());
}

int32_t ipc_platform_event_wait(uint32_t timeout)
{
    return rtos_task_wait_notification((int32_t)timeout);
}

int32_t ipc_platform_event_notify(void *ccb, void *arg)
{
    rtos_task_notify((rtos_task_handle)arg, true);

    return 0;
}

