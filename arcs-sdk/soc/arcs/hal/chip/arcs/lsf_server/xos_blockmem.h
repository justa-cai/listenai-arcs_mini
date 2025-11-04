/** @file */

// xos_blockmem.h - XOS block memory pool manager API.

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

// NOTE: Do not include this file directly in your application. Including
// xos.h will automatically include this file.

#ifndef XOS_BLOCKMEM_H
#define XOS_BLOCKMEM_H

// 平台
#include "FreeRTOS.h"
#include "semphr.h"

// 标准库
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


//-----------------------------------------------------------------------------
// Block Pool flags.
//-----------------------------------------------------------------------------
#define XOS_BLOCKMEM_WAIT_PRIORITY  0x0000U ///< Wake waiters in priority order (default)
#define XOS_BLOCKMEM_WAIT_FIFO      0x0001U ///< Wake waiters in FIFO order


//-----------------------------------------------------------------------------
///
/// XosBlockPool object.
///
//-----------------------------------------------------------------------------
typedef struct XosBlockPool {
  uint32_t              flags;          ///< Properties.
  uint32_t *            head;           ///< Pointer to free block list.
  uint32_t              nblks;          ///< Number of blocks in the pool.
  uint32_t *            first;          ///< Pointer to first block in pool.
  uint32_t *            last;           ///< Pointer to last block in pool.
  SemaphoreHandle_t     sem;            ///< Semaphore to track free count.
  uint32_t              sig;            // Valid signature indicates inited.
#if XOS_OPT_STATS
  uint32_t              num_allocs;     ///< Number of alloc requests.
  uint32_t              num_frees;      ///< Number of free requests.
  uint32_t              num_waits;      ///< Number of requests that blocked.
#endif
} XosBlockPool;


//-----------------------------------------------------------------------------
///
///  Initialize a block memory pool.
///
///  \param     pool            Pointer to block pool object.
///
///  \param     mem             Pointer to the memory area that the pool will use.
///                             This area must be at least (blocksize * nblocks)
///                             bytes in size.
///
///  \param     blocksize       Size of each block in bytes. Must be nonzero and
///                             a multiple of 4.
///
///  \param     nblocks         The number of blocks to be created. Must be > 0
///                             and <= INT32_MAX.
///
///  \param     flags           Creation flags:
///                             - XOS_BLOCKMEM_WAIT_FIFO -- queue waiting threads in
///                               fifo order.
///                             - XOS_BLOCKMEM_WAIT_PRIORITY -- queue waiting threads
///                               by priority. This is the default.
///
///  \return    Returns 0 on success, else error code.
///
//-----------------------------------------------------------------------------
int32_t
xos_block_pool_init(XosBlockPool * pool,
                    void *         mem,
                    uint32_t       blocksize,
                    uint32_t       nblocks,
                    uint32_t       flags);


//-----------------------------------------------------------------------------
///
///  Allocate a single block from the pool. If no block is available, the calling
///  thread waits until memory becomes available. This function cannot be called
///  from interrupt context because it can block.
///
///  \param     pool              Pointer to the memory pool to allocate from.
///
///  \return    Returns pointer to the allocated block on success, else XOS_NULL.
///
//-----------------------------------------------------------------------------
void *
xos_block_alloc(XosBlockPool * pool);


//-----------------------------------------------------------------------------
///
///  Nonblocking version of xos_block_alloc(). Returns immediately on failure.
///
///  \param     pool              Pointer to the memory pool to allocate from.
///
///  \return    Returns pointer to the allocated block on success, else XOS_NULL.
///
//-----------------------------------------------------------------------------
void *
xos_block_try_alloc(XosBlockPool * pool);


//-----------------------------------------------------------------------------
///
///  Free a single block back to the pool. The block must have been allocated
///  from the same pool.
///
///  \param     pool            Pointer to the memory pool to free into.
///
///  \param     mem             Pointer to the block to be freed.
///
///  \return    Returns 0 on success, XOS_ERR_ILLEGAL_OPERATION if the
///             block does not belong in this pool, else error code.
///
///  NOTE: Freeing a block can potentially wake up a higher priority thread and
///  cause a context switch.
///
//-----------------------------------------------------------------------------
int32_t
xos_block_free(XosBlockPool * pool, void * mem);


//-----------------------------------------------------------------------------
///
///  Verify that the state of the pool is consistent.
///
///  \param     pool            Pointer to memory pool to be checked.
///
///  \return    Returns 0 on success, XOS_ERR_INTERNAL_ERROR on error.
///
///  NOTE: The check is done with interrupts enabled. Make sure that the state
///  of the pool does not change while the check is running.
///
//-----------------------------------------------------------------------------
int32_t
xos_block_pool_check(const XosBlockPool * pool);


#ifdef __cplusplus
}
#endif

#endif  //      XOS_BLOCKMEM_H

