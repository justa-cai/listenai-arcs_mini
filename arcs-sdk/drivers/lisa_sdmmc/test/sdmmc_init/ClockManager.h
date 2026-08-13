#ifndef TEST_CLOCK_MANAGER_H
#define TEST_CLOCK_MANAGER_H

#include <stdint.h>

#define CRM_IpSrcFlashClk 1U

void HAL_CRM_SetSdio_hClkDiv(uint32_t n, uint32_t m);
void HAL_CRM_SetSdio_hClkSrc(uint32_t src);

#endif
