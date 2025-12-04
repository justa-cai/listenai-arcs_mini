/*
 * Copyright (c) 2019 Nuclei Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/******************************************************************************
 * @file     system_demosoc.c
 * @brief    NMSIS Nuclei Core Device Peripheral Access Layer Source File for
 *           Nuclei Demo SoC which support Nuclei N/NX class cores
 * @version  V1.00
 * @date     22. Nov 2019
 ******************************************************************************/
#include <stdint.h>
#include <stdio.h>
#include "arcs_ap.h"
#include "cache.h"
#include "log_print.h"
#include "ClockManager.h"

#include "sysexec.h"

#if CFG_MEMDUMP
#include "memdump.h"
#endif

enum {
    NORMAL_MODE = 0,
    INTERRUPT_MODE,
    EXCE_MODE,
    NMI_MODE,
};

/*----------------------------------------------------------------------------
  Define clocks
 *----------------------------------------------------------------------------*/
/* ToDo: add here your necessary defines for device initialization
         following is an example for different system frequencies */
#ifndef SYSTEM_CLOCK
#define SYSTEM_CLOCK    (24000000UL)
#endif

/**
 * \defgroup  NMSIS_Core_SystemConfig       System Device Configuration
 * \brief Functions for system and clock setup available in system_<device>.c.
 * \details
 * Nuclei provides a template file **system_Device.c** that must be adapted by
 * the silicon vendor to match their actual device. As a <b>minimum requirement</b>,
 * this file must provide:
 *  -  A device-specific system configuration function, \ref SystemInit.
 *  -  A global variable that contains the system frequency, \ref SystemCoreClock.
 *  -  A global eclic configuration initialization, \ref ECLIC_Init.
 *  -  Global c library \ref _init and \ref _fini functions called right before calling main function.
 *  -  Vendor customized interrupt, exception and nmi handling code, see \ref NMSIS_Core_IntExcNMI_Handling
 *
 * The file configures the device and, typically, initializes the oscillator (PLL) that is part
 * of the microcontroller device. This file might export other functions or variables that provide
 * a more flexible configuration of the microcontroller system.
 *
 * And this file also provided common interrupt, exception and NMI exception handling framework template,
 * Silicon vendor can customize these template code as they want.
 *
 * \note Please pay special attention to the static variable \c SystemCoreClock. This variable might be
 * used throughout the whole system initialization and runtime to calculate frequency/time related values.
 * Thus one must assure that the variable always reflects the actual system clock speed.
 *
 * \attention
 * Be aware that a value stored to \c SystemCoreClock during low level initialization (i.e. \c SystemInit()) might get
 * overwritten by C libray startup code and/or .bss section initialization.
 * Thus its highly recommended to call \ref SystemCoreClockUpdate at the beginning of the user \c main() routine.
 *
 * @{
 */

/*----------------------------------------------------------------------------
  System Core Clock Variable
 *----------------------------------------------------------------------------*/
/* ToDo: initialize SystemCoreClock with the system core clock frequency value
         achieved after system intitialization.
         This means system core clock frequency after call to SystemInit() */
/**
 * \brief      Variable to hold the system core clock value
 * \details
 * Holds the system core clock, which is the system clock frequency supplied to the SysTick
 * timer and the processor core clock. This variable can be used by debuggers to query the
 * frequency of the debug timer or to configure the trace clock speed.
 *
 * \attention
 * Compilers must be configured to avoid removing this variable in case the application
 * program is not using it. Debugging systems require the variable to be physically
 * present in memory so that it can be examined to configure the debugger.
 */
volatile uint32_t SystemCoreClock = SYSTEM_CLOCK;  /* System Clock Frequency (Core Clock) */

volatile int HARTID = -1;

/*----------------------------------------------------------------------------
  Clock functions
 *----------------------------------------------------------------------------*/

/**
 * \brief      Function to update the variable \ref SystemCoreClock
 * \details
 * Updates the variable \ref SystemCoreClock and must be called whenever the core clock is changed
 * during program execution. The function evaluates the clock register settings and calculates
 * the current core clock.
 */
void SystemCoreClockUpdate(void)             /* Get Core Clock Frequency */
{
    /* ToDo: add code to calculate the system frequency based upon the current
     *    register settings.
     * Note: This function can be used to retrieve the system core clock frequeny
     *    after user changed register settings.
     */
    SystemCoreClock = CRM_GetSrcFreq(CRM_IpSrcCoreClk);//SYSTEM_CLOCK;
}

