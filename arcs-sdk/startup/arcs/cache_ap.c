/*
 * cache_ap.c
 *
 *  Created on: Jul 10, 2020
 *
 */
#include <assert.h>
#include "chip.h"
#include "nmsis_core.h"
#include "cache.h"

/**
 * @brief Enables the CPU instruction cache.
 *
 * This function activates the internal instruction cache of the CPU
 * to enhance the execution speed of programs. It is part of the Hardware
 * Abstraction Layer (HAL), providing an interface to manage hardware-specific
 * features such as caching operations. Enabling the instruction cache allows
 * for faster access to frequently executed instructions, which is crucial
 * for performance-critical applications.
 */
void HAL_EnableICache(void){
#if HAL_ICACHE_VALID
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

	EnableICache();


    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
#endif
}

/**
 * @brief Disables the CPU instruction cache.
 *
 * This function deactivates the internal instruction cache of the CPU
 * to potentially aid in debugging or to meet specific system requirements
 * where caching of instructions needs to be prevented. It is part of the Hardware
 * Abstraction Layer (HAL), providing an interface to manage hardware-specific
 * features such as caching operations. Disabling the instruction cache may be
 * necessary in scenarios where precise control over instruction execution is required.
 */
void HAL_DisableICache(void){
#if HAL_ICACHE_VALID
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

	DisableICache();


    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
#endif
}

/**
 * @brief Invalidates the CPU instruction cache.
 *
 * This function clears the contents of the internal instruction cache of the CPU.
 * Invalidating the cache is useful to ensure that no stale or corrupted data is used
 * by the CPU, which is particularly important after direct memory access (DMA) operations
 * or after loading new programs into memory. It is part of the Hardware Abstraction Layer (HAL),
 * providing an interface to manage hardware-specific features such as caching operations.
 * This operation helps in maintaining data coherency and consistency across the system.
 */
void HAL_InvalidateICache(void){
#if HAL_ICACHE_VALID
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

	MInvalICache();


    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
#endif
}

/**
 * @brief Invalidates a range of the CPU data cache based on address and size.
 *
 * This function clears a specific portion of the CPU's internal data cache. By providing
 * an address and the size of the area, this function ensures that any modifications in
 * memory in this specified range do not use stale or outdated cache entries. This is particularly
 * useful for systems where memory regions are dynamically altered or when devices not supporting
 * cache coherency modify the memory. It helps in maintaining data integrity and coherency
 * especially in systems involving direct memory access (DMA) operations.
 *
 * @param addr Pointer to the start address of the memory region to invalidate.
 * @param dsize Size of the memory region to invalidate, in bytes.
 */
void HAL_InvalidateICache_by_Addr(uint32_t *addr, uint32_t dsize){
#if HAL_ICACHE_VALID
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

    unsigned long cnt = 0;
    cnt = ((uint32_t)addr % HAL_ICACHE_CFG_LINE_SIZE + dsize + (HAL_ICACHE_CFG_LINE_SIZE - 1)) / HAL_ICACHE_CFG_LINE_SIZE;
    MInvalICacheLines((unsigned long)addr, (unsigned long)cnt);


    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
#endif
}

/**
 * @brief Locks a range of the CPU instruction cache based on address and size.
 *
 * This function prevents the CPU instruction cache from being updated or invalidated within a specified
 * range. It ensures that the cache entries in this range remain fixed and are not replaced or
 * evicted. This can be useful in scenarios where instruction stability is critical and should not be
 * changed by other operations or processes.
 *
 * @param addr Pointer to the start address of the memory region to lock.
 * @param dsize Size of the memory region to lock, in bytes.
 */
void HAL_LockICache_by_Addr(uint32_t *addr, uint32_t dsize){
#if HAL_ICACHE_VALID
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

    unsigned long cnt = 0;
    cnt = ((uint32_t)addr % HAL_ICACHE_CFG_LINE_SIZE + dsize + (HAL_ICACHE_CFG_LINE_SIZE - 1)) / HAL_ICACHE_CFG_LINE_SIZE;
    MLockICacheLines((unsigned long)addr, (unsigned long)cnt);


    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
#endif
}

