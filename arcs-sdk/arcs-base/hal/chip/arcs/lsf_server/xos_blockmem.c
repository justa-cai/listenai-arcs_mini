
// xos_blockmem.h - XOS block memory pool manager.

// Copyright (c) 2015-2020 Cadence Design Systems, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "xos_blockmem.h"

#include "ic_common.h"

// log模块
#include "log_print.h"


#define XOS_BLOCKPOOL_SIG   0x706F6F6CU     // Signature of block pool
#define XOS_BLOCKMEM_SIG    0x66726565U     // Signature of free block

//-----------------------------------------------------------------------------
// Type conversion helpers.
//-----------------------------------------------------------------------------

__attribute__((always_inline))
static inline uint32_t *
xos_voidp_to_uint32p(void * arg)
{
    return (uint32_t *) arg; // parasoft-suppress MISRAC2012-RULE_11_5-a-4 "Type conversion checked"
}

__attribute__((always_inline))
static inline void *
xos_uint32p_to_voidp(uint32_t * arg)
{
    return (void *) arg; // parasoft-suppress MISRAC2012-RULE_11_5-a-4 "Type conversion checked"
}

__attribute__((always_inline))
static inline uint32_t
xos_voidp_to_uint32(void * arg)
{
    return (uint32_t) arg; // parasoft-suppress MISRAC2012-RULE_11_6-a-2 "Type conversion checked"
}

__attribute__((always_inline))
static inline void *
xos_uint32_to_voidp(uint32_t arg)
{
    return (void *) arg; // parasoft-suppress MISRAC2012-RULE_11_6-a-2 "Type conversion checked"
}

__attribute__((always_inline))
static inline uint32_t *
xos_uint32_to_uint32p(uint32_t arg)
{
    return (uint32_t *) arg; // parasoft-suppress MISRAC2012-RULE_11_4-a-4 "Type conversion checked"
}

__attribute__((always_inline))
static inline uint32_t
xos_uint32p_to_uint32(uint32_t * arg)
{
    return (uint32_t) arg; // parasoft-suppress MISRAC2012-RULE_11_4-a-4 "Type conversion checked"
}

//-----------------------------------------------------------------------------
//  Initialize the memory pool.
//-----------------------------------------------------------------------------
int32_t
xos_block_pool_init(XosBlockPool * pool,
                    void *         mem,
                    uint32_t       blocksize,
                    uint32_t       nblocks,
                    uint32_t       flags)
{
    uint32_t * ptr;
    int32_t    ret;
    uint32_t   i;

    // Check input parameters.
    if ((pool == NULL) || (mem == NULL) || (blocksize == 0U) || (nblocks == 0U)) {
        return -1;
    }
    if ((blocksize % 4U) != 0U) {
        return -1;
    }
    // Limited by semaphore.
    if (nblocks > (uint32_t) INT32_MAX) {
        return -1;
    }

    // Build the free block list.
    ptr = xos_voidp_to_uint32p(mem);
    for (i = 0; i < (nblocks - 1U); i++) {
        ptr[0] = xos_uint32p_to_uint32(ptr) + blocksize;
        ptr[1] = XOS_BLOCKMEM_SIG;
        ptr = xos_uint32_to_uint32p(ptr[0]);
    }
    ptr[0] = 0;
    ptr[1] = XOS_BLOCKMEM_SIG;

    // Init the pool object.
    pool->head  = xos_voidp_to_uint32p(mem);
    pool->first = xos_voidp_to_uint32p(mem);
    pool->last  = ptr;
    pool->nblks = nblocks;
    pool->flags = flags;
    pool->sig   = XOS_BLOCKPOOL_SIG;

    // Create the pool semaphore with correct properties and count.
    pool->sem = xSemaphoreCreateCounting(nblocks, nblocks);
    if (NULL == pool->sem) {
        return -1;
    }

#if XOS_OPT_STATS
    pool->num_allocs = 0;
    pool->num_frees  = 0;
    pool->num_waits  = 0;
#endif

    return 0;
}


