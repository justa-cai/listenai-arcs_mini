/**
 * @file spinlock.h
 * @brief Spin Lock Implementation for Microcontroller Systems
 * @details This header provides a lightweight spin lock mechanism using atomic memory operations.
 *          The implementation uses an Atomic-Memory-Operation (AMO) based approach with exponential backoff.
 *          (Compatible with architectures supporting __AMOSWAP_W intrinsic)
 */
#ifndef __SPIN_LOCK_H_
#define __SPIN_LOCK_H_

/**
 * @struct spinlock
 * @brief Spin Lock Data Structure
 * @var state: Lock state indicator (0 = unlocked, non-zero = locked)
 *          Uses a single 32-bit word for minimal footprint and efficient AMO operations.
 */
typedef struct {
    uint32_t state;  //!< Lock state storage location
} spinlock;

/**
 * @fn spinlock_init
 * @brief Initialize a spin lock to unlocked state
 * @param[in,out] lock Pointer to spinlock structure to initialize
 * @note Must be called before first use of the lock
 */
__STATIC_FORCEINLINE void spinlock_init(spinlock *lock)
{
    lock->state = 0;  // Set initial state to unlocked
}

/**
 * @fn spinlock_lock
 * @brief Acquire the spin lock exclusively
 * @param[in,out] lock Pointer to spinlock structure
 * @details Implementation features:
 *          - Uses atomic compare-and-swap operation via __AMOSWAP_W
 *          - Employs exponential backoff strategy during contention
 *          - Starts with 10 NOP cycles as initial backoff
 *          - Doubles backoff time after each failed acquisition attempt
 *          - Guaranteed eventual success due to busy-wait loop
 * @note Blocking call - will spin until lock is acquired
 */
__STATIC_FORCEINLINE void spinlock_lock(spinlock *lock)
{
   uint32_t old;
   uint32_t backoff = 10;  // Initial backoff delay in NOP cycles
   do {
       // Atomic test-and-set using AMO swap instruction
       old = __AMOSWAP_W((&(lock->state)), 1);
       if (old == 0) {
           break;  // Successfully acquired lock
       }
       // Exponential backoff with active waiting
       for (volatile int i = 0; i < backoff; i ++) {
           __NOP();  // Prevent compiler optimization while waiting
       }
       backoff += 10;  // Increase backoff for next iteration
   } while (1);
}

/**
 * @fn spinlock_unlock
 * @brief Release ownership of the spin lock
 * @param[in,out] lock Pointer to spinlock structure
 * @details Simply resets the lock state to unlocked (0).
 *          Must only be called by the thread that currently holds the lock.
 */
__STATIC_FORCEINLINE void spinlock_unlock(spinlock *lock)
{
    lock->state = 0;  // Release lock by setting state to unlocked
}

#endif /* __SPIN_LOCK_H_ */
