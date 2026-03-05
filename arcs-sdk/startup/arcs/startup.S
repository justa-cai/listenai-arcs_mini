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
 * \file     startup_demosoc.S
 * \brief    NMSIS Nuclei N/NX Class Core based Core Device Startup File for
 *  Nuclei Demo SoC which support Nuclei N/NX class cores
 * \version  V1.00
 * \date     17. Dec 2019
 *
 ******************************************************************************/

#include "riscv_encoding.h"

/* If BOOT_HARTID is not defined, default value is 0 */
#ifndef BOOT_HARTID
    .equ BOOT_HARTID,    0
#endif

.macro DECLARE_INT_HANDLER  INT_HDL_NAME
#if defined(__riscv_xlen) && (__riscv_xlen == 32)
    .word \INT_HDL_NAME
#else
    .dword \INT_HDL_NAME
#endif
.endm

    .section .vtable

    .globl vector_base
    .type vector_base, @object
vector_base:
#ifndef VECTOR_TABLE_REMAPPED
    j _start                                             /* 0: Reserved, Jump to _start when reset for vector table not remapped cases.*/
    .align LOG_REGBYTES                                  /*    Need to align 4 byte for RV32, 8 Byte for RV64 */
#else
    DECLARE_INT_HANDLER     Interrupt0_Handler           /* 0: Reserved, default handler for vector table remapped cases */
#endif
    DECLARE_INT_HANDLER     Interrupt1_Handler           /* 1: Reserved */
    DECLARE_INT_HANDLER     Interrupt2_Handler           /* 2: Reserved */
#ifndef CFG_RTOS
    DECLARE_INT_HANDLER     Interrupt3_Handler           /* 3: Machine software interrupt */
#else
    DECLARE_INT_HANDLER     eclic_msip_handler           /* 3: Machine software interrupt, vector mode for FreeRTOS implementation */
