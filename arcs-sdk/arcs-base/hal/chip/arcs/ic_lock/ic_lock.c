/****************************************************************************************
 *
 * @file ic_lock.c
 *
 * @brief
 *
 * Copyright (C) ListenAI 2025
 *
 *
 *
 ****************************************************************************************
 */
#include "log_print.h"
#include "ic_spinlock.h"
#include "ic_mutex_internal.h"
#include "ic_mutex.h"
#include "ic_platform.h"

struct ic_shared_lock
{
   volatile ic_spin_lock_t spin_lock[IC_SPIN_LOCK_TYPE_MAX];
   volatile ic_mutex_wait_queue_t mutex_wait_queue[IC_MUTEX_TYPE_MAX];
};

static struct ic_shared_lock ic_lock __attribute__ ((section(".ic_lock_shared_mem")));


volatile ic_spin_lock_t *spin_lock_pool = ic_lock.spin_lock;
volatile ic_mutex_wait_queue_t *ic_mutex_pool = ic_lock.mutex_wait_queue;

static int32_t ic_lock_init_flag = 0;

int32_t ic_lock_init(void)
{
#if (BOOT_HARTID == 0)
    int32_t i, j;
#endif

    if (ic_lock_init_flag)
        return 0;
    ic_lock_init_flag = 1;

    if ((IC_MUTEX_MAX_WAITERS & (IC_MUTEX_MAX_WAITERS - 1)) != 0) {
        CLOGE("IC_mutex_init: Internal Error!. \
              Expecting IC_MUTEX_MAX_WAITERS %d to be a power of 2\n",
            IC_MUTEX_MAX_WAITERS);
        return -1;
    }

#if (BOOT_HARTID == 0)
    for (i = 0; i < IC_SPIN_LOCK_TYPE_MAX; i++) {
        spin_lock_pool[i].state = 0;
    }
    for (i = 0; i < IC_MUTEX_TYPE_MAX; i++) {
        ic_mutex_pool[i]._owner = IC_MUTEX_NO_OWNER;
        ic_mutex_pool[i]._wq_head = 0;
        ic_mutex_pool[i]._wq_tail = 0;

        for (j = 0; j < IC_MUTEX_MAX_WAITERS; j++) {
            ic_mutex_pool[i]._wait_queue[j] = IC_MUTEX_NO_OWNER;
        }
    }
#endif
    IC_Mutex_linkInterrupt();

    return 0;
}
