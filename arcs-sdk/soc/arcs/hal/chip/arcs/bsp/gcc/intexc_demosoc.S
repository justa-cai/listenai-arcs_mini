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
 * \file     intexc_demosoc.S
 * \brief    NMSIS Interrupt and Exception Handling Template File
 *  for Nuclei Demo SoC which support Nuclei N/NX class cores
 * \version  V1.00
 * \date     17 Dec 2019
 *
 ******************************************************************************/

#include "riscv_encoding.h"

/**
 * \brief  Global interrupt disabled
 * \details
 *  This function disable global interrupt.
 * \remarks
 *  - All the interrupt requests will be ignored by CPU.
 */
.macro DISABLE_MIE
    csrc CSR_MSTATUS, MSTATUS_MIE
.endm

/**
 * \brief  Macro for context save
 * \details
 * This macro save ABI defined caller saved registers in the stack.
 * \remarks
 * - This Macro could use to save context when you enter to interrupt
 * or exception
*/
/* Save caller registers */
.macro SAVE_CONTEXT
    /* Allocate stack space for context saving */
#ifndef __riscv_32e
    addi sp, sp, -20*REGBYTES
#else
    addi sp, sp, -14*REGBYTES
#endif /* __riscv_32e */

    STORE x1, 0*REGBYTES(sp)
    STORE x4, 1*REGBYTES(sp)
    STORE x5, 2*REGBYTES(sp)
    STORE x6, 3*REGBYTES(sp)
    STORE x7, 4*REGBYTES(sp)
    STORE x10, 5*REGBYTES(sp)
    STORE x11, 6*REGBYTES(sp)
    STORE x12, 7*REGBYTES(sp)
    STORE x13, 8*REGBYTES(sp)
    STORE x14, 9*REGBYTES(sp)
    STORE x15, 10*REGBYTES(sp)
#ifndef __riscv_32e
    STORE x16, 14*REGBYTES(sp)
    STORE x17, 15*REGBYTES(sp)
    STORE x28, 16*REGBYTES(sp)
    STORE x29, 17*REGBYTES(sp)
    STORE x30, 18*REGBYTES(sp)
    STORE x31, 19*REGBYTES(sp)
#endif /* __riscv_32e */
.endm

/**
 * \brief  Macro for restore caller registers
 * \details
 * This macro restore ABI defined caller saved registers from stack.
 * \remarks
 * - You could use this macro to restore context before you want return
 * from interrupt or exeception
 */
/* Restore caller registers */
.macro RESTORE_CONTEXT
    LOAD x1, 0*REGBYTES(sp)
    LOAD x4, 1*REGBYTES(sp)
    LOAD x5, 2*REGBYTES(sp)
    LOAD x6, 3*REGBYTES(sp)
    LOAD x7, 4*REGBYTES(sp)
    LOAD x10, 5*REGBYTES(sp)
    LOAD x11, 6*REGBYTES(sp)
    LOAD x12, 7*REGBYTES(sp)
    LOAD x13, 8*REGBYTES(sp)
    LOAD x14, 9*REGBYTES(sp)
    LOAD x15, 10*REGBYTES(sp)
#ifndef __riscv_32e
    LOAD x16, 14*REGBYTES(sp)
    LOAD x17, 15*REGBYTES(sp)
    LOAD x28, 16*REGBYTES(sp)
    LOAD x29, 17*REGBYTES(sp)
    LOAD x30, 18*REGBYTES(sp)
    LOAD x31, 19*REGBYTES(sp)

    /* De-allocate the stack space */
    addi sp, sp, 20*REGBYTES
#else
    /* De-allocate the stack space */
    addi sp, sp, 14*REGBYTES
#endif /* __riscv_32e */

.endm

/**
 * \brief  Macro for save necessary CSRs to stack
 * \details
 * This macro store MCAUSE, MEPC, MSUBM to stack.
 */