/**
 * @brief Unlocks a previously locked range of the CPU instruction cache based on address and size.
 *
 * This function allows the CPU instruction cache to be updated or invalidated within a previously locked
 * range. It ensures that the cache entries in this range can now be replaced or evicted as needed,
 * returning the cache operation to its normal behavior. This is useful when the critical operation
 * requiring instruction stability is complete and normal cache operations need to resume.
 *
 * @param addr Pointer to the start address of the memory region to unlock.
 * @param dsize Size of the memory region to unlock, in bytes.
 */
void HAL_UnLockICache_by_Addr(uint32_t *addr, uint32_t dsize){
#if HAL_ICACHE_VALID
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

    unsigned long cnt = 0;
    cnt = ((uint32_t)addr % HAL_ICACHE_CFG_LINE_SIZE + dsize + (HAL_ICACHE_CFG_LINE_SIZE - 1)) / HAL_ICACHE_CFG_LINE_SIZE;
    MUnlockICacheLines((unsigned long)addr, (unsigned long)cnt);


    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
#endif
}


/**
 * @brief Enables the CPU data cache.
 *
 * This function activates the internal data cache of the CPU
 * to enhance the execution speed and efficiency of data access and processing.
 * It is part of the Hardware Abstraction Layer (HAL), providing an interface to manage
 * hardware-specific features such as caching operations. Enabling the data cache helps
 * improve system performance by reducing memory access times and minimizing CPU idle time
 * during data fetches from main memory.
 */
void HAL_EnableDCache(void){
#if HAL_DCACHE_VALID
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

	EnableDCache();


    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
#endif
}

/**
 * @brief Disables the CPU data cache.
 *
 * This function deactivates the internal data cache of the CPU.
 * Disabling the data cache can be useful in scenarios where data caching may lead
 * to consistency issues, such as during non-cache-coherent DMA operations or when
 * the predictability of every data access is required. It is part of the Hardware
 * Abstraction Layer (HAL), providing an interface to manage hardware-specific features.
 * Disabling the data cache ensures that all data reads and writes are directly made to
 * and from the main memory, which can be crucial for real-time and safety-critical applications.
 */
void HAL_DisableDCache(void){
#if HAL_DCACHE_VALID
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

	DisableDCache();


    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
#endif
}


/**
 * @brief Invalidates the CPU data cache.
 *
 * This function clears the contents of the internal data cache of the CPU.
 * Invalidating the cache is essential to prevent the use of stale or incorrect data
 * that might remain after changes in memory. It is commonly used after direct memory
 * access (DMA) operations or when hardware devices modify memory outside of the CPU's control.
 * It is part of the Hardware Abstraction Layer (HAL), providing an interface to manage
 * hardware-specific features such as caching operations. This operation ensures data coherency
 * and consistency across the system, particularly in systems where memory is shared between the CPU
 * and other hardware components.
 */
void HAL_InvalidateDCache(void){
#if HAL_DCACHE_VALID
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

    MInvalDCache();


    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
#endif
}

/**
 * @brief Flushes the CPU data cache.
 *
 * This function ensures that all modified data within the CPU's internal data cache
 * are written back to the main memory. Flushing the data cache is crucial before
 * any operations that require up-to-date data from other processors or hardware
 * that do not have cache coherency mechanisms. It is part of the Hardware
 * Abstraction Layer (HAL), providing an interface to manage hardware-specific features
 * such as caching operations. This operation helps maintain data coherency and
 * consistency across different parts of the system, particularly in multi-core
 * or multi-processor environments.
 */
void HAL_FlushDCache(void){
#if HAL_DCACHE_VALID
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

	MFlushDCache();


    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
#endif
}

/**
 * @brief Flushes and invalidates the CPU data cache.
 *
 * This function ensures that all modified data within the CPU's internal data cache
 * are written back to the main memory, and then invalidates the cache to remove all entries.
 * This is particularly useful in scenarios where data coherence and consistency are critical,
 * such as before DMA operations where peripheral devices need to access the latest data,
 * or after updating firmware that changes the memory layout. It is part of the Hardware
 * Abstraction Layer (HAL), providing an interface to manage hardware-specific features
 * such as caching operations. Flushing and invalidating the data cache ensures that
 * no stale data is used and all future data reads are done directly from the main memory.
 */