//-----------------------------------------------------------------------------
//  Allocate a single block from the pool, suspend until available.
//  Can't call this from interrupt context.
//-----------------------------------------------------------------------------
void *
xos_block_alloc(XosBlockPool * pool)
{
    uint32_t * ptr;
    BaseType_t ret;

    if ((pool == NULL) || (pool->sig != XOS_BLOCKPOOL_SIG)) {
        return NULL;
    }

    // Decrement the semaphore. We'll block here if no free memory.
#if XOS_OPT_STATS
    if (xos_sem_test(&(pool->sem)) == 0) {
        pool->num_waits++;
    }
#endif
    ret = xSemaphoreTake(pool->sem, portMAX_DELAY);
    if (ret != pdPASS) {
        return NULL;
    }

    // Once we have decremented the semaphore, we are guaranteed to find a
    // free memory block. Lock out interrupts while we update the free list.
    taskENTER_CRITICAL();

    ptr = pool->head;
    IC_ASSERT(ptr != NULL);
    pool->head = xos_uint32_to_uint32p(ptr[0]);
#if XOS_OPT_STATS
    pool->num_allocs++;
#endif

    taskEXIT_CRITICAL();
    return ptr;
}


//-----------------------------------------------------------------------------
//  Nonblocking version of xos_block_alloc(). Returns immediately on failure.
//-----------------------------------------------------------------------------
void *
xos_block_try_alloc(XosBlockPool * pool)
{
    uint32_t * ptr;
    BaseType_t    ret;

    if ((pool == NULL) || (pool->sig != XOS_BLOCKPOOL_SIG)) {
        return NULL;
    }

    // Try to decrement the semaphore.
    ret = xSemaphoreTake(pool->sem, 0);
    if (ret != pdPASS) {
        return NULL;
    }

    // Once we have decremented the semaphore, we are guaranteed to find a
    // free memory block. Lock out interrupts while we update the free list.
    taskENTER_CRITICAL();

    ptr = pool->head;
    IC_ASSERT(ptr != NULL);
    pool->head = xos_uint32_to_uint32p(ptr[0]);
#if XOS_OPT_STATS
    pool->num_allocs++;
#endif

    taskEXIT_CRITICAL();
    return ptr;
}


//-----------------------------------------------------------------------------
//  Free a single block back to the pool.
//-----------------------------------------------------------------------------
int32_t
xos_block_free(XosBlockPool * pool, void * mem)
{
    uint32_t * ptr = xos_voidp_to_uint32p(mem);

    if ((pool == NULL) || (pool->sig != XOS_BLOCKPOOL_SIG) || (mem == NULL)) {
        return -1;
    }

    // Pointer must belong to pool.
    if ((ptr < pool->first) || (ptr > pool->last)) {
        return -1;
    }

    // Lock out interrupts while we update the free list.
    taskENTER_CRITICAL();

    ptr[0] = xos_uint32p_to_uint32(pool->head);
    pool->head = ptr;
    ptr[1] = XOS_BLOCKMEM_SIG;
#if XOS_OPT_STATS
    pool->num_frees++;
#endif

    taskEXIT_CRITICAL();

    // Update the semaphore.
    if (xSemaphoreGive(pool->sem) != pdPASS) {
        return -1;
    }

    return 0;
}


//-----------------------------------------------------------------------------
//  Verify that the state of the pool is consistent.
//-----------------------------------------------------------------------------
int32_t
xos_block_pool_check(const XosBlockPool * pool)
{
    uint32_t   cnt;
    uint32_t * p;

    if ((pool == NULL) || (pool->sig != XOS_BLOCKPOOL_SIG)) {
        return -1;
    }

    p   = pool->head;
    cnt = 0;

    while (p != NULL) {
        if (((xos_uint32p_to_uint32(p)) & 0x3U) != 0U) {
            // Bad pointer, do not use
            return -1;
        }
        if (p[1] != XOS_BLOCKMEM_SIG) {
            return -1;
        }
        cnt++;
        // Try to make sure loop terminates eventually
        if (cnt > pool->nblks) {
            break;
        }
        p = xos_uint32_to_uint32p(p[0]);
    }

    if ((cnt != (uint32_t) uxSemaphoreGetCount(pool->sem)) || (cnt > pool->nblks)) {
        return -1;
    }

#if XOS_OPT_STATS
    if ((pool->num_allocs - pool->num_frees) != (pool->nblks - cnt)) {
        return -1;
    }
#endif

    return 0;
}