.macro SAVE_CSR_CONTEXT
    /* Store CSR mcause to stack using pushmcause */
    csrrwi  x0, CSR_PUSHMCAUSE, 11
    /* Store CSR mepc to stack using pushmepc */
    csrrwi  x0, CSR_PUSHMEPC, 12
    /* Store CSR msub to stack using pushmsub */
    csrrwi  x0, CSR_PUSHMSUBM, 13
.endm

/**
 * \brief  Macro for restore necessary CSRs from stack
 * \details
 * This macro restore MSUBM, MEPC, MCAUSE from stack.
 */
.macro RESTORE_CSR_CONTEXT
    LOAD x5,  13*REGBYTES(sp)
    csrw CSR_MSUBM, x5
    LOAD x5,  12*REGBYTES(sp)
    csrw CSR_MEPC, x5
    LOAD x5,  11*REGBYTES(sp)
    csrw CSR_MCAUSE, x5
.endm

/**
 * \brief  Exception/NMI Entry
 * \details
 * This function provide common entry functions for exception/nmi.
 * \remarks
 * This function provide a default exception/nmi entry.
 * ABI defined caller save register and some CSR registers
 * to be saved before enter interrupt handler and be restored before return.
 */
.section .text.trap
/* In CLIC mode, the exeception entry must be 64bytes aligned */
.align 6
.global exc_entry
.weak exc_entry
exc_entry:
    /* Save the caller saving registers (context) */
    SAVE_CONTEXT
    /* Save the necessary CSR registers */
    SAVE_CSR_CONTEXT

    /*
     * Set the exception handler function arguments
     * argument 1: mcause value
     * argument 2: current stack point(SP) value
     */
    csrr a0, mcause
    mv a1, sp
    /*
     * TODO: Call the exception handler function
     * By default, the function template is provided in
     * system_Device.c, you can adjust it as you want
     */
    call core_exception_handler

    /* Restore the necessary CSR registers */
    RESTORE_CSR_CONTEXT
    /* Restore the caller saving registers (context) */
    RESTORE_CONTEXT

    /* Return to regular code */
    mret

/**
 * \brief  Non-Vector Interrupt Entry
 * \details
 * This function provide common entry functions for handling
 * non-vector interrupts
 * \remarks
 * This function provide a default non-vector interrupt entry.
 * ABI defined caller save register and some CSR registers need
 * to be saved before enter interrupt handler and be restored before return.
 */
.section      .text.irq
/* In CLIC mode, the interrupt entry must be 4bytes aligned */
.align 2
.global irq_entry
.weak irq_entry
/* This label will be set to MTVT2 register */
irq_entry:
    /* Save the floating point registers */
#if defined(__riscv_flen)
    addi sp, sp, -(2 * REGBYTES)
    STORE t0, 0 * REGBYTES(sp)
    STORE t1, 1 * REGBYTES(sp)
    csrr t0, CSR_MSTATUS
    li t1, MSTATUS_FS_DIRTY
    and t0,t0,t1
#if defined(__riscv_vector)
    bne t0, t1, __irq_save_vp_stack
#else
    bne t0, t1, __irq_save_integer_stack
