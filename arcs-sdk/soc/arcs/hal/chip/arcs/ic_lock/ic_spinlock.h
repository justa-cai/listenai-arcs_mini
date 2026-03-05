/****************************************************************************************
 *
 * @file spin_lock.h
 *
 * @brief
 *
 * Copyright (C) ListenAI 2025
 *
 *
 *
 ****************************************************************************************
 */
#ifndef __SPIN_LOCK_H__
#define __SPIN_LOCK_H__

#ifdef CFG_RTOS
#include "FreeRTOS.h"
#endif
#include "spinlock.h"


typedef enum {
    IC_SPIN_LOCK_TYPE_MUTEX,
    IC_SPIN_LOCK_TYPE_SLEEP,
    IC_SPIN_LOCK_TYPE_VRTC,
    IC_SPIN_LOCK_TYPE_IPC,
    IC_SPIN_LOCK_TYPE_MAX
} ic_spin_lock_type;

typedef spinlock ic_spin_lock_t;

extern volatile ic_spin_lock_t *spin_lock_pool;

__STATIC_FORCEINLINE void ic_spin_lock_init(ic_spin_lock_type lock)
{
    return;
}

__STATIC_FORCEINLINE void ic_spin_lock(ic_spin_lock_type lock)
{
    spinlock_lock((ic_spin_lock_t*)&spin_lock_pool[lock]);
}

__STATIC_FORCEINLINE void ic_spin_unlock(ic_spin_lock_type lock)
{
    spinlock_unlock((ic_spin_lock_t*)&spin_lock_pool[lock]);
}

#ifdef CFG_RTOS
__STATIC_FORCEINLINE void ic_spin_lock_irqsave(ic_spin_lock_type lock)
{
    vPortEnterCritical();
    spinlock_lock((ic_spin_lock_t*)&spin_lock_pool[lock]);
}

__STATIC_FORCEINLINE void ic_spin_unlock_irqsave(ic_spin_lock_type lock)
{
    spinlock_unlock((ic_spin_lock_t*)&spin_lock_pool[lock]);
    vPortExitCritical();
}
#endif

__STATIC_FORCEINLINE void ic_spin_lock_irq(ic_spin_lock_type lock)
{
    __disable_irq();
    spinlock_lock((ic_spin_lock_t*)&spin_lock_pool[lock]);
}

__STATIC_FORCEINLINE void ic_spin_unlock_irq(ic_spin_lock_type lock)
{
    spinlock_unlock((ic_spin_lock_t*)&spin_lock_pool[lock]);
    __enable_irq();
}

#endif /* __SPINLOCK_H__ */
