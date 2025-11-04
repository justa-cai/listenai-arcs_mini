/**
 ****************************************************************************************
 *
 * Copyright (C) ListenAI 2023
 *
 *
 ****************************************************************************************
*/
#ifndef _IPC_PLATFORM_H_
#define _IPC_PLATFORM_H_

#include <rtos_def.h>
#include "cache.h"

#define platform_wmb()
#define platform_rmb()

#define IPC_CHAN_INIT(lock)                 rtos_mutex_create(&(lock));
#define IPC_CHAN_DEINIT(lock)               rtos_mutex_delete(lock);
#define IPC_CHAN_LOCK(lock)                 rtos_mutex_lock(lock)
#define IPC_CHAN_UNLOCK(lock)               rtos_mutex_unlock(lock)
#define IPC_PLATFORM_IRQ_OFFSET            16
#define IPC_PLATFORM_IRQ_STATUS(status)    ((status) >> IPC_PLATFORM_IRQ_OFFSET)

typedef rtos_mutex  ipc_lock_t;

#ifdef CFG_AMP_IPC_MASTER

#define IPC_FUNC_ATTR                       __attribute__ ((section (".ramcode")))
#define IPC_FUNC_ALIGN                      __attribute__((aligned(32)))

#else

#define IPC_FUNC_ATTR                       __attribute__((aligned(32)))
#define IPC_FUNC_ALIGN                      __attribute__((aligned(32)))

#endif

void ipc_platform_irq_disable(int32_t chan);
int32_t ipc_platform_wait_event(void **arg, uint32_t timeout);
void ipc_platform_init(int8_t (*handle)(uint32_t status, uint32_t param));
int32_t ipc_platform_task_notify(void *ccb, void *arg);
void ipc_platform_notify(uint32_t core_id, uint32_t irq);


#endif