#endif
    LOAD t0, 0 * REGBYTES(sp)
    LOAD t1, 1 * REGBYTES(sp)
    addi sp, sp, (2 * REGBYTES)

    /* Save fp registers and fcsr */
    addi sp, sp, -(5 * REGBYTES)
    STORE t0,   0 * REGBYTES(sp)
    csrr  t0, CSR_FCSR
    STORE t0,   1 * REGBYTES(sp)
    LOAD t0, 0 * REGBYTES(sp)
    addi sp, sp, (1 * REGBYTES)
    addi sp, sp, -(32 * FPREGBYTES)
    FPSTORE f0 , 0  * FPREGBYTES(sp)
    FPSTORE f1 , 1  * FPREGBYTES(sp)
    FPSTORE f2 , 2  * FPREGBYTES(sp)
    FPSTORE f3 , 3  * FPREGBYTES(sp)
    FPSTORE f4 , 4  * FPREGBYTES(sp)
    FPSTORE f5 , 5  * FPREGBYTES(sp)
    FPSTORE f6 , 6  * FPREGBYTES(sp)
    FPSTORE f7 , 7  * FPREGBYTES(sp)
    FPSTORE f8 , 8  * FPREGBYTES(sp)
    FPSTORE f9 , 9  * FPREGBYTES(sp)
    FPSTORE f10, 10 * FPREGBYTES(sp)
    FPSTORE f11, 11 * FPREGBYTES(sp)
    FPSTORE f12, 12 * FPREGBYTES(sp)
    FPSTORE f13, 13 * FPREGBYTES(sp)
    FPSTORE f14, 14 * FPREGBYTES(sp)
    FPSTORE f15, 15 * FPREGBYTES(sp)
    FPSTORE f16, 16 * FPREGBYTES(sp)
    FPSTORE f17, 17 * FPREGBYTES(sp)
    FPSTORE f18, 18 * FPREGBYTES(sp)
    FPSTORE f19, 19 * FPREGBYTES(sp)
    FPSTORE f20, 20 * FPREGBYTES(sp)
    FPSTORE f21, 21 * FPREGBYTES(sp)
    FPSTORE f22, 22 * FPREGBYTES(sp)
    FPSTORE f23, 23 * FPREGBYTES(sp)
    FPSTORE f24, 24 * FPREGBYTES(sp)
    FPSTORE f25, 25 * FPREGBYTES(sp)
    FPSTORE f26, 26 * FPREGBYTES(sp)
    FPSTORE f27, 27 * FPREGBYTES(sp)
    FPSTORE f28, 28 * FPREGBYTES(sp)
    FPSTORE f29, 29 * FPREGBYTES(sp)
    FPSTORE f30, 30 * FPREGBYTES(sp)
    FPSTORE f31, 31 * FPREGBYTES(sp)
#endif
#if defined(__riscv_vector)
    addi sp, sp, -(2 * REGBYTES)
    STORE t0, 0 * REGBYTES(sp)
    STORE t1, 1 * REGBYTES(sp)
__irq_save_vp_stack:
    csrr t0, CSR_MSTATUS
    li t1, MSTATUS_VS_DIRTY
    and t0,t0,t1
    bne t0, t1, _save_integer_stack
    LOAD t0, 0 * REGBYTES(sp)
    LOAD t1, 1 * REGBYTES(sp)
    addi sp, sp, (2 * REGBYTES)

    /* Save vector registers and vtype/vl/vstart/vcsr */
    addi sp, sp, -(6 * REGBYTES)
    STORE t0,   0 * REGBYTES(sp)
    STORE t1,   1 * REGBYTES(sp)
    csrr  t0, CSR_VSTART
    STORE t0,   2 * REGBYTES(sp)
    csrr  t0, CSR_VTYPE
    STORE t0,   3 * REGBYTES(sp)
    csrr  t0, CSR_VL
    STORE t0,   4 * REGBYTES(sp)
    csrr  t0, CSR_VCSR
    STORE t0,   5 * REGBYTES(sp)

    mv t1, sp
    csrr  t0, CSR_VLENB
    slli t0, t0, 5
    /* t1 is the new temp stack where t0, t1 saved */
    sub t1, t1, t0
    LOAD t0, 0 * REGBYTES(sp)
    STORE t0,   0 * REGBYTES(t1)
    LOAD t0, 1 * REGBYTES(sp)
    STORE t0,   1 * REGBYTES(t1)
    mv sp, t1
    /* new sp -> t1 to save vector registers */
    addi t1, t1, (2 * REGBYTES)
    vsetvli t0, x0, e8, m8, ta, ma
    vse8.v v0, (t1)
    add t1, t1, t0
    vse8.v v8, (t1)
    add t1, t1, t0
    vse8.v v16, (t1)
    add t1, t1, t0
    vse8.v v24, (t1)
    LOAD t0, 0 * REGBYTES(sp)
    LOAD t1, 1 * REGBYTES(sp)
    addi sp, sp, (2 * REGBYTES)
