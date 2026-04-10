#ifndef PSRAM_MANAGER_H
#define PSRAM_MANAGER_H

#include <stdint.h>

#define PSRAM_BASE_ADDRESS 0x0

int DCachePresent(void);
void HAL_FlushDCache_by_Addr(void *addr, uint32_t len);
void vPortEnterCritical(void);
void vPortExitCritical(void);

#endif
