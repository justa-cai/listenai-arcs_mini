/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * VenusA SoC initialization - soc_init() for ARCS SDK startup framework.
 *
 * Merges BSP SystemInit() + _premain_init() logic into the SDK boot sequence.
 */

#include <stdint.h>
#include <stdio.h>

#include "venusa_ap.h"
#include "cache.h"
#include "ClockManager.h"
#include "PSRAMUnified.h"
#include "PowerManager.h"

#if CONFIG_SOC_EARLY_LOG
#include <soc/early_log.h>
#endif

#if CONFIG_SYS_INIT
#include "sys_init.h"
#endif

#include "riscv_encoding.h"

/* ========================================================================
 * Forward declarations
 * ======================================================================== */

/* SystemCoreClock, SystemCoreClockUpdate, ECLIC_Init, irq_vectors_init
 * are defined below (replacing system_RISCVN300.c) */

#ifndef SYSTEM_CLOCK
#define SYSTEM_CLOCK    (24000000UL)
#endif

/* ========================================================================
 * Exception handling (re-implemented here because BSP's is static)
 * ======================================================================== */

#define MAX_SYSTEM_EXCEPTION_NUM    16

typedef void (*EXC_HANDLER)(unsigned long cause, unsigned long sp);

static void system_default_exception_handler(unsigned long mcause, unsigned long sp)
{
    printf("MCAUSE : 0x%lx\r\n", mcause);
    printf("MEPC   : 0x%lx\r\n", __RV_CSR_READ(CSR_MEPC));
    printf("MTVAL  : 0x%lx\r\n", __RV_CSR_READ(CSR_MTVAL));
    while (1);
}

static unsigned long SystemExceptionHandlers[MAX_SYSTEM_EXCEPTION_NUM + 1];

static void Exception_Init(void)
{
    for (int i = 0; i <= MAX_SYSTEM_EXCEPTION_NUM; i++) {
        SystemExceptionHandlers[i] = (unsigned long)system_default_exception_handler;
    }
}

uint32_t core_exception_handler(unsigned long mcause, unsigned long sp)
{
    uint32_t EXCn = (uint32_t)(mcause & 0x00000fff);
    EXC_HANDLER exc_handler;

    if (EXCn < MAX_SYSTEM_EXCEPTION_NUM) {
        exc_handler = (EXC_HANDLER)SystemExceptionHandlers[EXCn];
    } else if (EXCn == NMI_EXCn) {
        exc_handler = (EXC_HANDLER)SystemExceptionHandlers[MAX_SYSTEM_EXCEPTION_NUM];
    } else {
        exc_handler = (EXC_HANDLER)system_default_exception_handler;
    }
    if (exc_handler != NULL) {
        exc_handler(mcause, sp);
    }
    return 0;
}

void Exception_Register_EXC(uint32_t EXCn, unsigned long exc_handler)
{
    if (EXCn < MAX_SYSTEM_EXCEPTION_NUM) {
        SystemExceptionHandlers[EXCn] = exc_handler;
    } else if (EXCn == NMI_EXCn) {
        SystemExceptionHandlers[MAX_SYSTEM_EXCEPTION_NUM] = exc_handler;
    }
}

unsigned long Exception_Get_EXC(uint32_t EXCn)
{
    if (EXCn < MAX_SYSTEM_EXCEPTION_NUM) {
        return SystemExceptionHandlers[EXCn];
    } else if (EXCn == NMI_EXCn) {
        return SystemExceptionHandlers[MAX_SYSTEM_EXCEPTION_NUM];
    }
    return 0;
}

/* ========================================================================
 * PMP / Clock helpers
 * ======================================================================== */

static void PMP_Init(void)
{
    pmp_config pmp_config_rom = {
        .protection = PMP_L | PMP_R | PMP_X,
        .order = 19,
        .base_addr = 0,
    };
    pmp_config pmp_config_reserved = {
        .protection = PMP_L,
        .order = 29,
        .base_addr = 0,
    };
    __set_PMPENTRYx(0, &pmp_config_rom);
    __set_PMPENTRYx(1, &pmp_config_reserved);
}