void HAL_FlushInvalidateDCache(void){
#if HAL_DCACHE_VALID
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

	MFlushInvalDCache();


    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
#endif
}

/**
 * @brief Invalidates a range of the CPU data cache based on address and size.
 *
 * This function clears a specific portion of the CPU's internal data cache. By providing
 * an address and the size of the area, this function ensures that any modifications in
 * memory in this specified range do not use stale or outdated cache entries. This is particularly
 * useful for systems where memory regions are dynamically altered or when devices not supporting
 * cache coherency modify the memory. It helps in maintaining data integrity and coherency
 * especially in systems involving direct memory access (DMA) operations.
 *
 * @param addr Pointer to the start address of the memory region to invalidate.
 * @param dsize Size of the memory region to invalidate, in bytes.
 */
_FAST_FUNC_SRAM void HAL_InvalidateDCache_by_Addr(uint32_t *addr, uint32_t dsize){
#if HAL_DCACHE_VALID
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

	unsigned long cnt = 0;
	cnt = ((uint32_t)addr % HAL_DCACHE_CFG_LINE_SIZE + dsize + (HAL_DCACHE_CFG_LINE_SIZE - 1)) / HAL_DCACHE_CFG_LINE_SIZE;
	MInvalDCacheLines((unsigned long)addr, (unsigned long)cnt);


    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
#endif
}

/**
 * @brief Flushes a range of the CPU data cache based on address and size.
 *
 * This function writes back all modified data within a specified range of the CPU's internal data cache
 * to the main memory. This operation is crucial for ensuring data coherence in systems where other processors
 * or hardware devices access the same memory region but do not share a cache coherency mechanism. It is typically
 * used prior to DMA operations or when processors in a multi-processor system access shared data.
 *
 * @param addr Pointer to the start address of the memory region to flush.
 * @param dsize Size of the memory region to flush, in bytes.
 */
_FAST_FUNC_SRAM void HAL_FlushDCache_by_Addr(uint32_t *addr, uint32_t dsize){
#if HAL_DCACHE_VALID
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

	unsigned long cnt = 0;
	cnt = ((uint32_t)addr % HAL_DCACHE_CFG_LINE_SIZE + dsize + (HAL_DCACHE_CFG_LINE_SIZE - 1)) / HAL_DCACHE_CFG_LINE_SIZE;
	MFlushDCacheLines((unsigned long)addr, (unsigned long)cnt);


    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
#endif
}

/**
 * @brief Flushes and invalidates a range of the CPU data cache based on address and size.
 *
 * This function combines the actions of writing back all modified data within a specified range
 * of the CPU's internal data cache to the main memory and then invalidating the cache entries.
 * This ensures that no stale data remains and all future accesses to this memory range will be
 * fetched directly from the main memory. This operation is particularly vital in systems with
 * non-cache-coherent DMA operations or in multi-core systems where processors need to share
 * up-to-date data without any inconsistencies.
 *
 * @param addr Pointer to the start address of the memory region to flush and invalidate.
 * @param dsize Size of the memory region to flush and invalidate, in bytes.
 */
void HAL_FlushInvalidateDCache_by_Addr(uint32_t *addr, uint32_t dsize){
#if HAL_DCACHE_VALID
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

	unsigned long cnt = 0;
	cnt = ((uint32_t)addr % HAL_DCACHE_CFG_LINE_SIZE + dsize + (HAL_DCACHE_CFG_LINE_SIZE - 1)) / HAL_DCACHE_CFG_LINE_SIZE;
	MFlushInvalDCacheLines((unsigned long)addr, (unsigned long)cnt);


    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
#endif
}

