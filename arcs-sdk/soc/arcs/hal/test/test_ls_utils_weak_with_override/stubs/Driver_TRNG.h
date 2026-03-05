#ifndef DRIVER_TRNG_H
#define DRIVER_TRNG_H

#include <stdint.h>

// Clock control macros
#define __HAL_CRM_TRNG_CLK_ENABLE() do {} while(0)
#define __HAL_CRM_TRNG_CLK_DISABLE() do {} while(0)

// TRNG function stubs
static inline void HAL_TRNG_Uninitialize(void* trng) { (void)trng; }
static inline void HAL_TRNG_PowerControl(void* trng, uint32_t state) { (void)trng; (void)state; }
static inline void HAL_TRNG_Initialize(void* trng) { (void)trng; }
static inline void HAL_TRNG_Control(void* trng, uint32_t control) { (void)trng; (void)control; }
static inline void HAL_TRNG_InterruptDisable(void* trng) { (void)trng; }
static inline void HAL_TRNG_Enable(void* trng) { (void)trng; }
static inline uint32_t HAL_TRNG_GetDataReady(void* trng) { (void)trng; return 1; }
static inline uint32_t HAL_TRNG_GetData(void* trng) { (void)trng; return 0x12345678; }

static inline void* TRNG(void) {
    return (void*)0x87654321;
}

// Power states
#define CSK_POWER_OFF   0
#define CSK_POWER_FULL  1

// TRNG 控制常量
#define CSK_TRNG_COLDTIME_2_23  (1U << 0)
#define CSK_TRNG_HOTTIME_2_17   (1U << 1)
#define CSK_TRNG_DELAYTIME_2_11 (1U << 2)

#endif // DRIVER_TRNG_H