static void ClockInit(void)
{
#if CONFIG_CLOCK_INIT
    extern void BootClock_Init(void);
    BootClock_Init();
    __FENCE_I();
#endif

    SystemCoreClockUpdate();
    __HAL_CRM_MTIME_CLK_ENABLE();
}

/* ========================================================================
 * Symbols previously provided by system_RISCVN300.c
 * ======================================================================== */

volatile uint32_t SystemCoreClock = SYSTEM_CLOCK;

/* cloglvl: BSP log level variable (used by CLOG macros in BSP drivers).
 * Originally defined in log_print.c which we excluded. */
uint32_t cloglvl = 0;
__attribute__((aligned(512))) void *OS_CPU_Vector_Table[IRQ_MAX] = {0};

void eclic_msip_handler(void) __attribute__((weak));
void eclic_mtip_handler(void) __attribute__((weak));

void SystemCoreClockUpdate(void)
{
    SystemCoreClock = SYSTEM_CLOCK;
}

void irq_vectors_init(void)
{
    OS_CPU_Vector_Table[SysTimerSW_IRQn] = &eclic_msip_handler;
    OS_CPU_Vector_Table[SysTimer_IRQn] = &eclic_mtip_handler;
}

void ECLIC_Init(void)
{
    ECLIC_SetMth(0);
    ECLIC_SetCfgNlbits(__ECLIC_INTCTLBITS);
}

int32_t ECLIC_Register_IRQ(IRQn_Type IRQn, uint8_t shv, ECLIC_TRIGGER_Type trig_mode,
                           uint8_t lvl, uint8_t priority, void *handler)
{
    if ((IRQn >= IRQ_MAX) || (shv > ECLIC_VECTOR_INTERRUPT)
        || (trig_mode > ECLIC_NEGTIVE_EDGE_TRIGGER)) {
        return -1;
    }
    ECLIC_SetShvIRQ(IRQn, shv);
    ECLIC_SetTrigIRQ(IRQn, trig_mode);
    ECLIC_SetLevelIRQ(IRQn, lvl);
    ECLIC_SetPriorityIRQ(IRQn, priority);
    if (handler != NULL) {
        ECLIC_SetVector(IRQn, (rv_csr_t)handler);
    }
    ECLIC_EnableIRQ(IRQn);
    return 0;
}

void register_ISR(uint32_t irq_no, ISR isr, ISR *isr_old)
{
    if (irq_no >= IRQ_MAX)
        return;
    ECLIC_SetShvIRQ(irq_no, ECLIC_NON_VECTOR_INTERRUPT);
    ECLIC_SetTrigIRQ(irq_no, ECLIC_LEVEL_TRIGGER);
    if (isr_old)
        *isr_old = (ISR)(OS_CPU_Vector_Table[irq_no]);
    OS_CPU_Vector_Table[irq_no] = isr;
    if (ECLIC_GetPriorityIRQ(irq_no) == 0) {
        ECLIC_SetLevelIRQ(irq_no, 0);
        ECLIC_SetPriorityIRQ(irq_no, 0);
    }
}

extern void default_intexc_handler(void);

#define DEFINE_INTERRUPT_HANDLER(irq)               \
    void Interrupt##irq##_Handler(void)             \
    {                                               \
        if (OS_CPU_Vector_Table[irq] != NULL) {     \
            ((ISR)(OS_CPU_Vector_Table[irq]))();    \
        } else {                                    \
            default_intexc_handler();               \
        }                                           \
    }

