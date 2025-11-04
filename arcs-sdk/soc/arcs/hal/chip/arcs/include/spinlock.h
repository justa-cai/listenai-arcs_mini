#ifndef __SPIN_LOCK_H_
#define __SPIN_LOCK_H_

#include "core_feature_base.h"

typedef struct {
    uint32_t state;
} spinlock;

__STATIC_FORCEINLINE void spinlock_init(spinlock *lock)
{
    lock->state = 0;
}

__STATIC_FORCEINLINE void spinlock_lock(spinlock *lock)
{
   uint32_t old;
   uint32_t backoff = 10;
   do {
       // Use amoswap as spinlock
       old = __AMOSWAP_W((&(lock->state)), 1);
       if (old == 0) {
           break;
       }
       for (volatile int i = 0; i < backoff; i ++) {
           __NOP();
       }
       backoff += 10;
   } while (1);
}

__STATIC_FORCEINLINE void spinlock_unlock(spinlock *lock)
{
    lock->state = 0;
}


  
#endif /* __SPIN_LOCK_H_ */
 
