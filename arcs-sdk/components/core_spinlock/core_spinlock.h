#ifndef __CORE_SPINLOCK_H__
#define __CORE_SPINLOCK_H__
#include <stdint.h>

#define SPINLOCK_DEFINE(x)  volatile uint32_t x __attribute((section(".ipc.spinlock")))

void spinlock_init(volatile uint32_t *lock);

int spinlock_acquire(volatile uint32_t *lock);

int spinlock_release(volatile uint32_t *lock);

#endif