/**
 * @brief Locks a range of the CPU data cache based on address and size.
 *
 * This function prevents the CPU data cache from being updated or invalidated within a specified
 * range. It ensures that the cache entries in this range remain fixed and are not replaced or
 * evicted. This can be useful in scenarios where data stability is critical and should not be
 * changed by other operations or processes.
 *
 * @param addr Pointer to the start address of the memory region to lock.
 * @param dsize Size of the memory region to lock, in bytes.
 */
void HAL_LockDCache_by_Addr(uint32_t *addr, uint32_t dsize){
#if HAL_DCACHE_VALID
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

    unsigned long cnt = 0;
    cnt = ((uint32_t)addr % HAL_DCACHE_CFG_LINE_SIZE + dsize + (HAL_DCACHE_CFG_LINE_SIZE - 1)) / HAL_DCACHE_CFG_LINE_SIZE;
    MLockDCacheLines((unsigned long)addr, (unsigned long)cnt);


    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
#endif
}

/**
 * @brief Unlocks a previously locked range of the CPU data cache based on address and size.
 *
 * This function allows the CPU data cache to be updated or invalidated within a previously locked
 * range. It ensures that the cache entries in this range can now be replaced or evicted as needed,
 * returning the cache operation to its normal behavior. This is useful when the critical operation
 * requiring data stability is complete and normal cache operations need to resume.
 *
 * @param addr Pointer to the start address of the memory region to unlock.
 * @param dsize Size of the memory region to unlock, in bytes.
 */
void HAL_UnLockDCache_by_Addr(uint32_t *addr, uint32_t dsize){
#if HAL_DCACHE_VALID
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

    unsigned long cnt = 0;
    cnt = ((uint32_t)addr % HAL_DCACHE_CFG_LINE_SIZE + dsize + (HAL_DCACHE_CFG_LINE_SIZE - 1)) / HAL_DCACHE_CFG_LINE_SIZE;
    MUnlockDCacheLines((unsigned long)addr, (unsigned long)cnt);


    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
#endif
}

// This function will be abandon
int range_is_cacheable(unsigned long start, unsigned long size){
	return 0;
}

void unaligned_cache_line_move(unsigned char* src, unsigned char* dst, unsigned long len)
{
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

    int i;
    unsigned char* src_p = (unsigned char*) src;
    unsigned char* dst_p = (unsigned char*) dst;
    for (i = 0; i < len; ++i) {
        *(dst_p + i) = *(src_p + i);
    }

    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
}
void dcache_clean_range(unsigned long start, unsigned long end){
    uint32_t line_mask = HAL_DCACHE_CFG_LINE_SIZE - 1;
    
    // 对start向上取整到cache line边界
    unsigned long aligned_start = (start + line_mask) & (~line_mask);
    
    // 对end向下取整到cache line边界
    unsigned long aligned_end = end & (~line_mask);
    
    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

    // 只有当有完整的cache line需要处理时才进行操作
    if (aligned_start < aligned_end) {
        HAL_FlushDCache_by_Addr((uint32_t *)aligned_start, (aligned_end - aligned_start));
    }

    // 对不对齐的部分进行单独处理
    if (start < aligned_start) {
        // 处理start到aligned_start之间的数据
        // 这里需要更细粒度的处理方式
    }

    if (end > aligned_end) {
        // 处理aligned_end到end之间的数据
        // 这里需要更细粒度的处理方式
    }

    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
}

void dcache_invalidate_range(unsigned long start, unsigned long end){
    uint32_t line_mask = HAL_DCACHE_CFG_LINE_SIZE - 1;
    
    // 对start向上取整到cache line边界
    unsigned long aligned_start = (start + line_mask) & (~line_mask);
    
    // 对end向下取整到cache line边界
    unsigned long aligned_end = end & (~line_mask);

    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

    // 只有当有完整的cache line需要处理时才进行操作
    if (aligned_start < aligned_end) {
        HAL_InvalidateDCache_by_Addr((uint32_t *)aligned_start, (aligned_end - aligned_start));
    }

    // 对不对齐的部分进行单独处理
    if (start < aligned_start) {
        // 处理start到aligned_start之间的数据
        // 这里需要更细粒度的处理方式
    }

    if (end > aligned_end) {
        // 处理aligned_end到end之间的数据
        // 这里需要更细粒度的处理方式
    }

    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
}

