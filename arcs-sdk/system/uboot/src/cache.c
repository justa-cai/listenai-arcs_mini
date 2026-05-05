#include "chip.h"
#include "nmsis_core.h"
#include "cache.h"

void HAL_InvalidateDCache_by_Addr(uint32_t *addr, uint32_t dsize)
{
#if HAL_DCACHE_VALID
    unsigned long cnt;

    __disable_irq();
    cnt =
        ((uint32_t)addr % HAL_DCACHE_CFG_LINE_SIZE + dsize + (HAL_DCACHE_CFG_LINE_SIZE - 1)) / HAL_DCACHE_CFG_LINE_SIZE;
    MInvalDCacheLines((unsigned long)addr, cnt);
    __enable_irq();
#else
    (void)addr;
    (void)dsize;
#endif
}

void HAL_FlushDCache_by_Addr(uint32_t *addr, uint32_t dsize)
{
#if HAL_DCACHE_VALID
    unsigned long cnt;

    __disable_irq();
    cnt =
        ((uint32_t)addr % HAL_DCACHE_CFG_LINE_SIZE + dsize + (HAL_DCACHE_CFG_LINE_SIZE - 1)) / HAL_DCACHE_CFG_LINE_SIZE;
    MFlushDCacheLines((unsigned long)addr, cnt);
    __enable_irq();
#else
    (void)addr;
    (void)dsize;
#endif
}

void HAL_FlushInvalidateDCache_by_Addr(uint32_t *addr, uint32_t dsize)
{
#if HAL_DCACHE_VALID
    unsigned long cnt;

    __disable_irq();
    cnt =
        ((uint32_t)addr % HAL_DCACHE_CFG_LINE_SIZE + dsize + (HAL_DCACHE_CFG_LINE_SIZE - 1)) / HAL_DCACHE_CFG_LINE_SIZE;
    MFlushInvalDCacheLines((unsigned long)addr, cnt);
    __enable_irq();
#else
    (void)addr;
    (void)dsize;
#endif
}