DEFINE_INTERRUPT_HANDLER(0)
DEFINE_INTERRUPT_HANDLER(1)
DEFINE_INTERRUPT_HANDLER(2)
DEFINE_INTERRUPT_HANDLER(3)
DEFINE_INTERRUPT_HANDLER(4)
DEFINE_INTERRUPT_HANDLER(5)
DEFINE_INTERRUPT_HANDLER(6)
DEFINE_INTERRUPT_HANDLER(7)
DEFINE_INTERRUPT_HANDLER(8)
DEFINE_INTERRUPT_HANDLER(9)
DEFINE_INTERRUPT_HANDLER(10)
DEFINE_INTERRUPT_HANDLER(11)
DEFINE_INTERRUPT_HANDLER(12)
DEFINE_INTERRUPT_HANDLER(13)
DEFINE_INTERRUPT_HANDLER(14)
DEFINE_INTERRUPT_HANDLER(15)
DEFINE_INTERRUPT_HANDLER(16)
DEFINE_INTERRUPT_HANDLER(17)
DEFINE_INTERRUPT_HANDLER(18)
DEFINE_INTERRUPT_HANDLER(19)
DEFINE_INTERRUPT_HANDLER(20)
DEFINE_INTERRUPT_HANDLER(21)
DEFINE_INTERRUPT_HANDLER(22)
DEFINE_INTERRUPT_HANDLER(23)
DEFINE_INTERRUPT_HANDLER(24)
DEFINE_INTERRUPT_HANDLER(25)
DEFINE_INTERRUPT_HANDLER(26)
DEFINE_INTERRUPT_HANDLER(27)
DEFINE_INTERRUPT_HANDLER(28)
DEFINE_INTERRUPT_HANDLER(29)
DEFINE_INTERRUPT_HANDLER(30)
DEFINE_INTERRUPT_HANDLER(31)
DEFINE_INTERRUPT_HANDLER(32)
DEFINE_INTERRUPT_HANDLER(33)
DEFINE_INTERRUPT_HANDLER(34)
DEFINE_INTERRUPT_HANDLER(35)
DEFINE_INTERRUPT_HANDLER(36)
DEFINE_INTERRUPT_HANDLER(37)
DEFINE_INTERRUPT_HANDLER(38)
DEFINE_INTERRUPT_HANDLER(39)
DEFINE_INTERRUPT_HANDLER(40)
DEFINE_INTERRUPT_HANDLER(41)
DEFINE_INTERRUPT_HANDLER(42)
DEFINE_INTERRUPT_HANDLER(43)
DEFINE_INTERRUPT_HANDLER(44)
DEFINE_INTERRUPT_HANDLER(45)
DEFINE_INTERRUPT_HANDLER(46)
DEFINE_INTERRUPT_HANDLER(47)
DEFINE_INTERRUPT_HANDLER(48)
DEFINE_INTERRUPT_HANDLER(49)
DEFINE_INTERRUPT_HANDLER(50)
DEFINE_INTERRUPT_HANDLER(51)
DEFINE_INTERRUPT_HANDLER(52)
DEFINE_INTERRUPT_HANDLER(53)
DEFINE_INTERRUPT_HANDLER(54)
DEFINE_INTERRUPT_HANDLER(55)
DEFINE_INTERRUPT_HANDLER(56)
DEFINE_INTERRUPT_HANDLER(57)
DEFINE_INTERRUPT_HANDLER(58)
DEFINE_INTERRUPT_HANDLER(59)
DEFINE_INTERRUPT_HANDLER(60)
DEFINE_INTERRUPT_HANDLER(61)
DEFINE_INTERRUPT_HANDLER(62)
DEFINE_INTERRUPT_HANDLER(63)
DEFINE_INTERRUPT_HANDLER(64)
DEFINE_INTERRUPT_HANDLER(65)
DEFINE_INTERRUPT_HANDLER(66)

