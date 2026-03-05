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
#include "log_print.h"
#include "ClockManager.h"
#if CFG_MEMDUMP
#include "memdump.h"
#endif

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

__attribute__((aligned (512))) _FAST_DATA_ZI void* OS_CPU_Vector_Table[IRQ_MAX] = { 0 };

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

__attribute__((weak)) uint32_t __copy_table_start__ = 0, __copy_table_end__ = 0;
__attribute__((weak)) uint32_t __zero_table_start__ = 0, __zero_table_end__ = 0;

void SystemInit_Copy(void){
    uint32_t *pSrc, *pDst, *pEnd;
    uint32_t *pCpyItem, *pCpyEnd;

    // copy all regions of copy table from LMA to VMA if necessary
    pCpyItem = &__copy_table_start__;
    pCpyEnd = &__copy_table_end__;
    if (pCpyItem && pCpyEnd) {
    
        while (pCpyItem < pCpyEnd) {
            // retrieve one group of LMA, VMA_START, and VMA_END, and do copy operation
            pSrc = (uint32_t*)(*pCpyItem++);
            pDst = (uint32_t*)(*pCpyItem++);
            pEnd = (uint32_t*)(*pCpyItem++);
            if (pDst != pSrc) {
                for (;pDst < pEnd;)
                    *pDst++ = *pSrc++;
            }
        }
    }

    pCpyItem = &__zero_table_start__;
    pCpyEnd = &__zero_table_end__;

    if (pCpyItem && pCpyEnd) {
        while (pCpyItem < pCpyEnd) {
            // retrieve one group of LMA, VMA_START, and VMA_END, and do copy operation
            pSrc = (uint32_t*)(*pCpyItem++);
            pEnd = (uint32_t*)(*pCpyItem++);
            for (; pSrc < pEnd;)
                *pSrc++ = 0;
        }
    }
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
    SystemInit_Copy();
    /* ToDo: add code to initialize the system
     * Warn: do not use global variables because this function is called before
     * reaching pre-main. RW section maybe overwritten afterwards.
     */
    SystemCoreClock = SYSTEM_CLOCK;

    __HAL_CRM_MTIME_CLK_ENABLE();

    // Config extern buck 
#ifdef  CONFIG_EXTERN_BUCK_ENABLE
    IP_AON_CTRL->REG_AON_TUNE1.bit.TUNE_LDOCORE = 0x38;         //tune ldocore to be 0.8V
    IP_AON_CTRL->REG_EFU_LOAD_REG.bit.LDEFU_TUNE_LDOCORE = 0x1; // load tune_ldocore code from reg not efuse
#endif

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
    unsigned long mstatus  = __RV_CSR_READ(CSR_MSTATUS);
    unsigned long mstratch = __RV_CSR_READ(CSR_MSCRATCH);

    /* TODO: Uncomment this if you have implement printf function */
    CLOGD("MCAUSE : 0x%08x", mcause);
    CLOGD("MDCAUSE: 0x%08x", __RV_CSR_READ(CSR_MDCAUSE));
    CLOGD("MEPC   : 0x%08x", __RV_CSR_READ(CSR_MEPC));
    CLOGD("MTVAL  : 0x%08x", __RV_CSR_READ(CSR_MTVAL));
    CLOGD("MSTATUS: 0x%08x", mstatus);
    CLOGD("HARTID : %u\r\n", (__RV_CSR_READ(CSR_MHARTID) & 0xFF));
    Exception_DumpFrame(sp, PRV_M, mstatus, mstratch);

#if defined(SIMULATION_MODE)
    // directly exit if in SIMULATION
    extern void simulation_exit(int status);
    simulation_exit(1);
#else
#if CFG_MEMDUMP
    memdump_process(MDUMP_PATH_FLASH);
#else
    while(1);
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

/**
 * \brief      Dump Exception Frame
 * \details
 * This function provided feature to dump exception frame stored in stack.
 * \param [in]  sp    stackpoint
 * \param [in]  mode  privileged mode to decide whether to dump msubm CSR
 */
void Exception_DumpFrame(unsigned long sp, uint8_t mode, unsigned long mstatus, unsigned long mstratch)
{
    EXC_Frame_Type *exc_frame = (EXC_Frame_Type *)sp;

    sp += sizeof(EXC_Frame_Type);
#ifndef __riscv_32e
    CLOGD("sp: 0x%08x ra: 0x%08x cause: 0x%08x epc: 0x%08x\ntp: 0x%08x t0: 0x%08x t1: 0x%08x t2: 0x%08x\nt3: 0x%08x t4: 0x%08x t5: 0x%08x t6: 0x%08x\n" \
           "a0: 0x%08x a1: 0x%08x a2: 0x%08x a3: 0x%08x\na4: 0x%08x a5: 0x%08x a6: 0x%08x a7: 0x%08x", \
           sp, exc_frame->ra, exc_frame->cause, exc_frame->epc, exc_frame->tp, exc_frame->t0, \
           exc_frame->t1, exc_frame->t2, exc_frame->t3, exc_frame->t4, exc_frame->t5, exc_frame->t6, \
           exc_frame->a0, exc_frame->a1, exc_frame->a2, exc_frame->a3, exc_frame->a4, exc_frame->a5, \
           exc_frame->a6, exc_frame->a7);
#else
    CLOGD("ra: 0x%08x, tp: 0x%08x, t0: 0x%08x, t1: 0x%08x, t2: 0x%08x\n" \
           "a0: 0x%08x, a1: 0x%08x, a2: 0x%08x, a3: 0x%08x, a4: 0x%08x, a5: 0x%08x\n" \
           "cause: 0x%08x, epc: 0x%08x\n", exc_frame->ra, exc_frame->tp, exc_frame->t0, \
           exc_frame->t1, exc_frame->t2, exc_frame->a0, exc_frame->a1, exc_frame->a2, exc_frame->a3, \
           exc_frame->a4, exc_frame->a5, exc_frame->cause, exc_frame->epc);
#endif

    if (PRV_M == mode) {
        /* msubm is exclusive to machine mode */
        CLOGD("msubm: 0x%08x\n", exc_frame->msubm);
    }
#ifdef CFG_BACK_TRACE
    extern void rv_backtrace_fault(uint32_t sp, EXC_Frame_Type *frame, uint32_t mstatus, uint32_t mstratch);
    rv_backtrace_fault(sp, exc_frame, mstatus, mstratch);
#endif
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
static void _get_iregion_info(IRegion_Info_Type *iregion)
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
 * \brief do the init for memory protection entry, total 16 entries.
 * \details
 * This function provide initialization physical memory protection.
 * Remove X permission of protected_execute region: PMP_L | PMP_R | PMP_W
 * Remove R permission of protected_data region: PMP_L | PMP_W
 * Remove W permission of protected_data region: PMP_L | PMP_R
 */
static void PMP_Init(void)
{
    unsigned long hartid = __RV_CSR_READ(CSR_MHARTID) & 0xFF;

    if(!hartid) {
        pmp_config pmp_config_region[] = {
            /*
             * Locked PMP entries remain locked until the hart is reset,
             * the L bit also indicates whether the R/W/X permissions are enforced on M-mode accesses
             */
    
            /* PMP entry 0, [0x00080000 - 0x00084000] */
            { // set AP ILM (ITCM) writable for Interrupt Vector Table
                .protection = PMP_L | PMP_R | PMP_X | PMP_W,
                .order = 14,
                .base_addr = 0x80000,
            },
            /* PMP entry 1, [0x00100000 - 0x00102000] */
            {
                .protection = PMP_L | PMP_R | PMP_W,
                .order = 13,
                .base_addr = 0x100000,
            },
            /* PMP entry 2, [0x08000000 - 0x10000000] */
            {
                .protection = PMP_L | PMP_R | PMP_W | PMP_X,
                .order = 27,
                .base_addr = 0x8000000,
            },
            /* PMP entry 3, [0x10000000 - 0x20000000] */
            {
                .protection = PMP_L | PMP_R | PMP_W | PMP_X,
                .order = 28,
                .base_addr = 0x10000000,
            },
            /* PMP entry 4, [0x20000000 - 0x40000000] */
            {
                .protection = PMP_L | PMP_R | PMP_W | PMP_X,
                .order = 29,
                .base_addr = 0x20000000,
            },
            /* PMP entry 5, [0x40000000 - 0x80000000] */
            {
                .protection = PMP_L | PMP_R | PMP_W,
                .order = 30,
                .base_addr = 0x40000000,
            },
            /* PMP entry 6, [0x80000000 - 0x100000000] */
            {
                .protection = PMP_L | PMP_R | PMP_W,
                .order = 31,
                .base_addr = 0x80000000,
            },
            /* PMP entry 7, [0x00000000 - 0x100000000] */
            {
                .protection = PMP_L,
                .order = 32,
                .base_addr = 0x0,
            },
        };
        for(int i = 0; i < sizeof(pmp_config_region)/sizeof(pmp_config); i++)
        __set_PMPENTRYx(i, &pmp_config_region[i]);    
    } else {
        pmp_config pmp_config_region[] = {
            /*
             * Locked PMP entries remain locked until the hart is reset,
             * the L bit also indicates whether the R/W/X permissions are enforced on M-mode accesses
             */
    
            /* PMP entry 0, [0x00200000 - 0x00300000] */
            {    // set CP ILM (ITCM) writable for Interrupt Vector Table
                .protection = PMP_L | PMP_R | PMP_X | PMP_W,
                .order = 20,
                .base_addr = 0x200000,
            },
            /* PMP entry 1, [0x00200000 - 0x00400000] */
            {
                .protection = PMP_L | PMP_R | PMP_W,
                .order = 21,
                .base_addr = 0x200000,
            },
            /* PMP entry 2, [0x08000000 - 0x10000000] */
            {
                .protection = PMP_L | PMP_R | PMP_W | PMP_X,
                .order = 27,
                .base_addr = 0x8000000,
            },
            /* PMP entry 3, [0x10000000 - 0x20000000] */
            {
                .protection = PMP_L | PMP_R | PMP_W | PMP_X,
                .order = 28,
                .base_addr = 0x10000000,
            },
            /* PMP entry 4, [0x20000000 - 0x40000000] */
            {
                .protection = PMP_L | PMP_R | PMP_W | PMP_X,
                .order = 29,
                .base_addr = 0x20000000,
            },
            /* PMP entry 5, [0x40000000 - 0x80000000] */
            {
                .protection = PMP_L | PMP_R | PMP_W,
                .order = 30,
                .base_addr = 0x40000000,
            },
            /* PMP entry 6, [0x80000000 - 0x100000000] */
            {
                .protection = PMP_L | PMP_R | PMP_W,
                .order = 31,
                .base_addr = 0x80000000,
            },
            /* PMP entry 7, [0x00000000 - 0x100000000] */
            {
                .protection = PMP_L,
                .order = 32,
                .base_addr = 0x0,
            },
        };
        for(int i = 0; i < sizeof(pmp_config_region)/sizeof(pmp_config); i++)
        __set_PMPENTRYx(i, &pmp_config_region[i]);
    }

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
    // TODO to make it possible for configurable boot hartid
    unsigned long hartid = __RV_CSR_READ(CSR_MHARTID) & 0xFF;

    // BOOT_HARTID is defined <Device.h>
    if (hartid == BOOT_HARTID) { // only done in boot hart
        // IREGION INFO MUST BE SET BEFORE ANY PREMAIN INIT STEPS
        _get_iregion_info((IRegion_Info_Type *)(&SystemIRegionInfo));
    }

#if defined(__PMP_PRESENT)
    PMP_Init();
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

    non_cacheable_region_enable(WIFI_RAM_REGION, (CMN_PSRAM_REGION - WIFI_RAM_REGION));

    /* Do fence and fence.i to make sure previous ilm/dlm/icache/dcache control done */
    __RWMB();
    __FENCE_I();


    if (hartid == BOOT_HARTID) { // only required for boot hartid
		extern void BootClock_Init();
		BootClock_Init();
        /* Initialize exception default handlers */
        Exception_Init();
        /* ECLIC initialization, mainly MTH and NLBIT */
        ECLIC_Init();
    } else {
        /* for the other harts */
        __RV_CSR_WRITE(CSR_MTVT, OS_CPU_Vector_Table);
        ECLIC_Init();
    }

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
//    extern void simulation_exit(int status);
//    simulation_exit(status);
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

extern void default_intexc_handler(void);

//In order to improve the performance and reduce the gate count,
//the alignment of the base address in MTVT is determined by
//the actual number of interrupts, which is shown in the following table:
//  Max IRQ #   MTVT alignment
//  0 ~ 16      64-byte
//  17 ~ 32     128-byte
//  33 ~ 64     256-byte
//  65 ~ 128    512-byte
//  129 ~ 256   1KB
//  257 ~ 512   2KB
//  ......
// Set MTVT register to start address of OS_CPU_Vector_Table array. = { 0 }
void irq_vectors_init(void)
{
    extern uint32_t _start;

    //when original vtable is copied to ILM or SRAM as fast vtable,
    //the old entry 0# "j _start" may failed, because "j" or "jal" can cover about 1MB range
    //starting from current PC and the address of _start may be out of 1MB range!
    OS_CPU_Vector_Table[0] = &_start;

    for (int i = 1; i < IRQ_MAX; i++)
        OS_CPU_Vector_Table[i] = &default_intexc_handler;

#ifdef CFG_RTOS
    OS_CPU_Vector_Table[SysTimerSW_IRQn] = &eclic_msip_handler;
#endif

    OS_CPU_Vector_Table[SysTimer_IRQn] = &eclic_mtip_handler;

    // set my own interrupt vector table
    __RV_CSR_WRITE(CSR_MTVT, OS_CPU_Vector_Table);
}
#if CONFIG_PM
void irq_vectors_reinit(void)
{
    for (int i = 1; i < IRQ_MAX; i++)
    {
        if (OS_CPU_Vector_Table[i] != default_intexc_handler)
            enable_IRQ(i);
    }
    __RV_CSR_WRITE(CSR_MTVT, OS_CPU_Vector_Table);
}
#endif
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

void device_region_enable(uint32_t base_addr, uint32_t len){

    if (base_addr % len){
        return;
    }

    uint32_t mnocm = ~(len - 1);

    // Set cache region mask
    __RV_CSR_WRITE(CSR_MDEVM, mnocm);
    // Set base physical address and enable
    __RV_CSR_WRITE(CSR_MDEVB, base_addr | 0x1);
}

void device_region_disable(void){
    // disable cache region
    __RV_CSR_WRITE(CSR_MDEVB, 0x0);
}

void non_cacheable_region_disable(void){
    // disable cache region
    __RV_CSR_WRITE(CSR_MNOCB, 0x0);
}
/*
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
*/
/** @} */ /* End of Doxygen Group NMSIS_Core_SystemAndClock */

#define REBOOT_PASS_PIN 0xCAFE000A

/* API for software reboot/full_reset */
void sys_platform_sw_full_reset(void)
{
    IP_AON_CTRL->REG_AON_SW_RESET.all = REBOOT_PASS_PIN;
}