#endif
#if defined(__riscv_flen) || defined(__riscv_vector)
    addi sp, sp, -(2 * REGBYTES)
    STORE t0, 0 * REGBYTES(sp)
    STORE t1, 1 * REGBYTES(sp)
__irq_save_integer_stack:
    LOAD t0, 0 * REGBYTES(sp)
    LOAD t1, 1 * REGBYTES(sp)
    addi sp, sp, (2 * REGBYTES)
#endif
    /* Save the caller saving registers (context) */
    SAVE_CONTEXT
    /* Save the necessary CSR registers */
    SAVE_CSR_CONTEXT

#if defined(__riscv_flen) || defined(__riscv_vector)
    addi sp, sp, -(5 * REGBYTES)
    STORE t0, 0 * REGBYTES(sp)
    csrr t0, CSR_MSTATUS
    STORE t0, 1 * REGBYTES(sp)
    LOAD t0,   0 * REGBYTES(sp)
    addi sp, sp, (1 * REGBYTES)
#endif

    /* This special CSR read/write operation, which is actually
     * claim the CLIC to find its pending highest ID, if the ID
     * is not 0, then automatically enable the mstatus.MIE, and
     * jump to its vector-entry-label, and update the link register
     */
    csrrw ra, CSR_JALMNXTI, ra

    /* Critical section with interrupts disabled */
    DISABLE_MIE

#if defined(__riscv_flen) || defined(__riscv_vector)
    /* Restore mstatus */
    addi sp, sp, -(1 * REGBYTES)
    STORE t0, 0 * REGBYTES(sp)
    LOAD t0, 1 * REGBYTES(sp)
    csrw CSR_MSTATUS, t0
    LOAD t0, 0 * REGBYTES(sp)
    addi sp, sp, (5 * REGBYTES)
#endif

    /* Restore the necessary CSR registers */
    RESTORE_CSR_CONTEXT
    /* Restore the caller saving registers (context) */
    RESTORE_CONTEXT
    /* Restore the floating point registers */
#if defined(__riscv_vector)
    /* Pop vector stack if needed */
    addi sp, sp, -(2 * REGBYTES)
    STORE t0, 0 * REGBYTES(sp)
    STORE t1, 1 * REGBYTES(sp)
    csrr t0, CSR_MSTATUS
    li t1, MSTATUS_VS_DIRTY
    and t0,t0,t1
#if defined(__riscv_flen)
    bne t0, t1, __irq_restore_fp_stack
#else
    bne t0, t1, __irq_restore_done
#endif
    LOAD t0, 0 * REGBYTES(sp)
    LOAD t1, 1 * REGBYTES(sp)
    addi sp, sp, (2 * REGBYTES)

    /* pop vector registers and vtype/vl/vstart/vcsr */
    addi sp, sp, -(3 * REGBYTES)
    STORE t0,   0 * REGBYTES(sp)
    STORE t1,   1 * REGBYTES(sp)
    STORE t2,   2 * REGBYTES(sp)
    mv t2, sp
    /* temp regs are stored in t2 stack top */
    addi sp, sp, (3 * REGBYTES)
    vsetvli t0, x0, e8, m8, ta, ma
    vle8.v v0, (sp)
    add sp, sp, t0
    vle8.v v8, (sp)
    add sp, sp, t0
    vle8.v v16, (sp)
    add sp, sp, t0
    vle8.v v24, (sp)
    add sp, sp, t0

    LOAD t0,   2 * REGBYTES(sp)
    LOAD t1,   1 * REGBYTES(sp)
    vsetvl x0, t0, t1
    LOAD t0,   0 * REGBYTES(sp)
    csrw  CSR_VSTART, t0
    LOAD t0,   3 * REGBYTES(sp)
    csrw  CSR_VCSR, t0
    addi sp, sp, (4 * REGBYTES)

    LOAD t0,   0 * REGBYTES(t2)
    LOAD t1,   1 * REGBYTES(t2)
    LOAD t2,   2 * REGBYTES(t2)
