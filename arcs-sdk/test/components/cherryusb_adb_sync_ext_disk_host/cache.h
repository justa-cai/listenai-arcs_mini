#ifndef TEST_CACHE_H
#define TEST_CACHE_H

#include <stdint.h>

void HAL_InvalidateDCache_by_Addr(uint32_t *addr, uint32_t dsize);

#endif
