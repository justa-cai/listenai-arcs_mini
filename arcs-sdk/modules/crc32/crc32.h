#ifndef __CRC32_ALGORITHM_HEADER__
#define __CRC32_ALGORITHM_HEADER__

#include <stdint.h>

void crc32_init(void);
uint32_t crc32_calc(const void *data, uint32_t size, uint32_t last);

#endif//__CRC32_ALGORITHM_HEADER__