void dcache_flush_range(unsigned long start, unsigned long end){
	uint32_t line_mask = HAL_DCACHE_CFG_LINE_SIZE - 1;
	
	// 对start向上取整到cache line边界
	unsigned long aligned_start = (start + line_mask) & (~line_mask);
	
	// 对end向下取整到cache line边界
	unsigned long aligned_end = end & (~line_mask);

    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

	// 只有当有完整的cache line需要处理时才进行操作
	if (aligned_start < aligned_end) {
		HAL_FlushDCache_by_Addr((uint32_t *)aligned_start, (aligned_end - aligned_start));
		HAL_InvalidateDCache_by_Addr((uint32_t *)aligned_start, (aligned_end - aligned_start));
	}

	// 对不对齐的部分进行单独处理
	if (start < aligned_start) {
		// 处理start到aligned_start之间的数据
		// 这里需要更细粒度的处理方式
	}

	if (end > aligned_end) {
		// 处理aligned_end到end之间的数据
		// 这里需要更细粒度的处理方式
	}

    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
}
void cache_dma_fast_inv_stage1(unsigned long start, unsigned long end){
unsigned long line_size;
	unsigned long old_start = start;
	unsigned long old_end = end;
	line_size = HAL_DCACHE_CFG_LINE_SIZE;
	start = start & (~(line_size - 1));
	end = (end + line_size - 1) & (~(line_size - 1));
	if (start == end)
		return;

    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

	if (start != old_start) {
		HAL_FlushDCache_by_Addr((uint32_t *)start, line_size);
	}
	if (end != old_end) {
		HAL_FlushDCache_by_Addr((uint32_t *)(end - line_size), line_size);
	}
	HAL_InvalidateDCache_by_Addr((uint32_t *)start, (end - start));

    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }
}
// void cache_dma_fast_inv_stage2(unsigned long start, unsigned long end){

// }
// dcache_clean_range(buff_addr, buff_addr + blk_sz * blk_cnt);
//         gm_cpu_clean_dcache_range(q->payload, q->len);
//         gm_cpu_dcache_invalidate_range(q->payload, q->len);
//             	cache_dma_fast_inv_stage1(buff_addr, buff_addr + blk_sz * blk_cnt);

//             	dcache_clean_range(buff_addr, buff_addr + blk_sz * blk_cnt);
// // usually called after transferring data to memory  (NOT cache-line-aligned) via DMA

void cache_dma_fast_inv_stage2(unsigned long start, unsigned long end)
{
	unsigned long line_size;
	unsigned long old_start = start;
	unsigned long old_end = end;
	static unsigned char cache_line_buf[32];

	line_size = HAL_DCACHE_CFG_LINE_SIZE;
	start = start & (~(line_size - 1));
	end = (end + line_size - 1) & (~(line_size - 1));
	if (start == end)
		return;

	int use_lock = 0;
	// interrupt enabled, and not in interrupt context

	if(start != old_start || end != old_end)
		use_lock = 1;

	if (use_lock) {
		disable_GINT();
	}

    // 原子地清除 MIE 位并返回旧值
    unsigned long mstatus = __RV_CSR_READ_CLEAR(CSR_MSTATUS, MSTATUS_MIE);

	if (start != old_start) {
		unaligned_cache_line_move((unsigned char*) start, cache_line_buf, old_start - start);
		HAL_InvalidateDCache_by_Addr((uint32_t *)start, line_size);
		unaligned_cache_line_move(cache_line_buf, (unsigned char*) start, old_start - start);
	}
	if (end != old_end) {
		unaligned_cache_line_move((unsigned char*) old_end, cache_line_buf, end - old_end);
		HAL_InvalidateDCache_by_Addr((uint32_t *)(end - line_size), line_size);
		unaligned_cache_line_move(cache_line_buf, (unsigned char*) old_end, end - old_end);
	}

    // 如果之前中断是开启的，恢复 MIE 位
    if (mstatus & MSTATUS_MIE) {
        __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
    }

	if (use_lock) {
		enable_GINT();
	}

}
