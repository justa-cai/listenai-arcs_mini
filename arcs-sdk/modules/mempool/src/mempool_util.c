#include "mempool_util.h"
#include "memorypool.h"
#include <lisa_log.h>
#ifndef PLAYER_USE_MEMPOOL
#include <lisa_mem.h>
#endif

#define MEMPOOL_TAG "MemPool"

static uint8_t s_inst_count = 0;
// 全局内存池
static MemoryPool *s_MemPool = NULL;

int mempool_init(uint32_t pool_size)
{
#ifdef PLAYER_USE_MEMPOOL
	s_inst_count++;
	if (s_MemPool) {
		return 1;
	}

	s_MemPool = MemoryPoolInit(pool_size * 2, pool_size);
	return s_MemPool ? 0 : -1;
#else
	return 0;
#endif
}

void mempool_uninit()
{
#ifdef PLAYER_USE_MEMPOOL
	s_inst_count--;
	if (s_inst_count == 0) {
		if (s_MemPool) {
			MemoryPoolClear(s_MemPool);
			MemoryPoolDestroy(s_MemPool);
			s_MemPool = NULL;
		}
	}
#endif
}

void *mempool_malloc(uint32_t size)
{
#ifdef PLAYER_USE_MEMPOOL
	if (s_MemPool) {
		return MemoryPoolAlloc(s_MemPool, size);
	}
	return NULL;
#else
	return lisa_mem_alloc(size);
#endif
}
void mempool_free(void *addr)
{
#ifdef PLAYER_USE_MEMPOOL
	if (s_MemPool) {
		MemoryPoolFree(s_MemPool, addr);
		addr = NULL;
	}
#else
	lisa_mem_free(addr);
#endif
}

unsigned long long mempool_usage()
{
#ifdef PLAYER_USE_MEMPOOL
	if (s_MemPool) {
		unsigned long long total = GetTotalMemory(s_MemPool);
		unsigned long long used = GetUsedMemory(s_MemPool);
		LISA_LOGI(MEMPOOL_TAG, "Memory Pool Usage, Total: %lld, Used: %lld", total, used);
		return used;
	}
#endif
	return 0;
}
