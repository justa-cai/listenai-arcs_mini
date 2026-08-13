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
#include "venusa_ap.h"
#include "log_print.h"
#include "ClockManager.h"
#include "PSRAMUnified.h"
#include "PowerManager.h"

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
__attribute__((aligned (512))) void* OS_CPU_Vector_Table[IRQ_MAX] = { 0 };


void eclic_msip_handler(void) __attribute__((weak));
void eclic_mtip_handler(void) __attribute__((weak));

void irq_vectors_init(void);

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
    SystemCoreClock = SYSTEM_CLOCK;
}


extern uint32_t __copy_table_preinit_start__[] __attribute__((weak));
extern uint32_t __copy_table_preinit_end__[]   __attribute__((weak));
extern uint32_t __zero_table_preinit_start__[] __attribute__((weak));
extern uint32_t __zero_table_preinit_end__[]   __attribute__((weak));

extern uint32_t __copy_table_start__[] __attribute__((weak));
extern uint32_t __copy_table_end__[]   __attribute__((weak));
extern uint32_t __zero_table_start__[] __attribute__((weak));
extern uint32_t __zero_table_end__[]   __attribute__((weak));

void _copy_table_preinit(void) {
  for (uint32_t *p = __copy_table_preinit_start__; p < __copy_table_preinit_end__; p += 3)
    __builtin_memcpy((void*)p[1], (void*)p[0], (uintptr_t)p[2] - (uintptr_t)p[1]);
}

void _zero_table_preinit(void) {
  for (uint32_t *p = __zero_table_preinit_start__; p < __zero_table_preinit_end__; p += 2)
    __builtin_memset((void*)p[0], 0, (uintptr_t)p[1] - (uintptr_t)p[0]);
}

void _copy_table_init(void) {

  for (uint32_t *p = __copy_table_start__; p < __copy_table_end__; p += 3)
    __builtin_memcpy((void*)p[1], (void*)p[0], (uintptr_t)p[2] - (uintptr_t)p[1]);
}

