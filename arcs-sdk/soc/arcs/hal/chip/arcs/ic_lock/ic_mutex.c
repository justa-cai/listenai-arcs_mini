/****************************************************************************************
 *
 * @file ic_mutex.c
 *
 * @brief Inter-cores synchronization primitives for multiprocessor system.
 *
 * Copyright (C) ListenAI 2025
 *
 *
 *
 ****************************************************************************************
 */

#include "ic_spinlock.h"
#include "ic_mutex_internal.h"
#include "ic_mutex.h"
#include "ic_platform.h"


#define IC_LOG(fmt, ...)

extern volatile ic_mutex_wait_queue_t *ic_mutex_pool;

ic_mutex_status_t IC_Mutex_init(IC_Mutex *mutex, ic_mutex_wait_kind wait_kind, ic_mutex_type mutex_type)
{
    if (mutex == NULL) {
        IC_LOG("IC_mutex_init: mutex is NULL\n");
        return IC_MUTEX_ERR_INVALID_ARG;
    }

    if (mutex_type >= IC_MUTEX_TYPE_MAX) {
        IC_LOG("IC_mutex_init: mutex ID illegal\n");
        return IC_MUTEX_ERR_INVALID_ARG;
    }

  // 本 processor 多 task 互斥: mutex_init(mutex->_localMutex)
    mutex->_localMutex = xSemaphoreCreateMutex();
    if( mutex->_localMutex == NULL ) {
        return IC_MUTEX_ERR_INTERNAL;
    }

    ic_spin_lock_init(IC_SPIN_LOCK_TYPE_MUTEX);
    mutex->channel = IC_MUTEX_CHANNEL(mutex_type);
    mutex->_lock   = IC_SPIN_LOCK_TYPE_MUTEX;
    mutex->_wait_kind   = wait_kind;
    mutex->_sharedWaitQ = &ic_mutex_pool[mutex_type];

    IC_LOG("[ic_mutex] Initialized mutex %d\n", mutex_type);

    return IC_MUTEX_OK;
}

ic_mutex_status_t IC_Mutex_acquire(IC_Mutex *mutex)
{
    uint32_t id = IC_get_uniq_id();

    volatile ic_mutex_wait_queue_t *mutexWQ = mutex->_sharedWaitQ;

    IC_LOG("Attempting to acquire mutex @ %p\n", mutex);
  
    // 本 processor 多 task 互斥
    xSemaphoreTake(mutex->_localMutex, portMAX_DELAY);

    /* Acquire a spin-lock on the mutex to prevent others from updating the mutex
    * state */
    ic_spin_lock_irqsave(mutex->_lock);

    if (((mutexWQ->_wq_tail + 1) & (IC_MUTEX_MAX_WAITERS - 1)) == mutexWQ->_wq_head) {
        ic_spin_unlock_irqsave(mutex->_lock);
        xSemaphoreGive(mutex->_localMutex);
        return IC_MUTEX_ERR_MAX_WAITERS;
    }

    if (mutexWQ->_owner == IC_MUTEX_NO_OWNER) {
        mutexWQ->_owner = id;
        /* Release the mutex spin-lock */
        ic_spin_unlock_irqsave(mutex->_lock);
        IC_LOG("Acquired mutex @ %p\n", mutex);
        return IC_MUTEX_OK;
    } else {
        uint32_t wq_tail = mutexWQ->_wq_tail;
        mutexWQ->_wait_queue[wq_tail] = id;
        mutexWQ->_wq_tail = (wq_tail + 1) & (IC_MUTEX_MAX_WAITERS-1);
    }

    /* Release the mutex spin-lock */
    ic_spin_unlock_irqsave(mutex->_lock);

    if (mutex->_wait_kind == IC_MUTEX_SLEEP_WAIT) {
        while (1) {
            IC_disable_interrupts(mutex->channel);

            // clear task notification's value and pending status
            ulTaskNotifyTake(pdTRUE, 0 ); /* timeout */
            if (mutexWQ->_owner != id) {
                IC_LOG("Sleep waiting on mutex @ %p\n", mutex);
                IC_Mutex_set_task_handle(mutex->channel, (TaskHandle_t) IC_get_my_thread_id());
                IC_enable_interrupts(mutex->channel);
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            } else { // (&mutex->_owner) == id
                // 唤醒 task_handle = 0
                IC_Mutex_set_task_handle(mutex->channel, NULL);
                IC_enable_interrupts(mutex->channel);
                break; // out of while(1)
            }
        }
    } else { // busy wait
        do {
        int32_t d = mutexWQ->_wq_tail - mutexWQ->_wq_head;
        if (d < 0) {
            d = d + IC_MUTEX_MAX_WAITERS;
        }

        IC_delay(32*d);

        } while (mutexWQ->_owner != id);
    }

    IC_LOG("Acquired mutex @ %p\n", mutex);

    return IC_MUTEX_OK;
}