#define DEFINE_NOCACHE_REGION_ENABLE(id)               \
    void non_cacheable_region_enable_##id(uint32_t base_addr, uint32_t len)  \
    {                                               \
        uint32_t pos = __CTZ(len);                  \
        uint32_t mnocm = ~((1 << pos) - 1);        \
        mnocm |= (base_addr & ((1 << pos) - 1));   \
        if (id < 2) {                               \
            __RV_CSR_WRITE(0x7f4 + 2*id, mnocm);   \
            __RV_CSR_WRITE(0x7f3 + 2*id, base_addr | 0x5); \
        } else if (id < 5) {                        \
            __RV_CSR_WRITE(0x7fa + 2*(id - 2), mnocm); \
            __RV_CSR_WRITE(0x7f9 + 2*(id - 2), base_addr | 0x5); \
        } else {                                    \
            __RV_CSR_WRITE(0xbe1 + 2*(id - 5), mnocm); \
            __RV_CSR_WRITE(0xbe0 + 2*(id - 5), base_addr | 0x5); \
        }                                           \
    }

#define DEFINE_NOCACHE_REGION_DISABLE(id)               \
    void non_cacheable_region_disable_##id(void)        \
    {                                               \
        if (id < 2)                                 \
            __RV_CSR_WRITE(0x7f3 + 2*id, 0x0);     \
        else if (id < 5)                            \
            __RV_CSR_WRITE(0x7f9 + 2*(id - 2), 0x0); \
        else                                        \
            __RV_CSR_WRITE(0xbe0 + 2*(id - 5), 0x0); \
    }

DEFINE_NOCACHE_REGION_ENABLE(0)
DEFINE_NOCACHE_REGION_ENABLE(1)
DEFINE_NOCACHE_REGION_ENABLE(2)
DEFINE_NOCACHE_REGION_ENABLE(3)
DEFINE_NOCACHE_REGION_ENABLE(4)
DEFINE_NOCACHE_REGION_ENABLE(5)
DEFINE_NOCACHE_REGION_ENABLE(6)
DEFINE_NOCACHE_REGION_ENABLE(7)

DEFINE_NOCACHE_REGION_DISABLE(0)
DEFINE_NOCACHE_REGION_DISABLE(1)
DEFINE_NOCACHE_REGION_DISABLE(2)
DEFINE_NOCACHE_REGION_DISABLE(3)
DEFINE_NOCACHE_REGION_DISABLE(4)
DEFINE_NOCACHE_REGION_DISABLE(5)
DEFINE_NOCACHE_REGION_DISABLE(6)
DEFINE_NOCACHE_REGION_DISABLE(7)

#define VENUSA_SRAM_PHYS_BASE 0x20000000U
#define VENUSA_SRAM_PHYS_SIZE 0x00080000U

void _init(void) { }
void _fini(void) { }

__attribute__((section(".init"))) void __sync_harts(void) { }

/* ========================================================================
 * SDK startup framework hooks
 * ======================================================================== */

void soc_pre_init(void)
{
}