#endif

#if defined(__riscv_flen)
    addi sp, sp, -(2 * REGBYTES)
    STORE t0, 0 * REGBYTES(sp)
    STORE t1, 1 * REGBYTES(sp)
__irq_restore_fp_stack:
    csrr t0, CSR_MSTATUS
    li t1, MSTATUS_FS_DIRTY
    and t0,t0,t1
    bne t0, t1, __irq_restore_done
    LOAD t0, 0 * REGBYTES(sp)
    LOAD t1, 1 * REGBYTES(sp)
    addi sp, sp, (2 * REGBYTES)

    /* Save fp registers and fcsr */
    FPLOAD f0 , 0  * FPREGBYTES(sp)
    FPLOAD f1 , 1  * FPREGBYTES(sp)
    FPLOAD f2 , 2  * FPREGBYTES(sp)
    FPLOAD f3 , 3  * FPREGBYTES(sp)
    FPLOAD f4 , 4  * FPREGBYTES(sp)
    FPLOAD f5 , 5  * FPREGBYTES(sp)
    FPLOAD f6 , 6  * FPREGBYTES(sp)
    FPLOAD f7 , 7  * FPREGBYTES(sp)
    FPLOAD f8 , 8  * FPREGBYTES(sp)
    FPLOAD f9 , 9  * FPREGBYTES(sp)
    FPLOAD f10, 10 * FPREGBYTES(sp)
    FPLOAD f11, 11 * FPREGBYTES(sp)
    FPLOAD f12, 12 * FPREGBYTES(sp)
    FPLOAD f13, 13 * FPREGBYTES(sp)
    FPLOAD f14, 14 * FPREGBYTES(sp)
    FPLOAD f15, 15 * FPREGBYTES(sp)
    FPLOAD f16, 16 * FPREGBYTES(sp)
    FPLOAD f17, 17 * FPREGBYTES(sp)
    FPLOAD f18, 18 * FPREGBYTES(sp)
    FPLOAD f19, 19 * FPREGBYTES(sp)
    FPLOAD f20, 20 * FPREGBYTES(sp)
    FPLOAD f21, 21 * FPREGBYTES(sp)
    FPLOAD f22, 22 * FPREGBYTES(sp)
    FPLOAD f23, 23 * FPREGBYTES(sp)
    FPLOAD f24, 24 * FPREGBYTES(sp)
    FPLOAD f25, 25 * FPREGBYTES(sp)
    FPLOAD f26, 26 * FPREGBYTES(sp)
    FPLOAD f27, 27 * FPREGBYTES(sp)
    FPLOAD f28, 28 * FPREGBYTES(sp)
    FPLOAD f29, 29 * FPREGBYTES(sp)
    FPLOAD f30, 30 * FPREGBYTES(sp)
    FPLOAD f31, 31 * FPREGBYTES(sp)
    addi sp, sp, (32 * FPREGBYTES)

    addi sp, sp, -(1 * REGBYTES)
    STORE t0,   0 * REGBYTES(sp)
    LOAD  t0,   1 * REGBYTES(sp)
    csrw  CSR_FCSR, t0
    LOAD t0,   0 * REGBYTES(sp)
    addi sp, sp, (5 * REGBYTES)
#endif

#if defined(__riscv_flen) || defined(__riscv_vector)
    addi sp, sp, -(2 * REGBYTES)
    STORE t0, 0 * REGBYTES(sp)
    STORE t1, 1 * REGBYTES(sp)
__irq_restore_done:
    LOAD t0, 0 * REGBYTES(sp)
    LOAD t1, 1 * REGBYTES(sp)
    addi sp, sp, (2 * REGBYTES)
#endif

    /* Return to regular code */
    mret

/* Default Handler for Exceptions / Interrupts */
.global default_intexc_handler
.weak default_intexc_handler
Undef_Handler:
default_intexc_handler:
1:
    j 1b
