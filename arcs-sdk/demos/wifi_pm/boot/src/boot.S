#include "riscv_encoding.h"

#ifndef BOOT_HARTID
    .equ BOOT_HARTID, 0
#endif

    .section .vtable, "ax", @progbits

    .globl vector_base
    .type vector_base, @object
vector_base:
    j _start
    .align LOG_REGBYTES

    .word default_intexc_handler    /* 1 */
    .word default_intexc_handler    /* 2 */
    .word default_intexc_handler    /* 3 */
    .word default_intexc_handler    /* 4 */
    .word default_intexc_handler    /* 5 */
    .word default_intexc_handler    /* 6 */
    .word default_intexc_handler    /* 7 */
    .word default_intexc_handler    /* 8 */
    .word default_intexc_handler    /* 9 */
    .word default_intexc_handler    /* 10 */
    .word default_intexc_handler    /* 11 */
    .word default_intexc_handler    /* 12 */
    .word default_intexc_handler    /* 13 */
    .word default_intexc_handler    /* 14 */
    .word default_intexc_handler    /* 15 */
    .word default_intexc_handler    /* 16 */
    .word default_intexc_handler    /* 17 */
    .word default_intexc_handler    /* 18 */
    .word default_intexc_handler    /* 19 */
    .word default_intexc_handler    /* 20 */
    .word default_intexc_handler    /* 21 */
    .word default_intexc_handler    /* 22 */
    .word default_intexc_handler    /* 23 */
    .word default_intexc_handler    /* 24 */
    .word default_intexc_handler    /* 25 */
    .word default_intexc_handler    /* 26 */
    .word default_intexc_handler    /* 27 */
    .word default_intexc_handler    /* 28 */
    .word default_intexc_handler    /* 29 */
    .word default_intexc_handler    /* 30 */
    .word default_intexc_handler    /* 31 */
    .word default_intexc_handler    /* 32 */
    .word default_intexc_handler    /* 33 */
    .word default_intexc_handler    /* 34 */
    .word default_intexc_handler    /* 35 */
    .word default_intexc_handler    /* 36 */
    .word default_intexc_handler    /* 37 */
    .word default_intexc_handler    /* 38 */
    .word default_intexc_handler    /* 39 */
    .word default_intexc_handler    /* 40 */
    .word default_intexc_handler    /* 41 */
    .word default_intexc_handler    /* 42 */
    .word default_intexc_handler    /* 43 */
    .word default_intexc_handler    /* 44 */
    .word default_intexc_handler    /* 45 */
    .word default_intexc_handler    /* 46 */
    .word default_intexc_handler    /* 47 */
    .word default_intexc_handler    /* 48 */
    .word default_intexc_handler    /* 49 */
    .word default_intexc_handler    /* 50 */
    .word default_intexc_handler    /* 51 */
    .word default_intexc_handler    /* 52 */
    .word default_intexc_handler    /* 53 */
    .word default_intexc_handler    /* 54 */
    .word default_intexc_handler    /* 55 */
    .word default_intexc_handler    /* 56 */
    .word default_intexc_handler    /* 57 */
    .word default_intexc_handler    /* 58 */
    .word default_intexc_handler    /* 59 */
    .word default_intexc_handler    /* 60 */
    .word default_intexc_handler    /* 61 */
    .word default_intexc_handler    /* 62 */
    .word default_intexc_handler    /* 63 */
    .word default_intexc_handler    /* 64 */
    .word default_intexc_handler    /* 65 */
    .word default_intexc_handler    /* 66 */
    .word default_intexc_handler    /* 67 */
    .word default_intexc_handler    /* 68 */
    .word default_intexc_handler    /* 69 */
    .word default_intexc_handler    /* 70 */
    .word default_intexc_handler    /* 71 */
    .word default_intexc_handler    /* 72 */
    .word default_intexc_handler    /* 73 */
    .word default_intexc_handler    /* 74 */
    .word default_intexc_handler    /* 75 */
    .word default_intexc_handler    /* 76 */
    .word default_intexc_handler    /* 77 */
    .word 0                         /* 78: reserved */
    .word 0                         /* 79: reserved */

/* Image Header */
    .ascii   "Hr"
    .byte    0x01, 0x04
    .word    0
    .word    vector_base
    .byte    0x01, 0x00, 0x03
    .byte    0x00
    .ascii   "ARCS"
    .ascii   "_BOO"
    .ascii   "T\0\0\0"
    .short   0x00
    .short   0x00
    .word    0, 0, 0, 0, 0, 0, 0
    .short   0x0
    .short   0x0

    .section .init

    .globl _start
    .type _start, @function
_start:
    csrc CSR_MSTATUS, MSTATUS_MIE

#ifndef SMP_CPU_CNT
    csrr a0, CSR_MHARTID
    andi a0, a0, 0xFF
    li a1, BOOT_HARTID
    bne a0, a1, __amp_wait
#endif

    .option push
    .option norelax
    la gp, __global_pointer$
    la tp, __tls_base
    .option pop

    la sp, _sp

#if (BOOT_HARTID == 0) && (CONFIG_PM) && !defined(CFG_AMP_IPC)
    call ap_startup_check
#endif

    /* Copy ap_ilm_text from flash to ILM (required for flash clock config functions) */
    la a0, __copy_table_start__
    la a1, __copy_table_end__
    bgeu a0, a1, 2f
1:
    lw a2, 0(a0)    /* src (LMA) */
    lw a3, 4(a0)    /* dst (VMA) */
    lw a4, 8(a0)    /* end */
    addi a0, a0, 12
    bgeu a3, a4, 1f
3:
    lw t0, (a2)
    sw t0, (a3)
    addi a2, a2, 4
    addi a3, a3, 4
    bltu a3, a4, 3b
1:
    bltu a0, a1, 1b
2:

    /* Load data section */
    la a0, _data_lma
    la a1, _data
    beq a0, a1, 2f
    la a2, _edata
    bgeu a1, a2, 2f
1:
    lw t0, (a0)
    sw t0, (a1)
    addi a0, a0, 4
    addi a1, a1, 4
    bltu a1, a2, 1b
2:
    /* Clear bss section */
    la a0, __bss_start
    la a1, _end
    bgeu a0, a1, 2f
1:
    sw zero, (a0)
    addi a0, a0, 4
    bltu a0, a1, 1b
2:
    li a0, 0
    li a1, 0
    call main

__amp_wait:
1:
    wfi
    j 1b

.align 6
.global default_intexc_handler
.type default_intexc_handler, @function
default_intexc_handler:
    wfi
    j default_intexc_handler