/* Release the mutex. Notifies the single task on a proc waiting for the mutex
 *
 * mutex: mutex to be released.
 *
 * Returns IC_MUTEX_OK, if successful, else returns IC_ERROR_MUTEX_NOT_OWNER if
 * attempting to release the mutex that was not acquire by this proc.
 */
ic_mutex_status_t IC_Mutex_release(IC_Mutex *mutex)
{
    uint32_t wq_head;
    uint32_t wake_pid;
    volatile ic_mutex_wait_queue_t * mutexWQ = mutex->_sharedWaitQ;

    // 不做检查, cache line 操作开销, 不合理, 约定即可.
    // if (IC_load(&mutexWQ->_owner) != IC_get_uniq_id())
    //   return IC_ERROR_MUTEX_NOT_OWNER;

    /* Acquire a spin-lock on the mutex to prevent others from updating the mutex
    * state */
    ic_spin_lock_irqsave(mutex->_lock);

    mutexWQ->_owner = IC_MUTEX_NO_OWNER;
    wq_head  = mutexWQ->_wq_head;
    wake_pid = 0xffffffff;
    if (mutexWQ->_wait_queue[wq_head] != IC_MUTEX_NO_OWNER) {
        uint32_t owner = mutexWQ->_wait_queue[wq_head];
        mutexWQ->_wait_queue[wq_head] = IC_MUTEX_NO_OWNER;
        mutexWQ->_wq_head = (wq_head+1) & (IC_MUTEX_MAX_WAITERS-1);
        mutexWQ->_owner = owner;
        wake_pid = IC_get_uniq_id_proc_id(owner);
    }

    /* Release the mutex spin-lock */
    ic_spin_unlock_irqsave(mutex->_lock);

    // 本 processor 多 task 互斥
    xSemaphoreGive(mutex->_localMutex);

    IC_LOG("Releasing mutex @ %p\n", mutex);

    if (wake_pid != 0xffffffff && mutex->_wait_kind == IC_MUTEX_SLEEP_WAIT) {
        if (wake_pid != IC_get_proc_id()) {
            IC_proc_notify(mutex->channel);
        }
    }

    return IC_MUTEX_OK;
}

/* Attempts to acquire the mutex
 *
 * mutex : mutex to be acquire
 *
 * Returns IC_MUTEX_OK if successful, else returns IC_ERROR_MUTEX_ACQUIRED if
 * mutex is alread acqired.
 */
ic_mutex_status_t IC_Mutex_try_acquire(IC_Mutex *mutex)
{
    uint32_t id = IC_get_uniq_id();
    volatile ic_mutex_wait_queue_t *mutexWQ;
    BaseType_t ret;

    IC_LOG("Attempting to acquire mutex @ %p\n", mutex);

    // 本 processor 多 task 互斥

    // try to acquire local mutex
    ret = xSemaphoreTake(mutex->_localMutex, 0);
    if (ret != pdPASS) {
        // 本地 proc 已有其他 owner
        return IC_MUTEX_ERR_ACQUIRED;
    }

    mutexWQ = mutex->_sharedWaitQ;

    /* Check if mutex is already owned. If yes, abort */
    if (mutexWQ->_owner != IC_MUTEX_NO_OWNER) {
        // 无效 acquire, 释放本地互斥
        xSemaphoreGive(mutex->_localMutex);

        return IC_MUTEX_ERR_ACQUIRED;
    }

    /* Acquire a spin-lock on the mutex to prevent others from updating
    * the mutex state */
    ic_spin_lock_irqsave(mutex->_lock);

    if (mutexWQ->_owner == IC_MUTEX_NO_OWNER) {
        mutexWQ->_owner = id;
    }

    /* Release the mutex spin-lock */
    ic_spin_unlock_irqsave(mutex->_lock);

    if (mutexWQ->_owner != id) {
        // 无效 acquire, 释放本地互斥
        xSemaphoreGive(mutex->_localMutex);

        IC_LOG("Failed to acquire mutex @ %p\n", mutex);
        return IC_MUTEX_ERR_ACQUIRED;
    }

    IC_LOG("Acquired mutex @ %p\n", mutex);

    return IC_MUTEX_OK;
}
