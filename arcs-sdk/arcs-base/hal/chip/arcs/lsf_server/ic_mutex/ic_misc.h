/*
 * Inter-cores synchronization primitives for multiprocessor system.
 * Copyright 2024 ListenAI
 */
#ifndef __IC_MISC_H__
#define __IC_MISC_H__

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#include "arcs_ap.h"
#include "nmsis_core.h"

#include "cache.h"

//#include "ic_mutex_common.h"

#ifdef IC_USE_RTOS
//#include "ic_rtos.h"
#elif IC_USE_BAREMETAL
#include "ic_baremetal.h"
#endif

//#include "ic_sys.h"

// module scope error-no
extern int IC_Mutex_errorNo;

// 内部保证 size cache line 对齐
/*! Write back modified cache lines where data is present to main memory  */
static inline
void IC_HAL_dcache_region_writeback(void *addr, uint32_t size)
{
  HAL_FlushDCache_by_Addr((uint32_t *)addr, size);

  /* ~=dsb, to make sure previous ilm/dlm/icache/dcache control done */
  __FENCE(iorw, iorw);
}

static inline
void IC_HAL_dcache_line_writeback(void *addr)
{
  HAL_FlushDCache_by_Addr((uint32_t *)addr, IC_HAL_DCACHE_SIZE);

  /* ~=dsb, to make sure previous ilm/dlm/icache/dcache control done */
  __FENCE(iorw, iorw);
}

// 内部保证 size cache line 对齐
/*! Invalidate cache lines where data is present to main memory */
static inline
void IC_HAL_dcache_region_invalidate(void *addr, uint32_t size)
{
  HAL_InvalidateDCache_by_Addr((uint32_t *)addr, size);

  /* ~=dsb, to make sure previous ilm/dlm/icache/dcache control done */
  __FENCE(iorw, iorw);
}

static inline
void IC_HAL_dcache_line_invalidate(void *addr)
{
  HAL_InvalidateDCache_by_Addr((uint32_t *)addr, IC_HAL_DCACHE_SIZE);

  /* ~=dsb, to make sure previous ilm/dlm/icache/dcache control done */
  __FENCE(iorw, iorw);
}

// 内部保证 size cache line 对齐
/*! Write back and invalidate cache lines where data is present to
  * main memory  */
static inline
void IC_HAL_dcache_region_writeback_inv(void *addr, uint32_t size)
{
  HAL_FlushInvalidateDCache_by_Addr((uint32_t *)addr, size);

  /* ~=dsb, to make sure previous ilm/dlm/icache/dcache control done */
  __FENCE(iorw, iorw);
}

static inline
void IC_HAL_dcache_line_writeback_inv(void *addr)
{
  HAL_FlushInvalidateDCache_by_Addr((uint32_t *)addr, IC_HAL_DCACHE_SIZE);

  /* ~=dsb, to make sure previous ilm/dlm/icache/dcache control done */
  __FENCE(iorw, iorw);
}

#include "log_print.h"
#ifdef IC_DEBUG
#define IC_LOG(...)     CLOGD(__VA_ARGS__)
#else
#define IC_LOG(...)
#endif

__attribute__((unused)) static inline uint32_t
IC_load(volatile uint32_t *address)
{
#if IC_HAL_DCACHE_SIZE>0 && !IC_HAL_DCACHE_IS_COHERENT
  IC_HAL_dcache_line_invalidate((void *)address);
  return *address;
#else
  uint32_t val;
  // memory barrier: read acquire: part 1
  __FENCE(rw,rw);

  val = *address;

  // memory barrier: read acquire: part 2
  __FENCE(r,rw);

  return val;
#endif
}

__attribute__((unused)) static inline void
IC_store(uint32_t value, volatile uint32_t *address)
{
  *address = value;
#if IC_HAL_DCACHE_SIZE>0 && !IC_HAL_DCACHE_IS_COHERENT
  IC_HAL_dcache_line_writeback((void *)address);
#endif
}

__attribute__((unused)) static inline void
IC_delay(int delay_count)
{
  int i;
  for (i = 0; i < delay_count; i++) {
    __ASM volatile("nop");
  }
}

/* If successful, returns 1 else returns 0 */
__attribute__((unused)) static inline int32_t
IC_atomic_int_conditional_set_bool(volatile int32_t *addr,
                                     int32_t from,
                                     int32_t to)
{
  // no need to memory barrier inside
  // "volatile" assures the order of read/write

  // first read to check current owner
  int32_t val = *addr;
  __FENCE(iorw,iorw);

  if (val == from) { // val == from, free
    // try to write non-zero id
    *addr = to;
    __FENCE(iorw,iorw);

    if (to == 0) {
      // write success
      return 1;
    }
    // else: to != 0
    // check result of write
    val = *addr;
    __FENCE(iorw,iorw);

    if (val == to) {
      // write success
      return 1;
    }
    else {
      // write failure, preempted
      // IC_Mutex_errorNo = IC_MUTEX_ERROR_PREEMPTED;
      // IC_Mutex_errorNo ++;
      // while(1);

      return 0;
    }
  }

  // else: val != from, not free
  if (val == to) {
    // repeated acquire
    // IC_Mutex_errorNo = IC_MUTEX_ERROR_REACQUIRE;
    IC_Mutex_errorNo ++;
    while(1);
  }

  // not free, can't update, return false
  return 0;
}

__attribute__((unused)) static inline void
IC_spin_lock_acquire(volatile uint32_t *lock)
{
  IC_disable_preemption();
  uint32_t pid = IC_get_proc_id();

  // memory barrier: read acquire: part 1
  __FENCE(iorw,iorw);

  while (IC_atomic_int_conditional_set_bool((volatile int32_t *)lock,
                                              0, pid + 1) == 0)
    IC_delay(16);

  // memory barrier: read acquire: part 2
  __FENCE(iorw,iorw);
}

__attribute__((unused)) static inline void
IC_spin_lock_release(volatile uint32_t *lock)
{
  uint32_t pid = IC_get_proc_id();

  // memory barrier: write release
  __FENCE(iorw,iorw);

  while (IC_atomic_int_conditional_set_bool((volatile int32_t *)lock,
                                              pid + 1, 0) == 0)
    IC_delay(16);

  __FENCE(iorw,iorw);

  IC_enable_preemption();
}

#ifdef IC_PROFILE
#define IC_PROFILE_EVENT(ic_profile_event_type, ic_obj_id) \
        IC_profile_event(ic_profile_event_type, ic_obj_id)
#else
#define IC_PROFILE_EVENT(ic_profile_event_type, ic_obj_id)
#endif

#endif /* __IC_MISC_H__ */
