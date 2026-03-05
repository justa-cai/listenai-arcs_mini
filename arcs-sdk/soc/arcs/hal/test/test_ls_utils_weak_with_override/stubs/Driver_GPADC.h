#ifndef DRIVER_GPADC_H
#define DRIVER_GPADC_H

#include <stdint.h>
#include "../mock_gpadc.h"

// Mock GPADC() macro
static inline void* GPADC(void) {
    return (void*)0x12345678;
}

// GPADC 控制相关常量定义
#define CSK_GPADC_CHANNEL_SEL_TEMP    (1U << 0)
#define CSK_GPADC_CHANNEL_SEL_VBAT    (1U << 1)
#define CSK_GPADC_DMA_ENABLE(x)       ((x) << 8)

#endif // DRIVER_GPADC_H