/**
 * \defgroup  NMSIS_Core_IntExcNMI_Handling   Interrupt and Exception and NMI Handling
 * \brief Functions for interrupt, exception and nmi handle available in system_<device>.c.
 * \details
 * Nuclei provide a template for interrupt, exception and NMI handling. Silicon Vendor could adapat according
 * to their requirement. Silicon vendor could implement interface for different exception code and
 * replace current implementation.
 *
 * @{
 */
/** \brief Max exception handler number, don't include the NMI(0xFFF) one */
#define MAX_SYSTEM_EXCEPTION_NUM        16
/**
 * \brief      Store the exception handlers for each exception ID
 * \note
 * - This SystemExceptionHandlers are used to store all the handlers for all
 * the exception codes Nuclei N/NX core provided.
 * - Exception code 0 - 11, totally 12 exceptions are mapped to SystemExceptionHandlers[0:11]
 * - Exception for NMI is also re-routed to exception handling(exception code 0xFFF) in startup code configuration, the handler itself is mapped to SystemExceptionHandlers[MAX_SYSTEM_EXCEPTION_NUM]
 */
static unsigned long SystemExceptionHandlers[MAX_SYSTEM_EXCEPTION_NUM + 1];

/**
 * \brief      Exception Handler Function Typedef
 * \note
 * This typedef is only used internal in this system_<Device>.c file.
 * It is used to do type conversion for registered exception handler before calling it.
 */
typedef void (*EXC_HANDLER)(unsigned long cause, unsigned long sp);

/**
 * \brief      System Default Exception Handler
 * \details
 * This function provides a default exception and NMI handler for all exception ids.
 * By default, It will just print some information for debug, Vendor can customize it according to its requirements.
 * \param [in]  mcause    code indicating the reason that caused the trap in machine mode
 * \param [in]  sp        stack pointer
 */
static void system_default_exception_handler(unsigned long mcause, unsigned long sp)
{
    unsigned long mstatus  = __RV_CSR_READ(CSR_MSTATUS);
    unsigned long mstratch = __RV_CSR_READ(CSR_MSCRATCH);

    uint8_t trap_before = (CSR_MSUBM_Type){.d=__RV_CSR_READ(CSR_MSUBM)}.b.ptyp;

    // 在异常处理期间，需要确保日志输出的及时性，禁用异步日志以避免数据丢失
#if defined(CONFIG_SDK_MODULE_EASYLOGGER) && defined(CONFIG_EASYLOGGER_LOG_MODE_ASYNC)
    #include "elog.h"
    elog_async_enabled(false);  // 切换到同步模式确保异常信息不会丢失
#endif

    // 关闭可能存在的后端,比如shell
#if defined(CONFIG_LOG)
    lisa_log_backend_pause_all();
#endif

    __disable_irq();

    CLOG_FLUSH();

    printf("\n");
    printf("*******************system exception******************\n");

    /* TODO: Uncomment this if you have implement printf function */
    printf("MCAUSE : 0x%08lx\n", (unsigned long)mcause);
    printf("MDCAUSE: 0x%08lx\n", (unsigned long)__RV_CSR_READ(CSR_MDCAUSE));
    printf("MEPC   : 0x%08lx\n", (unsigned long)__RV_CSR_READ(CSR_MEPC));
    printf("MTVAL  : 0x%08lx\n", (unsigned long)__RV_CSR_READ(CSR_MTVAL));
    printf("MSTATUS: 0x%08lx\n", (unsigned long)mstatus);
    printf("HARTID : %lu\r\n", (unsigned long)(__RV_CSR_READ(CSR_MHARTID) & 0xFF));
    printf("SP     : %lx\r\n", (unsigned long)sp);
    printf("MSTACK_CTL:   0x%08lx\r\n", (unsigned long)__RV_CSR_READ(mstack_ctl));
    printf("MSTACK_BASE:  0x%08lx\r\n", (unsigned long)__RV_CSR_READ(mstack_base));
    printf("MSTACK_bound: 0x%08lx\r\n", (unsigned long)__RV_CSR_READ(mstack_bound));
    printf("\n");

    if(trap_before == NMI_MODE || trap_before == EXCE_MODE){
        printf("*** NESTED EXCEPTION DETECTED ***\r\n");
        while(1);
    }

    Exception_DumpFrame(sp, PRV_M, mstatus, mstratch);

#if defined(SIMULATION_MODE)
    // directly exit if in SIMULATION
    extern void simulation_exit(int status);
    simulation_exit(1);
#else
#if CFG_MEMDUMP
    memdump_process(MDUMP_PATH_FLASH);
#else
    volatile bool flag = true;
    while (flag);
#endif
#endif
}

