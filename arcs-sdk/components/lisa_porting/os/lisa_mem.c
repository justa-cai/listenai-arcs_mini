#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <lisa_mem.h>
#include <stdbool.h>
#include "sysheap.h"
#include "lisa_log.h"

#define TAG "lisa_mem"
#define LISA_MEM_MALLOC_MAX_SIZE (10240)

/**
 * @brief 从堆上分配指定大小的内存
 * 
 * @param size 
 * @return void* 
 */
void *lisa_mem_alloc(uint32_t size)
{
	if(size >= LISA_MEM_MALLOC_MAX_SIZE) {
		LISA_LOGV(TAG, "lisa mem alloc %d", size);
	}
	void *ptr = psram_malloc(size);
	return ptr;
}

void *lisa_mem_align_alloc(uint32_t align,uint32_t size)
{
	if(size >= LISA_MEM_MALLOC_MAX_SIZE) {
		LISA_LOGV(TAG, "lisa mem alloc %d", size);
	}
	void *ptr = psram_malloc_align(align, size);
	return ptr;
}


/**
 * @brief 释放指针指向的堆上已分配内存区域
 * 
 * @param ptr 
 */

void lisa_mem_free(void *ptr)
{

	if (ptr) {
		psram_free(ptr);
	} else {
		// LISA_LOGW(TAG, "lisa_mem_free NULL");
	}
}

/**
 * @brief 分配堆上指定的内存，并且将分配的内存初始化为零
 * 
 * @param count 
 * @param size 
 * @return void* 
 */
void *lisa_mem_calloc(uint32_t count, uint32_t size)
{
	uint32_t total_size = count * size;
	if(total_size >= LISA_MEM_MALLOC_MAX_SIZE) {
		LISA_LOGV(TAG, "lisa mem calloc %d %d", count, size);
	}
	void * ptr = psram_calloc(count, size);

	return ptr;
}

/**
 * @brief 给一个已经分配了地址的堆指针重新分配空间
 * 
 * @param ptr 
 * @param size 
 * @return void* 
 */
#define LISA_MEM_REALLOC_MAX_SIZE (20480)
void *lisa_mem_realloc(void *ptr, uint32_t size)
{
	if(size >= LISA_MEM_REALLOC_MAX_SIZE) {
		LISA_LOGD(TAG, "lisa mem realloc to %d > max %d",
										size, LISA_MEM_REALLOC_MAX_SIZE);
	}
	void *reptr = psram_realloc(ptr, size);
	return reptr;
}

void *lisa_mem_sram_alloc(uint32_t size)
{
	return lisa_mem_alloc(size);
}

void *lisa_mem_sram_calloc(uint32_t count, uint32_t size)
{
	return lisa_mem_calloc(count, size);
}

void *lisa_mem_sram_realloc(void *ptr, uint32_t size)
{
	return lisa_mem_realloc(ptr, size);
}

void lisa_mem_sram_free(void *ptr)
{
	if (ptr) {
		return lisa_mem_free(ptr);
	}
}