#endif        
    DECLARE_INT_HANDLER     Interrupt4_Handler           /* 4: Reserved */
    DECLARE_INT_HANDLER     Interrupt5_Handler           /* 5: Reserved */
    DECLARE_INT_HANDLER     Interrupt6_Handler           /* 6: Reserved */
    DECLARE_INT_HANDLER     Interrupt7_Handler           /* 7: Machine timer interrupt */
    DECLARE_INT_HANDLER     Interrupt8_Handler           /* 8: Reserved */
    DECLARE_INT_HANDLER     Interrupt9_Handler           /* 9: Reserved */
    DECLARE_INT_HANDLER     Interrupt10_Handler          /* 10: Reserved */
    DECLARE_INT_HANDLER     Interrupt11_Handler          /* 11: Reserved */
    DECLARE_INT_HANDLER     Interrupt12_Handler          /* 12: Reserved */
    DECLARE_INT_HANDLER     Interrupt13_Handler          /* 13: Reserved */
    DECLARE_INT_HANDLER     Interrupt14_Handler          /* 14: Reserved */
    DECLARE_INT_HANDLER     Interrupt15_Handler          /* 15: Reserved */
    DECLARE_INT_HANDLER     Interrupt16_Handler          /* 16: Reserved */
    DECLARE_INT_HANDLER     Interrupt17_Handler          /* 17: Reserved */
    DECLARE_INT_HANDLER     Interrupt18_Handler          /* 18: Reserved */
    DECLARE_INT_HANDLER     Interrupt19_Handler          /* 19: Interrupt 19 */
    DECLARE_INT_HANDLER     Interrupt20_Handler          /* 20: Interrupt 20 */
    DECLARE_INT_HANDLER     Interrupt21_Handler          /* 21: Interrupt 21 */
    DECLARE_INT_HANDLER     Interrupt22_Handler          /* 22: Interrupt 22 */
    DECLARE_INT_HANDLER     Interrupt23_Handler          /* 23: Interrupt 23 */
    DECLARE_INT_HANDLER     Interrupt24_Handler          /* 24: Interrupt 24 */
    DECLARE_INT_HANDLER     Interrupt25_Handler          /* 25: Interrupt 25 */
    DECLARE_INT_HANDLER     Interrupt26_Handler          /* 26: Interrupt 26 */
    DECLARE_INT_HANDLER     Interrupt27_Handler          /* 27: Interrupt 27 */
    DECLARE_INT_HANDLER     Interrupt28_Handler          /* 28: Interrupt 28 */
    DECLARE_INT_HANDLER     Interrupt29_Handler          /* 29: Interrupt 29 */
    DECLARE_INT_HANDLER     Interrupt30_Handler          /* 30: Interrupt 30 */
    DECLARE_INT_HANDLER     Interrupt31_Handler          /* 31: Interrupt 31 */
    DECLARE_INT_HANDLER     Interrupt32_Handler          /* 32: Interrupt 32 */
    DECLARE_INT_HANDLER     Interrupt33_Handler          /* 33: Interrupt 33 */
    DECLARE_INT_HANDLER     Interrupt34_Handler          /* 34: Interrupt 34 */
    DECLARE_INT_HANDLER     Interrupt35_Handler          /* 35: Interrupt 35 */
    DECLARE_INT_HANDLER     Interrupt36_Handler          /* 36: Interrupt 36 */
    DECLARE_INT_HANDLER     Interrupt37_Handler          /* 37: Interrupt 37 */
    DECLARE_INT_HANDLER     Interrupt38_Handler          /* 38: Interrupt 38 */
    DECLARE_INT_HANDLER     Interrupt39_Handler          /* 39: Interrupt 39 */
    DECLARE_INT_HANDLER     Interrupt40_Handler          /* 40: Interrupt 40 */
    DECLARE_INT_HANDLER     Interrupt41_Handler          /* 41: Interrupt 41 */
    DECLARE_INT_HANDLER     Interrupt42_Handler          /* 42: Interrupt 42 */
    DECLARE_INT_HANDLER     Interrupt43_Handler          /* 43: Interrupt 43 */
    DECLARE_INT_HANDLER     Interrupt44_Handler          /* 44: Interrupt 44 */
    DECLARE_INT_HANDLER     Interrupt45_Handler          /* 45: Interrupt 45 */
    DECLARE_INT_HANDLER     Interrupt46_Handler          /* 46: Interrupt 46 */
    DECLARE_INT_HANDLER     Interrupt47_Handler          /* 47: Interrupt 47 */
    DECLARE_INT_HANDLER     Interrupt48_Handler          /* 48: Interrupt 48 */
    DECLARE_INT_HANDLER     Interrupt49_Handler          /* 49: Interrupt 49 */
    DECLARE_INT_HANDLER     Interrupt50_Handler          /* 50: Interrupt 50 */
    DECLARE_INT_HANDLER     Interrupt51_Handler          /* 51: Interrupt 51 */
    DECLARE_INT_HANDLER     Interrupt52_Handler          /* 52: Interrupt 52 */
    DECLARE_INT_HANDLER     Interrupt53_Handler          /* 53: Interrupt 53 */
    DECLARE_INT_HANDLER     Interrupt54_Handler          /* 54: Interrupt 54 */
    DECLARE_INT_HANDLER     Interrupt55_Handler          /* 55: Interrupt 55 */
    DECLARE_INT_HANDLER     Interrupt56_Handler          /* 56: Interrupt 56 */
    DECLARE_INT_HANDLER     Interrupt57_Handler          /* 57: Interrupt 57 */
    DECLARE_INT_HANDLER     Interrupt58_Handler          /* 58: Interrupt 58 */
    DECLARE_INT_HANDLER     Interrupt59_Handler          /* 59: Interrupt 59 */
    DECLARE_INT_HANDLER     Interrupt60_Handler          /* 60: Interrupt 60 */
    DECLARE_INT_HANDLER     Interrupt61_Handler          /* 61: Interrupt 61 */
    DECLARE_INT_HANDLER     Interrupt62_Handler          /* 62: Interrupt 62 */
    DECLARE_INT_HANDLER     Interrupt63_Handler          /* 63: Interrupt 63 */
    DECLARE_INT_HANDLER     Interrupt64_Handler          /* 64: Interrupt 64 */
    DECLARE_INT_HANDLER     Interrupt65_Handler          /* 65: Interrupt 65 */
    DECLARE_INT_HANDLER     Interrupt66_Handler          /* 66: Interrupt 66 */
    DECLARE_INT_HANDLER     Interrupt67_Handler          /* 67: Interrupt 67 */
    DECLARE_INT_HANDLER     Interrupt68_Handler          /* 68: Interrupt 68 */
    DECLARE_INT_HANDLER     Interrupt69_Handler          /* 69: Interrupt 69 */
    DECLARE_INT_HANDLER     Interrupt70_Handler          /* 70: Interrupt 70 */
    DECLARE_INT_HANDLER     Interrupt71_Handler          /* 71: Interrupt 71 */
    DECLARE_INT_HANDLER     Interrupt72_Handler          /* 72: Interrupt 72 */
    DECLARE_INT_HANDLER     Interrupt73_Handler          /* 73: Interrupt 73 */
    DECLARE_INT_HANDLER     Interrupt74_Handler          /* 74: Interrupt 74 */
    DECLARE_INT_HANDLER     Interrupt75_Handler          /* 75: Interrupt 75 */
    DECLARE_INT_HANDLER     Interrupt76_Handler          /* 76: Interrupt 76 */
    DECLARE_INT_HANDLER     Interrupt77_Handler          /* 77: Interrupt 77 */
    .word                   0                            /* 78: reserved     */
    .word                   0                            /* 79: reserved     */