void soc_init(void)
{
    /* --- PMU LDO --- */
    __HAL_PMU_AON_LDO_VMEM_ON();
    __HAL_PMU_AON_LDO_NORMAL_ON();
    __HAL_PMU_AON_LDO_VA_ON();

    /* --- System clock baseline --- */
    SystemCoreClock = SYSTEM_CLOCK;

    /* --- IRQ vector table --- */
    irq_vectors_init();

    /* --- Clock --- */
    ClockInit();

#if CONFIG_SOC_EARLY_LOG
    soc_early_log_init();
#endif

    /* --- Cache --- */
#if CONFIG_ICACHE_ENABLE
#if defined(__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1)
    if (ICachePresent())
        HAL_EnableICache();
#endif
#else
    HAL_DisableICache();
#endif

    /*
     * Keep the whole 512 KiB physical SRAM window non-cacheable. SRAM is fast
     * enough, and AP/CP use SRAM for shared synchronization while Flash is busy.
     */
    non_cacheable_region_enable_1(VENUSA_SRAM_PHYS_BASE, VENUSA_SRAM_PHYS_SIZE);

#if CONFIG_DCACHE_ENABLE
    HAL_EnableDCache();
#else
    HAL_DisableDCache();
#endif

    __FENCE_I();

    /* --- PMP --- */
#if defined(__PMP_PRESENT)
    PMP_Init();
#endif

    /* --- Exception & ECLIC --- */
    Exception_Init();
    ECLIC_Init();

    /* --- Exception entry + ECLIC mode --- */
    extern void exc_entry(void);
    __RV_CSR_WRITE(CSR_MTVEC, (unsigned long)&exc_entry);
    __RV_CSR_CLEAR(CSR_MTVEC, 0x3f);
    __RV_CSR_SET(CSR_MTVEC, 0x3);

    /* --- BPU enable --- */
    __RV_CSR_SET(CSR_MMISC_CTL, MMISC_CTL_BPU);

    /* --- AHB bus lock for atomic access --- */
    __RV_CSR_READ_SET(CSR_MMISC_CTL, (1 << 17));

    __RWMB();
    __FENCE_I();

    /* --- MSCRATCH for backtrace before RTOS --- */
    extern char __StackTop[];
    __RV_CSR_WRITE(CSR_MSCRATCH, (unsigned long)__StackTop + 1024);

    /* --- PSRAM --- */
#if CONFIG_PSRAM_INIT
    {
        uint32_t write_delay = 0x40;
        uint32_t read_delay = 0x30;
        __psram_unified_init_t init_para = {
            .die_type = __psram_unified_die_any,
            .search = 0,
            .read_delay = &read_delay,
            .write_delay = &write_delay,
            .fifo0_master = __psram_unified_ahb_master_luna_dat,
            .fifo1_master = __psram_unified_ahb_master_gpdma2d_m0,
            .fifo2_master = __psram_unified_ahb_master_gpdma2d_m1,
            .fifo0_enable = 1,
            .fifo1_enable = 1,
            .fifo2_enable = 1,
        };
        PSRAMUnified_Initialize(&init_para);
    }
#endif

#if CONFIG_MEM_PSRAM_SIZE > 0
    HAL_InvalidateDCache_by_Addr((uint32_t *)CONFIG_MEM_PSRAM_BASE, CONFIG_MEM_PSRAM_SIZE);
    __FENCE_I();

    extern void scatload_psram(void);
    scatload_psram();
#endif
}


int soc_cpu_id_get(void)
{
    return __get_hart_index();
}

/* 唤醒 CP (Core 1): 设入口地址 + 写复位魔术值释放 CP 复位。寄存器序列照搬
 * soc/venusa/hal/bsp/system_RISCVN300.c::start_core1 (BSP 文件被
 * CMakeLists.txt EXCLUDE, 这里就近提供 SDK 启动链路所需版本)。
 * 由 AP 上的 startup.S 早期调用, 把 BOOT_HARTID 指定的 CP 拉起跑 app。 */
void venusa_start_cp(uint32_t entry)
{
    IP_CMN_SYSCFG->REG_SW_RESET_CFG1.all = 0xCAFE000A;
    IP_CMN_SYSCFG->REG_N300_CORE1_RST_ADDR.all = entry;
    __RWMB();
    IP_CMN_SYSCFG->REG_SW_RESET_CORE1.all = 0xCAFE000A;
    __RWMB();
    IP_CMN_SYSCFG->REG_SW_RESET_CFG1.all = 0xCAFE000B;
}

int soc_boot_core(uint8_t target_core_id, uint32_t addr)
{
    if (soc_cpu_id_get() == target_core_id) {
        return -1;
    }

    if (target_core_id != 1) {
        return -1;
    }

    IP_CMN_SYSCFG->REG_SW_RESET_CFG1.all = 0xCAFE000A;
    IP_CMN_SYSCFG->REG_N300_CORE1_RST_ADDR.all = addr;
    __RWMB();
    IP_CMN_SYSCFG->REG_SW_RESET_CORE1.all = 0xCAFE000A;
    __RWMB();
    IP_CMN_SYSCFG->REG_SW_RESET_CFG1.all = 0xCAFE000B;

    return 0;
}