void _zero_table_init(void) {
  for (uint32_t *p = __zero_table_start__; p < __zero_table_end__; p += 2)
    __builtin_memset((void*)p[0], 0, (uintptr_t)p[1] - (uintptr_t)p[0]);
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
#if (IC_BOARD == 1)
    __HAL_PMU_AON_LDO_VMEM_ON();
    __HAL_PMU_AON_LDO_NORMAL_ON();
    __HAL_PMU_AON_LDO_VA_ON();
    extern void BootClock_Init();
    BootClock_Init();
#endif
    _copy_table_preinit();
    _zero_table_preinit();

    /* ToDo: add code to initialize the system
     * Warn: do not use global variables because this function is called before
     * reaching pre-main. RW section maybe overwritten afterwards.
     */
    SystemCoreClock = SYSTEM_CLOCK;

    __HAL_CRM_MTIME_CLK_ENABLE();

    irq_vectors_init();
    
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
#if CONFIG_SKIP_EXCPT_LOG // DON'T print exception log for ATE TEST
#define DEBUG_LOG  0
#else
#define DEBUG_LOG  1
#endif

#if DEBUG_LOG
    CLOG("MCAUSE : 0x%lx\r\n", mcause);
    CLOG("MDCAUSE: 0x%lx\r\n", __RV_CSR_READ(CSR_MDCAUSE));
    CLOG("MEPC   : 0x%lx\r\n", __RV_CSR_READ(CSR_MEPC));
    CLOG("MTVAL  : 0x%lx\r\n", __RV_CSR_READ(CSR_MTVAL));
    CLOG("HARTID : %u\r\n", (__RV_CSR_READ(CSR_MHARTID) & 0xFF));
    Exception_DumpFrame(sp, PRV_M);
#endif

#if defined(SIMULATION_MODE)
    // directly exit if in SIMULATION
    extern void simulation_exit(int status);
    simulation_exit(1);
#else
    while (1);
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

/**
 * \brief      Dump Exception Frame
 * \details
 * This function provided feature to dump exception frame stored in stack.
 * \param [in]  sp    stackpoint
 * \param [in]  mode  privileged mode to decide whether to dump msubm CSR
 */
void Exception_DumpFrame(unsigned long sp, uint8_t mode)
{
    EXC_Frame_Type *exc_frame = (EXC_Frame_Type *)sp;

#ifndef __riscv_32e
    CLOGD("ra: 0x%lx, tp: 0x%lx, t0: 0x%lx, t1: 0x%lx, t2: 0x%lx, t3: 0x%lx, t4: 0x%lx, t5: 0x%lx, t6: 0x%lx\n" \
           "a0: 0x%lx, a1: 0x%lx, a2: 0x%lx, a3: 0x%lx, a4: 0x%lx, a5: 0x%lx, a6: 0x%lx, a7: 0x%lx\n" \
           "cause: 0x%lx, epc: 0x%lx\n", exc_frame->ra, exc_frame->tp, exc_frame->t0, \
           exc_frame->t1, exc_frame->t2, exc_frame->t3, exc_frame->t4, exc_frame->t5, exc_frame->t6, \
           exc_frame->a0, exc_frame->a1, exc_frame->a2, exc_frame->a3, exc_frame->a4, exc_frame->a5, \
           exc_frame->a6, exc_frame->a7, exc_frame->cause, exc_frame->epc);
#else
    CLOGD("ra: 0x%lx, tp: 0x%lx, t0: 0x%lx, t1: 0x%lx, t2: 0x%lx\n" \
           "a0: 0x%lx, a1: 0x%lx, a2: 0x%lx, a3: 0x%lx, a4: 0x%lx, a5: 0x%lx\n" \
           "cause: 0x%lx, epc: 0x%lx\n", exc_frame->ra, exc_frame->tp, exc_frame->t0, \
           exc_frame->t1, exc_frame->t2, exc_frame->a0, exc_frame->a1, exc_frame->a2, exc_frame->a3, \
           exc_frame->a4, exc_frame->a5, exc_frame->cause, exc_frame->epc);
#endif

    if (PRV_M == mode) {
        /* msubm is exclusive to machine mode */
        CLOGD("msubm: 0x%lx\n", exc_frame->msubm);
    }
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


/**
 * \brief Synchronize all harts
 * \details
 * This function is used to synchronize all the harts,
 * especially to wait the boot hart finish initialization of
 * data section, bss section and c runtines initialization
 * This function must be placed in .init section, since
 * section initialization is not ready, global variable
 * and static variable should be avoid to use in this function,
 * and avoid to call other functions
 */
#define CLINT_MSIP(base, hartid)    (*(volatile uint32_t *)((uintptr_t)((base) + ((hartid) * 4))))
#define SMP_CTRLREG(base, ofs)      (*(volatile uint32_t *)((uintptr_t)((base) + (ofs))))

__attribute__((section(".init"))) void __sync_harts(void)
{
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
 * Remove X permission of protected_execute region: PMP_L | PMP_R | PMP_W
 * Remove R permission of protected_data region: PMP_L | PMP_W
 * Remove W permission of protected_data region: PMP_L | PMP_R
 */
static void PMP_Init(void)
{
    /* Configuration of execution region*/
    pmp_config pmp_config_rom = {
        /*
         * Locked PMP entries remain locked until the hart is reset,
         * the L bit also indicates whether the R/W/X permissions are enforced on M-mode accesses
         */
        .protection = PMP_L | PMP_R | PMP_X,
        /* initial protected readable/writable address range is 2^12 = 4K bytes */
        .order = 19,
        /* initial base address is 0, change it to your memory assignment */
        .base_addr = 0,
    };

    pmp_config pmp_config_reserved = {
        /*
         * Locked PMP entries remain locked until the hart is reset,
         * the L bit also indicates whether the R/W/X permissions are enforced on M-mode accesses
         */
        .protection = PMP_L,
        /* initial protected readable/writable address range is 2^12 = 4K bytes */
        .order = 29,
        /* initial base address is 0, change it to your memory assignment */
        .base_addr = 0,
    };

    // Set PMP entry0 to protect ROM section and Reserved section (0x0 - 0x1FFF_FFFF)
    __set_PMPENTRYx(0, &pmp_config_rom);
    __set_PMPENTRYx(1, &pmp_config_reserved);
}



/**
 * \brief early init function before main
 * \details
 * This function is executed right before main function.
 * For RISC-V gnu toolchain, _init function might not be called
 * by __libc_init_array function, so we defined a new function
 * to do initialization.
 */
void _premain_init(void)
{
    unsigned long hartid = __RV_CSR_READ(CSR_MHARTID) & 0xFF;
    if (hartid == BOOT_HARTID) { // only required for boot hartid
        /* Initialize exception default handlers */
        Exception_Init();
        /* ECLIC initialization, mainly MTH and NLBIT */
        ECLIC_Init();

        // TODO: Clock Initialization here
    #if !CONFIG_SKIP_PSRAM_INIT
    uint32_t write_delay = 70;
    uint32_t read_delay = 38;

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
    #endif
        _copy_table_init();
        _zero_table_init();

    } else {
        /* for the other harts */
        __RV_CSR_WRITE(CSR_MTVT, OS_CPU_Vector_Table);
        ECLIC_Init();
    }

#if defined(__PMP_PRESENT)
    // TODO: disable PMP region temporarily
    // PMP_Init();
#endif

#if defined(__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1)
    if (ICachePresent()) { // Check whether icache real present or not
        EnableICache();
    }
#endif

#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1)
    if (DCachePresent()) { // Check whether dcache real present or not
        EnableDCache();
    }
#endif

    // Enable AHB bus lock signal to guarantee atomic access to memory
    __RV_CSR_READ_SET(CSR_MMISC_CTL, (1 << 17));

    /* Do fence and fence.i to make sure previous ilm/dlm/icache/dcache control done */
    __RWMB();
    __FENCE_I();

#ifndef CFG_RTOS
	enable_GINT();
#endif

}

/**
 * \brief finish function after main
 * \param [in]  status     status code return from main
 * \details
 * This function is executed right after main function.
 * For RISC-V gnu toolchain, _fini function might not be called
 * by __libc_fini_array function, so we defined a new function
 * to do initialization
 */
void _postmain_fini(int status)
{
    /* TODO: Add your own finishing code here, called after main */
}

/**
 * \brief _init function called in __libc_init_array()
 * \details
 * This `__libc_init_array()` function is called during startup code,
 * user need to implement this function, otherwise when link it will
 * error init.c:(.text.__libc_init_array+0x26): undefined reference to `_init'
 * \note
 * Please use \ref _premain_init function now
 */
void _init(void)
{
    /* Don't put any code here, please use _premain_init now */
}

/**
 * \brief _fini function called in __libc_fini_array()
 * \details
 * This `__libc_fini_array()` function is called when exit main.
 * user need to implement this function, otherwise when link it will
 * error fini.c:(.text.__libc_fini_array+0x28): undefined reference to `_fini'
 * \note
 * Please use \ref _postmain_fini function now
 */
void _fini(void)
{
    /* Don't put any code here, please use _postmain_fini now */
}


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

// Function template of all interrupt handlers
// Tips:
// 1 ------------------------------------------------
// The higher bits of mattrin_mask should be continuously 1, the remaining lower bits should be all 0. Technically the
// number (N) of 0 means the size of this region(2^N bytes).
// 2 ------------------------------------------------
// The BPA of mattrin_base should be integer multiples of the size of this region. A tip to check a new address is in
// the region or not: New Address & mattrin_mask == mattrin_base & mattrin_mask? Y, N
#define DEFINE_NOCACHE_REGION_ENABLE(id)               \
    void non_cacheable_region_enable_##id(uint32_t base_addr, uint32_t len)             \
    {                                               \
        uint32_t pos = __CTZ(len);	\
    	uint32_t mnocm = ~((1 << pos) - 1);		\
    	mnocm |= (base_addr & ((1 << pos) - 1));	\
    	if (id < 2) {	\
		    __RV_CSR_WRITE(0x7f4 + 2*id, mnocm);	\
		    __RV_CSR_WRITE(0x7f3 + 2*id, base_addr | 0x5);  \
    	} else if (id < 5) {	\
			__RV_CSR_WRITE(0x7fa + 2*(id - 2), mnocm);	\
		    __RV_CSR_WRITE(0x7f9 + 2*(id - 2), base_addr | 0x5);  \
    	} else {	\
			__RV_CSR_WRITE(0xbe1 + 2*(id - 5), mnocm);	\
		    __RV_CSR_WRITE(0xbe0 + 2*(id - 5), base_addr | 0x5);  \
	    }	\
	}

#define DEFINE_NOCACHE_REGION_DISABLE(id)               \
    void non_cacheable_region_disable_##id()             \
    {                                               \
    	if(id < 2)	\
			__RV_CSR_WRITE(0x7f3 + 2*id, 0x0);	\
		else if (id < 5) \
			__RV_CSR_WRITE(0x7f9 + 2*(id - 2), 0x0);	\
		else	\
			__RV_CSR_WRITE(0xbe0 + 2*(id - 5), 0x0);	\
    }

DEFINE_NOCACHE_REGION_ENABLE(0);
DEFINE_NOCACHE_REGION_ENABLE(1);
DEFINE_NOCACHE_REGION_ENABLE(2);
DEFINE_NOCACHE_REGION_ENABLE(3);
DEFINE_NOCACHE_REGION_ENABLE(4);
DEFINE_NOCACHE_REGION_ENABLE(5);
DEFINE_NOCACHE_REGION_ENABLE(6);
DEFINE_NOCACHE_REGION_ENABLE(7);

DEFINE_NOCACHE_REGION_DISABLE(0);
DEFINE_NOCACHE_REGION_DISABLE(1);
DEFINE_NOCACHE_REGION_DISABLE(2);
DEFINE_NOCACHE_REGION_DISABLE(3);
DEFINE_NOCACHE_REGION_DISABLE(4);
DEFINE_NOCACHE_REGION_DISABLE(5);
DEFINE_NOCACHE_REGION_DISABLE(6);
DEFINE_NOCACHE_REGION_DISABLE(7);

/**
 * \brief Start Core-1
 * \details
 * This function is used to start Core-1 by setting the reset address
 * and triggering the reset sequence for Core-1.
 * \param [in]  start_address  The start address for Core-1 execution
 */
void start_core1(uint32_t start_address)
{
    IP_CMN_SYSCFG->REG_SW_RESET_CFG1.all = 0xCAFE000A;
    IP_CMN_SYSCFG->REG_N300_CORE1_RST_ADDR.all = start_address;
    __RWMB();
    IP_CMN_SYSCFG->REG_SW_RESET_CORE1.all = 0xCAFE000A;
    __RWMB();
    IP_CMN_SYSCFG->REG_SW_RESET_CFG1.all = 0xCAFE000B;
}


extern void default_intexc_handler(void);

// Function template of all interrupt handlers
#define DEFINE_INTERRUPT_HANDLER(irq)               \
    void Interrupt##irq##_Handler(void)             \
    {                                               \
        if (OS_CPU_Vector_Table[irq] != NULL) {     \
            ((ISR)(OS_CPU_Vector_Table[irq]))();    \
        } else {                                    \
            default_intexc_handler();               \
        }                                           \
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

/** @} */ /* End of Doxygen Group NMSIS_Core_SystemAndClock */