/* Header */
    .ascii   "Hr"                                        /*   byte(0~1):   header mark */
    .byte    0x01, 0x04                                  /*   byte(2~3):   header version 1.4  */
    .word    0                                           /*   byte(4~7):   image size  */
    .word    vector_base                                 /*   byte(8~11):  image vma   */
    .byte    0x01, 0x00, 0x03                            /*   byte(12~14): image version 1.0.1 */
    .byte    0x00                                        /*   byte(15~15): image flags */
    .ascii   "ARCS", "_APP", "\0\0\0\0"                  /*   byte(16~27): image name  */
    .short   0x00                                        /*   byte(28~29): patch_offset*/
    .short   0x00                                        /*   byte(30~31): gpt_offset  */

    .word    0                                           /*   byte(32~59): reserved         */
    .word    0                                           /*   byte(36~39): reserved         */
    .word    0                                           /*   byte(40~43): reserved         */
    .word    0                                           /*   byte(44~47): reserved         */
    .word    0                                           /*   byte(48~51): reserved         */
    .word    0                                           /*   byte(52~55): reserved         */
    .word    0                                           /*   byte(56~59): reserved         */
    .short   0x0                                         /*   byte(60~61): header check sum */
    .short   0x0                                         /*   byte(62~63): vector check sum */


/* Reset Handler called on controller reset */
    .section .init
    .section .init

.section .init

.globl _start
.type _start, @function
_start:
    /* ===== Startup Stage 1 ===== */
    /* Disable Global Interrupt */
    csrc CSR_MSTATUS, MSTATUS_MIE

    /* Initialize GP and TP */
    .option push
    .option norelax
    la gp, __global_pointer$
    la tp, __tls_base
    .option pop

    /* Set correct sp for current cpu */
    la sp, __StackTop

    /*  Set the the NMI base mnvec to share with mtvec by setting CSR_MMISC_CTL bit 9 NMI_CAUSE_FFF to 1 */
    li t0, MMISC_CTL_NMI_CAUSE_FFF
    csrs CSR_MMISC_CTL, t0

    /*  Intialize ECLIC vector interrupt base address mtvt to vector_base */
    la t0, vector_base
    csrw CSR_MTVT, t0

    /* Set ECLIC non-vector entry to be controlled by mtvt2 CSR register.
     * Intialize ECLIC non-vector interrupt base address mtvt2 to irq_entry. */
    la t0, irq_entry
    csrw CSR_MTVT2, t0
    csrs CSR_MTVT2, 0x1

    /* Set Exception Entry MTVEC to early_exc_entry Due to settings above, Exception and NMI will share common entry.
     * This early_exc_entry is only used during early boot stage before main */
    la t0, early_exc_entry
    csrw CSR_MTVEC, t0

    /* Set the interrupt processing mode to ECLIC mode */
    li t0, 0x3f
    csrc CSR_MTVEC, t0
    csrs CSR_MTVEC, 0x3

    /* ===== Startup Stage 2 ===== */
    /* Enable FPU and Vector Unit if f/d/v exist in march */
#if defined(__riscv_flen) && __riscv_flen > 0
    /* Enable FPU, and set state to initial */
    li t0, MSTATUS_FS
    csrc mstatus, t0
    li t0, MSTATUS_FS_INITIAL
    csrs mstatus, t0
#endif

#if defined(__riscv_vector)
    /* Enable Vector, and set state to initial */
    li t0, MSTATUS_VS
    csrc mstatus, t0
    li t0, MSTATUS_VS_INITIAL
    csrs mstatus, t0
#endif

    /* Enable mcycle and minstret counter */
    csrci CSR_MCOUNTINHIBIT, 0x5

    /*call platform pre startup*/
    call platform_pre_startup

    /* ===== Startup Stage 3 ===== */
    /* Load each prev-stage section from ROM to RAM when the LMA!=VMA and LEN > 0 */
    call scatload

    /* Call vendor defined SystemInit to initialize the micro-controller system
     * SystemInit will just be called by boot cpu */
    call SystemInit

    /* When all initialization steps done set exception entry to correct exception entry and jump to main. 
     * And set the interrupt processing mode to ECLIC mode */
    la t0, exc_entry
    csrw CSR_MTVEC, t0
    li t0, 0x3f
    csrc CSR_MTVEC, t0
    csrs CSR_MTVEC, 0x3

    /* BPU cold bringup need time, so enable BPU before enter to main */
    li t0, MMISC_CTL_BPU
    csrs CSR_MMISC_CTL, t0

    /* 防止在RTOS未启动前发生exception无法正常回朔 */
    /* 注意,这里从中断栈的底部加上1024字节作为回溯时栈空间 */
    /* 如果触发异常前, 栈已经使用到了这个空间, 则可能导致回溯失败 */
    la t0, __StackTop
    addi t0, t0, 1024
    csrw CSR_MSCRATCH, t0

    /* Call Application Entry */
    call entry

/* Early boot exception entry before main */
.align 6
.global early_exc_entry
.type early_exc_entry, @function
early_exc_entry:
    wfi
    j early_exc_entry