/**
 * \brief      Initialize all the default core exception handlers
 * \details
 * The core exception handler for each exception id will be initialized to \ref system_default_exception_handler.
 * \note
 * Called in \ref _init function, used to initialize default exception handlers for all exception IDs
 * SystemExceptionHandlers contains NMI, but SystemExceptionHandlers_S not, because NMI can't be delegated to S-mode.
 */
static void Exception_Init(void)
{
    for (int i = 0; i < MAX_SYSTEM_EXCEPTION_NUM; i++) {
        SystemExceptionHandlers[i] = (unsigned long)system_default_exception_handler;
    }
    SystemExceptionHandlers[MAX_SYSTEM_EXCEPTION_NUM] = (unsigned long)system_default_exception_handler;
}

__attribute__((unused)) static void hex_dump_word(uint32_t *data, size_t cnt) {
    for (size_t i = 0; i < cnt; i++, data++) {
        if (i % 4 == 0) {
            printf("\n");
        }
        printf("%08lx ", (unsigned long)*data);
    }

    printf("\n");
}

const char *reg_name[32] = {
    "x0", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "fp", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

static void dump_reg(uint32_t *data, size_t cnt) {

    if (cnt >= 32) {
        cnt = 32;
    }

    for (size_t i = 0; i < cnt; i += 4) {
        printf("%-4s: 0x%08lx\t", reg_name[i+0], (unsigned long)data[i+0]);
        printf("%-4s: 0x%08lx\t", reg_name[i+1], (unsigned long)data[i+1]);
        printf("%-4s: 0x%08lx\t", reg_name[i+2], (unsigned long)data[i+2]);
        printf("%-4s: 0x%08lx\n", reg_name[i+3], (unsigned long)data[i+3]);
    }

    printf("\n");
}

static unsigned long user_exc_handler = 0;
void Exception_DumpFrame(unsigned long sp, uint8_t mode, unsigned long mstatus, unsigned long mstratch)
{
    struct exec_frame *exc_frame = (struct exec_frame *)sp;
    if (user_exc_handler) {
        ((void(*)(void))user_exc_handler)();
    }
    printf("**************general purpose registers**************\n");
    dump_reg((uint32_t *)sp, 32);

    if (PRV_M == mode) {
        /* msubm is exclusive to machine mode */
        printf("msubm: 0x%08lx\n", (unsigned long)exc_frame->msubm);
    }

#if CONFIG_EXCEPTION_REBOOT_IMMEDIATELY
    sys_platform_sw_full_reset();
#endif

#ifdef CFG_BACK_TRACE
    extern void rv_backtrace_fault(uint32_t sp, struct exec_frame *frame, uint32_t mstatus, uint32_t mstratch);
    rv_backtrace_fault(sp, exc_frame, mstatus, mstratch);
#endif
}

void Exception_User_Register_EXC(unsigned long exc_handler)
{
    user_exc_handler = exc_handler;
    return;
}

/**
 * \brief       Register an exception handler for exception code EXCn
 * \details
 * - For EXCn < \ref MAX_SYSTEM_EXCEPTION_NUM, it will be registered into SystemExceptionHandlers[EXCn-1].
 * - For EXCn == NMI_EXCn, it will be registered into SystemExceptionHandlers[MAX_SYSTEM_EXCEPTION_NUM].
 * \param [in]  EXCn    See \ref EXCn_Type
 * \param [in]  exc_handler     The exception handler for this exception code EXCn
 */
void Exception_Register_EXC(uint32_t EXCn, unsigned long exc_handler)
{
    if (EXCn < MAX_SYSTEM_EXCEPTION_NUM) {
        SystemExceptionHandlers[EXCn] = exc_handler;
    } else if (EXCn == NMI_EXCn) {
        SystemExceptionHandlers[MAX_SYSTEM_EXCEPTION_NUM] = exc_handler;
    }
}

/**
 * \brief       Get current exception handler for exception code EXCn
 * \details
 * - For EXCn < \ref MAX_SYSTEM_EXCEPTION_NUM, it will return SystemExceptionHandlers[EXCn-1].
 * - For EXCn == NMI_EXCn, it will return SystemExceptionHandlers[MAX_SYSTEM_EXCEPTION_NUM].
 * \param [in]  EXCn    See \ref EXCn_Type
 * \return  Current exception handler for exception code EXCn, if not found, return 0.
 */
unsigned long Exception_Get_EXC(uint32_t EXCn)
{
    if (EXCn < MAX_SYSTEM_EXCEPTION_NUM) {
        return SystemExceptionHandlers[EXCn];
    } else if (EXCn == NMI_EXCn) {
        return SystemExceptionHandlers[MAX_SYSTEM_EXCEPTION_NUM];
    } else {
        return 0;
    }
}

/**
 * \brief      Common NMI and Exception handler entry
 * \details
 * This function provided a command entry for NMI and exception. Silicon Vendor could modify
 * this template implementation according to requirement.
 * \param [in]  mcause    code indicating the reason that caused the trap in machine mode
 * \param [in]  sp        stack pointer
 * \remarks
 * - RISCV provided common entry for all types of exception. This is proposed code template
 *   for exception entry function, Silicon Vendor could modify the implementation.
 * - For the core_exception_handler template, we provided exception register function \ref Exception_Register_EXC
 *   which can help developer to register your exception handler for specific exception number.
 */
uint32_t core_exception_handler(unsigned long mcause, unsigned long sp)
{
    uint32_t EXCn = (uint32_t)(mcause & 0X00000fff);
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
/** @} */ /* End of Doxygen Group NMSIS_Core_ExceptionAndNMI */


/**
 * \brief initialize eclic config
 * \details
 * ECLIC needs be initialized after boot up,
 * Vendor could also change the initialization
 * configuration.
 */
void ECLIC_Init(void)
{
    /* Global Configuration about MTH and NLBits.
     * TODO: Please adapt it according to your system requirement.
     * This function is called in _init function */
    ECLIC_SetMth(0);
    ECLIC_SetCfgNlbits(__ECLIC_INTCTLBITS);
}

/**
 * \brief  Initialize a specific IRQ and register the handler
 * \details
 * This function set vector mode, trigger mode and polarity, interrupt level and priority,
 * assign handler for specific IRQn.
 * \param [in]  IRQn        NMI interrupt handler address
 * \param [in]  shv         \ref ECLIC_NON_VECTOR_INTERRUPT means non-vector mode, and \ref ECLIC_VECTOR_INTERRUPT is vector mode
 * \param [in]  trig_mode   see \ref ECLIC_TRIGGER_Type
 * \param [in]  lvl         interupt level
 * \param [in]  priority    interrupt priority
 * \param [in]  handler     interrupt handler, if NULL, handler will not be installed
 * \return       -1 means invalid input parameter. 0 means successful.
 * \remarks
 * - This function use to configure specific eclic interrupt and register its interrupt handler and enable its interrupt.
 * - If the vector table is placed in read-only section(FLASHXIP mode), handler could not be installed
 */
int32_t ECLIC_Register_IRQ(IRQn_Type IRQn, uint8_t shv, ECLIC_TRIGGER_Type trig_mode, uint8_t lvl, uint8_t priority, void* handler)
{
    if ((IRQn >= IRQ_MAX) || (shv > ECLIC_VECTOR_INTERRUPT) \
        || (trig_mode > ECLIC_NEGTIVE_EDGE_TRIGGER)) {
        return -1;
    }

    /* set interrupt vector mode */
    ECLIC_SetShvIRQ(IRQn, shv);
    /* set interrupt trigger mode and polarity */
    ECLIC_SetTrigIRQ(IRQn, trig_mode);
    /* set interrupt level */
    ECLIC_SetLevelIRQ(IRQn, lvl);
    /* set interrupt priority */
    ECLIC_SetPriorityIRQ(IRQn, priority);
    if (handler != NULL) {
        /* set interrupt handler entry to vector table */
        ECLIC_SetVector(IRQn, (rv_csr_t)handler);
    }
    /* enable interrupt */
    ECLIC_EnableIRQ(IRQn);
    return 0;
}

/** @} */ /* End of Doxygen Group NMSIS_Core_ExceptionAndNMI */

#define FALLBACK_DEFAULT_ECLIC_BASE             0x0C000000UL
#define FALLBACK_DEFAULT_SYSTIMER_BASE          0x02000000UL

volatile IRegion_Info_Type SystemIRegionInfo;
static void _get_iregion_info(volatile IRegion_Info_Type *iregion)
{
    unsigned long mcfg_info;
    if (iregion == NULL) {
        return;
    }
    mcfg_info = __RV_CSR_READ(CSR_MCFG_INFO);
    if (mcfg_info & MCFG_INFO_IREGION_EXIST) { // IRegion Info present
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

/**
 * \brief do the init for trap(interrupt and exception) entry for supervisor mode
 * \details
 * This function provide initialization of CSR_STVT CSR_STVT2 and CSR_STVEC.
 */
static void Trap_Init(void)
{
}

/**
 * \brief do the init for memory protection entry, total 16 entries.
 * \details
 * This function provide initialization physical memory protection.
 */
static void PMP_Init(void)
{
    // [R]Read; [W]Write; [X]eXecute
    // [L]Lock:   0: No lock;  1: Lock Until Next Reset
    // > the L bit also indicates whether the R/W/X permissions are enforced on M-mode accesses
    // [A]AddressMatching(NA4 or NAPOT added automatically, OFF and TOR not considered here):
    // > 0-OFF: Null Region, means disabled
    // > 1-TOR: Top Of Range, range from previous end address to configured address
    // > 2-NA4: Naturally Aligned 4-bytes Region, always 4 bytes
    // > 3-NAPOT: Naturally Aligned Power-Of-Two Regison, >= 8bytes
    // also note that:
    // 1. small index has higher priority
    // 2. total 8 entries
    // 3. base_addr should aligned to 2^order
    const uint32_t hm = HARTID << 21;
    pmp_config regions[8] = {  // max items = __PMP_ENTRY_NUM / 2
        { .base_addr = 0x00080000|hm, .order = 14, .protection = PMP_L|PMP_R|PMP_W|PMP_X }, // +APILM:16KB
        { .base_addr = 0x00100000|hm, .order = 13, .protection = PMP_L|PMP_R|PMP_W       }, // +APDLM:8KB
        { .base_addr = 0x200B0000,    .order = 16, .protection = PMP_L|PMP_R|PMP_W|PMP_X }, // -SRAM:64KB
        { .base_addr = 0x200C0000,    .order = 15, .protection = PMP_L|PMP_R|PMP_W|PMP_X }, // +SRAM:32KB for BT EM
        { .base_addr = 0x20000000,    .order = 20, .protection = PMP_L|PMP_R|PMP_W|PMP_X }, // +SRAM:1MB
        { .base_addr = 0x28000000,    .order = 24, .protection = PMP_L|PMP_R|PMP_W|PMP_X }, // +PSRAM:16MB
        { .base_addr = 0x30000000,    .order = 25, .protection = PMP_L|PMP_R|PMP_X       }, // +FLASH:32MB
        { .base_addr = 0x00000000,    .order = 30, .protection = PMP_L                   }, // -MEM:1GB
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

/**
 * \brief      Function to Initialize the system.
 * \details
 * Initializes the microcontroller system. Typically, this function configures the
 * oscillator (PLL) that is part of the microcontroller device. For systems
 * with a variable clock speed, it updates the variable \ref SystemCoreClock.
 * SystemInit is called from the file <b>startup<i>_device</i></b>.
 */
void SystemInit(void)
{
	HARTID = __RV_CSR_READ(CSR_MHARTID) & 0xff;

    /* ToDo: add code to initialize the system
     * Warn: do not use global variables because this function is called before
     * reaching pre-main. RW section maybe overwritten afterwards.
     */
    SystemCoreClock = SYSTEM_CLOCK;
    _get_iregion_info(&SystemIRegionInfo);

    /* Vector Initialize */
    extern void irq_vectors_init(void);
    irq_vectors_init();

    /* Clock Initialize */
    ClockInit();

    /* CACHE Initialize */
#if CONFIG_ICACHE_ENABLE
#if defined(__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1)
    if (ICachePresent())
        HAL_EnableICache();
#endif
#else
    HAL_DisableICache();
#endif

#if CONFIG_DCACHE_ENABLE
    HAL_EnableDCache();
#else
    HAL_DisableDCache();
#endif

    non_cacheable_region_enable(WIFI_RAM_REGION, (CMN_PSRAM_REGION - WIFI_RAM_REGION));

    /* PMP Initialize*/
#if defined(__PMP_PRESENT)
    PMP_Init();
#endif

    /* Initialize exception default handlers */
    Exception_Init();

    /* ECLIC initialization, mainly MTH and NLBIT */
    ECLIC_Init();

    /* TRAP Initialize */
    Trap_Init();
}

////////////////////////////////////////////////////////////////////////////////
__attribute__((aligned (64))) void* OS_CPU_Vector_Table[IRQ_MAX] = { 0 };

void eclic_msip_handler(void) __attribute__((weak));
void eclic_mtip_handler(void) __attribute__((weak));

void irq_vectors_init(void)
{
    OS_CPU_Vector_Table[SysTimerSW_IRQn] = &eclic_msip_handler;
    OS_CPU_Vector_Table[SysTimer_IRQn] = &eclic_mtip_handler;
}


// Register ISR into Interrupt Vector Table
void register_ISR(uint32_t irq_no, ISR isr, ISR* isr_old)
{
    if (irq_no >= IRQ_MAX)   return;

	ECLIC_SetShvIRQ(irq_no, ECLIC_NON_VECTOR_INTERRUPT);
    /* set interrupt trigger mode */
    ECLIC_SetTrigIRQ(irq_no, ECLIC_LEVEL_TRIGGER);

    if (isr_old) *isr_old = (ISR)(OS_CPU_Vector_Table[irq_no]);
    OS_CPU_Vector_Table[irq_no] = isr;
    // default interrupt priority is 0, we assume that
    // 0 indicates the interrupt has not been initialized before...
    if (ECLIC_GetPriorityIRQ(irq_no) == 0){
        /* set interrupt level */
        ECLIC_SetLevelIRQ(irq_no, 0);
        /* set interrupt priority */
        ECLIC_SetPriorityIRQ(irq_no, 0);
    }
}

void non_cacheable_region_enable(uint32_t base_addr, uint32_t len){

	if (base_addr % len){
		return;
	}

	uint32_t mnocm = ~(len - 1);

    // Set cache region mask
    __RV_CSR_WRITE(CSR_MNOCM, mnocm);
    // Set base physical address and enable
    __RV_CSR_WRITE(CSR_MNOCB, base_addr | 0x1);
}

void non_cacheable_region_disable(void){
    // disable cache region
    __RV_CSR_WRITE(CSR_MNOCB, 0x0);
}

void device_region_enable(uint32_t base_addr, uint32_t len)
{

    if (base_addr % len) {
        return;
    }

    uint32_t mnocm = ~(len - 1);

    // Set dev region mask
    __RV_CSR_WRITE(CSR_MDEVM, mnocm);
    // Set base physical address and enable
    __RV_CSR_WRITE(CSR_MDEVB, base_addr | 0x1);
}

void device_region_disable(void){
    // disable device region
    __RV_CSR_WRITE(CSR_MDEVB, 0x0);
}

extern void default_intexc_handler(void);

// Function template of all interrupt handlers
#define DEFINE_INTERRUPT_HANDLER(irq)               \
    void Interrupt##irq##_Handler(void)             \
    {                                               \
        if (OS_CPU_Vector_Table[irq] != NULL)       \
            ((ISR)(OS_CPU_Vector_Table[irq]))();    \
        else                                        \
            default_intexc_handler();               \
    }

// define interrupt handler functions
DEFINE_INTERRUPT_HANDLER(0);
DEFINE_INTERRUPT_HANDLER(1);
DEFINE_INTERRUPT_HANDLER(2);
DEFINE_INTERRUPT_HANDLER(3);
DEFINE_INTERRUPT_HANDLER(4);
DEFINE_INTERRUPT_HANDLER(5);
DEFINE_INTERRUPT_HANDLER(6);
DEFINE_INTERRUPT_HANDLER(7);
DEFINE_INTERRUPT_HANDLER(8);
DEFINE_INTERRUPT_HANDLER(9);
DEFINE_INTERRUPT_HANDLER(10);
DEFINE_INTERRUPT_HANDLER(11);
DEFINE_INTERRUPT_HANDLER(12);
DEFINE_INTERRUPT_HANDLER(13);
DEFINE_INTERRUPT_HANDLER(14);
DEFINE_INTERRUPT_HANDLER(15);
DEFINE_INTERRUPT_HANDLER(16);
DEFINE_INTERRUPT_HANDLER(17);
DEFINE_INTERRUPT_HANDLER(18);
DEFINE_INTERRUPT_HANDLER(19);
DEFINE_INTERRUPT_HANDLER(20);
DEFINE_INTERRUPT_HANDLER(21);
DEFINE_INTERRUPT_HANDLER(22);
DEFINE_INTERRUPT_HANDLER(23);
DEFINE_INTERRUPT_HANDLER(24);
DEFINE_INTERRUPT_HANDLER(25);
DEFINE_INTERRUPT_HANDLER(26);
DEFINE_INTERRUPT_HANDLER(27);
DEFINE_INTERRUPT_HANDLER(28);
DEFINE_INTERRUPT_HANDLER(29);
DEFINE_INTERRUPT_HANDLER(30);
DEFINE_INTERRUPT_HANDLER(31);
DEFINE_INTERRUPT_HANDLER(32);
DEFINE_INTERRUPT_HANDLER(33);
DEFINE_INTERRUPT_HANDLER(34);
DEFINE_INTERRUPT_HANDLER(35);
DEFINE_INTERRUPT_HANDLER(36);
DEFINE_INTERRUPT_HANDLER(37);
DEFINE_INTERRUPT_HANDLER(38);
DEFINE_INTERRUPT_HANDLER(39);
DEFINE_INTERRUPT_HANDLER(40);
DEFINE_INTERRUPT_HANDLER(41);
DEFINE_INTERRUPT_HANDLER(42);
DEFINE_INTERRUPT_HANDLER(43);
DEFINE_INTERRUPT_HANDLER(44);
DEFINE_INTERRUPT_HANDLER(45);
DEFINE_INTERRUPT_HANDLER(46);
DEFINE_INTERRUPT_HANDLER(47);
DEFINE_INTERRUPT_HANDLER(48);
DEFINE_INTERRUPT_HANDLER(49);
DEFINE_INTERRUPT_HANDLER(50);
DEFINE_INTERRUPT_HANDLER(51);
DEFINE_INTERRUPT_HANDLER(52);
DEFINE_INTERRUPT_HANDLER(53);
DEFINE_INTERRUPT_HANDLER(54);
DEFINE_INTERRUPT_HANDLER(55);
DEFINE_INTERRUPT_HANDLER(56);
DEFINE_INTERRUPT_HANDLER(57);
DEFINE_INTERRUPT_HANDLER(58);
DEFINE_INTERRUPT_HANDLER(59);
DEFINE_INTERRUPT_HANDLER(60);
DEFINE_INTERRUPT_HANDLER(61);
DEFINE_INTERRUPT_HANDLER(62);
DEFINE_INTERRUPT_HANDLER(63);
DEFINE_INTERRUPT_HANDLER(64);
DEFINE_INTERRUPT_HANDLER(65);
DEFINE_INTERRUPT_HANDLER(66);
DEFINE_INTERRUPT_HANDLER(67);
DEFINE_INTERRUPT_HANDLER(68);
DEFINE_INTERRUPT_HANDLER(69);
DEFINE_INTERRUPT_HANDLER(70);
DEFINE_INTERRUPT_HANDLER(71);
DEFINE_INTERRUPT_HANDLER(72);
DEFINE_INTERRUPT_HANDLER(73);
DEFINE_INTERRUPT_HANDLER(74);
DEFINE_INTERRUPT_HANDLER(75);
DEFINE_INTERRUPT_HANDLER(76);
DEFINE_INTERRUPT_HANDLER(77);


/** @} */ /* End of Doxygen Group NMSIS_Core_SystemAndClock */

#define REBOOT_PASS_PIN 0xCAFE000A

/* API for software reboot/full_reset */
void sys_platform_sw_full_reset(void)
{
    IP_AON_CTRL->REG_AON_SW_RESET.all = REBOOT_PASS_PIN;
}
