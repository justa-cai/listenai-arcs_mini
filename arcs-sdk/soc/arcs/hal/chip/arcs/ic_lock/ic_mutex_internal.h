/*
 * Inter-cores synchronization primitives for multiprocessor system.
 * Copyright 2024 ListenAI
 */
#ifndef __IC_MUTEX_INTERNAL_H__
#define __IC_MUTEX_INTERNAL_H__

#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "ic_mutex.h"
#include "ic_spinlock.h"
#include "ic_platform.h"


#define IC_MUTEX_NO_OWNER    0
#define IC_MUTEX_MAX_WAITERS 4  // must be a power of 2


/*! Mutex object type. The minimum size of an object of this type is 
 * \ref IC_MUTEX_SHARED_STRUCT_SIZE. For a cached subsystem, the size is rounded 
 * and aligned to the maximum dcache line size across all cores in the 
 * subsystem. */
// wait queue at shared memory
typedef struct {
    uint32_t          _wq_head;
    uint32_t          _wq_tail;
    uint32_t          _owner;
    uint32_t          _wait_queue[IC_MUTEX_MAX_WAITERS];
} ic_mutex_wait_queue_t;

struct _ic_mutex {
    ic_spin_lock_type _lock;
    ic_mutex_wait_kind  _wait_kind;

    // 本地 rtos mutex
    SemaphoreHandle_t _localMutex;
    uint32_t channel;

    // wait queue at shared memory
    volatile ic_mutex_wait_queue_t* _sharedWaitQ;
};


__attribute__((unused)) static inline uint32_t
IC_get_proc_id()
{
    return IC_get_my_pid();
}

/* Returns a unique id consisting of the proc and thread id.  
 * Unique id == 0 is unused. Only the higher 30-bits of the underlying OS's
 * thread id is used.  
 * FreeRTOS thread id's lower 2 bits is always b'00.
 *
 * Returns 32b unique id.
 */
__attribute__((unused)) static inline uint32_t
IC_uniq_id(uint32_t pid, uint32_t tid)
{
  #ifdef IC_USE_BAREMETAL
    return pid+1;
  #else
    /* Store proc id + 1 within lower 2-bits */
    uint32_t proc_id = (pid+1) & 0x3;
    /* Upper 30-bits are for the thread id */
    uint32_t thread_id = tid;
    return thread_id | proc_id;
#endif
}

/* Returns a unique id consisting of current proc and current thread id.
 *
 * Returns 32b unique id.
 */
__attribute__((unused)) static inline uint32_t
IC_get_uniq_id()
{
    return IC_uniq_id(IC_get_proc_id(), IC_get_my_thread_id());
}

/* Returns the proc id from the unique id. The proc id is stored in the lower
 * 2-bits of the unique id.
 *
 * uniq_id : unique id
 *
 * Returns the proc id
 */
__attribute__((unused)) static inline uint32_t
IC_get_uniq_id_proc_id(uint32_t uniq_id)
{
#ifdef IC_USE_BAREMETAL
    return uniq_id - 1;
#else
    return (uniq_id & 0x3) - 1;
#endif
}

/* Returns the thread id from the unique id. The thread id is stored in the
 * higher 30-bits of the unique id.  
 * Note, FreeRTOS thread id's lower 2 bits is always b'00.
 *
 * uniq_id : unique id
 *
 * Returns the thread id
 */
__attribute__((unused)) static inline uint32_t
IC_get_uniq_id_thread_id(uint32_t uniq_id)
{
    return uniq_id & 0xfffffffc;
}

__attribute__((unused)) static inline void
IC_delay(int delay_count)
{
    int i;
    for (i = 0; i < delay_count; i++) {
        __ASM volatile("nop");
    }
}

#endif /* __IC_MUTEX_INTERNAL_H__ */
