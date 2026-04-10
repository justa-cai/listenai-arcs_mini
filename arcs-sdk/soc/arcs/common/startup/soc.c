/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * LS26 SoC initialization - strong symbol implementations of soc_pre_init() and soc_init().
 *
 * Extracted from system.c (SystemInit, platform_pre_startup) and sys_main.c (PSRAM, watchdog).
 */

#include <stdint.h>
#include <stdio.h>
#include "arcs_ap.h"
#include "cache.h"
#include "ClockManager.h"

#if CONFIG_SYS_INIT
#include "sys_init.h"
#endif

#include "riscv_encoding.h"

/* ========================================================================
 * Forward declarations (internal helpers)
 * ======================================================================== */

extern volatile uint32_t SystemCoreClock;
extern volatile int HARTID;
extern volatile IRegion_Info_Type SystemIRegionInfo;

extern void SystemCoreClockUpdate(void);
extern void ECLIC_Init(void);
extern void non_cacheable_region_enable(uint32_t base_addr, uint32_t len);
extern void irq_vectors_init(void);

#define FALLBACK_DEFAULT_ECLIC_BASE             0x0C000000UL
#define FALLBACK_DEFAULT_SYSTIMER_BASE          0x02000000UL

static void _get_iregion_info(volatile IRegion_Info_Type *iregion)
{
    unsigned long mcfg_info;
    if (iregion == NULL) {
        return;
    }
    mcfg_info = __RV_CSR_READ(CSR_MCFG_INFO);
    if (mcfg_info & MCFG_INFO_IREGION_EXIST) {
        iregion->iregion_base = (__RV_CSR_READ(CSR_MIRGB_INFO) >> 10) << 10;
        iregion->eclic_base = iregion->iregion_base + IREGION_ECLIC_OFS;
        iregion->systimer_base = iregion->iregion_base + IREGION_TIMER_OFS;
        iregion->smp_base = iregion->iregion_base + IREGION_SMP_OFS;
        iregion->idu_base = iregion->iregion_base + IREGION_IDU_OFS;
    } else {
        iregion->eclic_base = FALLBACK_DEFAULT_ECLIC_BASE;
        iregion->systimer_base = FALLBACK_DEFAULT_SYSTIMER_BASE;
    }
}

static void Trap_Init(void)
{
}

static void PMP_Init(void)
{
    const uint32_t hm = HARTID << 21;
    pmp_config regions[] = {
        { .base_addr = 0x00080000|hm, .order = 14, .protection = PMP_L|PMP_R|PMP_W|PMP_X }, // +APILM:16KB
        { .base_addr = 0x00100000|hm, .order = 13, .protection = PMP_L|PMP_R|PMP_W       }, // +APDLM:8KB
        { .base_addr = 0x08000000,    .order = 27, .protection = PMP_L|PMP_R|PMP_W|PMP_X }, // +VADDR_REMAP:128MB (Region A)
        { .base_addr = 0x10000000,    .order = 28, .protection = PMP_L|PMP_R|PMP_W|PMP_X }, // +VADDR_REMAP:256MB (Region B-D)
        { .base_addr = 0x20000000,    .order = 29, .protection = PMP_L|PMP_R|PMP_W|PMP_X }, // +SRAM/PSRAM/FLASH:512MB
        { .base_addr = 0x40000000,    .order = 30, .protection = PMP_L|PMP_R|PMP_W       }, // +PERIPH:1GB
        { .base_addr = 0x80000000,    .order = 31, .protection = PMP_L|PMP_R|PMP_W       }, // +PERIPH:2GB
        { .base_addr = 0x00000000,    .order = 32, .protection = PMP_L                   }, // -DENY ALL:4GB
    };
    for (int i = 0; i < sizeof(regions)/sizeof(regions[0]); i++) __set_PMPENTRYx(i, &regions[i]);
}

static void ClockInit(void)
{
    extern void BootClock_Init();

#if CONFIG_CLOCK_INIT
    BootClock_Init();
    __FENCE_I();
#endif

    SystemCoreClockUpdate();
    __HAL_CRM_MTIME_CLK_ENABLE();
}

/* Exception_Init stays in system.c (uses static SystemExceptionHandlers array) */
extern void Exception_Init(void);


void soc_pre_init(void)
{
    /* CACHE Initialize (before scatterloading for better performance) */
#if CONFIG_ICACHE_ENABLE
#if defined(__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1)
    if (ICachePresent())
        HAL_EnableICache();
#endif
#else
    HAL_DisableICache();
#endif
}

int soc_cpu_id_get(void)
{
    return HARTID;
}

void soc_init(void)
{
    /* --- HARTID --- */
    HARTID = __RV_CSR_READ(CSR_MHARTID) & 0xff;

    #ifndef SYSTEM_CLOCK
    #define SYSTEM_CLOCK    (24000000UL)
    #endif
    SystemCoreClock = SYSTEM_CLOCK;
    _get_iregion_info(&SystemIRegionInfo);

    /* --- IRQ vector table --- */
    irq_vectors_init();

    /* --- Clock --- */
    ClockInit();


    /* --- Cache (D-cache only; I-cache moved to soc_pre_init) --- */
#if CONFIG_DCACHE_ENABLE
    HAL_EnableDCache();
#else
    HAL_DisableDCache();
#endif

    non_cacheable_region_enable(WIFI_RAM_REGION, (CMN_PSRAM_REGION - WIFI_RAM_REGION));
    __FENCE_I();
    /* --- PMP --- */
#if defined(__PMP_PRESENT)
    PMP_Init();
#endif

    /* --- Exception & ECLIC --- */
    Exception_Init();

    ECLIC_Init();
    Trap_Init();

    /* --- Final exception entry + ECLIC mode (moved from startup.S) --- */
    extern void exc_entry(void);
    __RV_CSR_WRITE(CSR_MTVEC, (unsigned long)&exc_entry);
    __RV_CSR_CLEAR(CSR_MTVEC, 0x3f);
    __RV_CSR_SET(CSR_MTVEC, 0x3);

    /* --- BPU enable (moved from startup.S) --- */
    __RV_CSR_SET(CSR_MMISC_CTL, MMISC_CTL_BPU);

    /* --- MSCRATCH for backtrace before RTOS (moved from startup.S) --- */
    extern char __StackTop[];
    __RV_CSR_WRITE(CSR_MSCRATCH, (unsigned long)__StackTop + 1024);

    /* --- PSRAM --- */
#if CONFIG_PSRAM_INIT
    {
        uint32_t rdly = 18, wdly = 22;
        PSRAM_Initialize(&rdly, &wdly, 1);
    }
#endif
    HAL_InvalidateDCache_by_Addr((uint32_t *)CONFIG_MEM_PSRAM_BASE, CONFIG_MEM_PSRAM_SIZE);
    __FENCE_I();

    extern void scatload_psram(void);
    scatload_psram();

    /* --- Watchdog --- */
#if CONFIG_WATCHDOG_ENABLE
    extern void boot_watchdog_init(void);
    boot_watchdog_init();
#elif CONFIG_BOOT_WITH_WATCHDOG
    extern void boot_watchdog_init(void);
    extern void boot_watchdog_enable(int enable);
    boot_watchdog_init();
    boot_watchdog_enable(0);
#endif
}
