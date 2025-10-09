
#include "riscv_encoding.h"

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
    DECLARE_INT_HANDLER     irq_default_handler           /* 0: Reserved, default handler for vector table remapped cases */
#endif
    DECLARE_INT_HANDLER     irq_default_handler           /* 1: Reserved */
    DECLARE_INT_HANDLER     irq_default_handler           /* 2: Reserved */
#ifndef CFG_RTOS
    DECLARE_INT_HANDLER     irq_default_handler           /* 3: Machine software interrupt */
#else
    DECLARE_INT_HANDLER     irq_default_handler           /* 3: Machine software interrupt, vector mode for FreeRTOS implementation */
#endif
    DECLARE_INT_HANDLER     irq_default_handler           /* 4: Reserved */
    DECLARE_INT_HANDLER     irq_default_handler           /* 5: Reserved */
    DECLARE_INT_HANDLER     irq_default_handler           /* 6: Reserved */
    DECLARE_INT_HANDLER     irq_default_handler           /* 7: Machine timer interrupt */
    DECLARE_INT_HANDLER     irq_default_handler           /* 8: Reserved */
    DECLARE_INT_HANDLER     irq_default_handler           /* 9: Reserved */
    DECLARE_INT_HANDLER     irq_default_handler          /* 10: Reserved */
    DECLARE_INT_HANDLER     irq_default_handler          /* 11: Reserved */
    DECLARE_INT_HANDLER     irq_default_handler          /* 12: Reserved */
    DECLARE_INT_HANDLER     irq_default_handler          /* 13: Reserved */
    DECLARE_INT_HANDLER     irq_default_handler          /* 14: Reserved */
    DECLARE_INT_HANDLER     irq_default_handler          /* 15: Reserved */
    DECLARE_INT_HANDLER     irq_default_handler          /* 16: Reserved */
    DECLARE_INT_HANDLER     irq_default_handler          /* 17: Reserved */
    DECLARE_INT_HANDLER     irq_default_handler          /* 18: Reserved */
    DECLARE_INT_HANDLER     irq_default_handler          /* 19: Interrupt 19 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 20: Interrupt 20 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 21: Interrupt 21 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 22: Interrupt 22 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 23: Interrupt 23 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 24: Interrupt 24 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 25: Interrupt 25 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 26: Interrupt 26 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 27: Interrupt 27 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 28: Interrupt 28 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 29: Interrupt 29 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 30: Interrupt 30 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 31: Interrupt 31 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 32: Interrupt 32 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 33: Interrupt 33 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 34: Interrupt 34 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 35: Interrupt 35 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 36: Interrupt 36 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 37: Interrupt 37 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 38: Interrupt 38 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 39: Interrupt 39 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 40: Interrupt 40 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 41: Interrupt 41 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 42: Interrupt 42 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 43: Interrupt 43 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 44: Interrupt 44 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 45: Interrupt 45 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 46: Interrupt 46 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 47: Interrupt 47 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 48: Interrupt 48 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 49: Interrupt 49 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 50: Interrupt 50 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 51: Interrupt 51 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 52: Interrupt 52 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 53: Interrupt 53 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 54: Interrupt 54 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 55: Interrupt 55 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 56: Interrupt 56 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 57: Interrupt 57 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 58: Interrupt 58 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 59: Interrupt 59 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 60: Interrupt 60 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 61: Interrupt 61 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 62: Interrupt 62 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 63: Interrupt 63 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 64: Interrupt 64 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 65: Interrupt 65 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 66: Interrupt 66 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 67: Interrupt 67 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 68: Interrupt 68 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 69: Interrupt 69 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 70: Interrupt 70 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 71: Interrupt 71 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 72: Interrupt 72 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 73: Interrupt 73 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 74: Interrupt 74 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 75: Interrupt 75 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 76: Interrupt 76 */
    DECLARE_INT_HANDLER     irq_default_handler          /* 77: Interrupt 77 */
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

    call boot_run

/* Early boot exception entry before main */
.align 6
.global early_exc_entry
.type early_exc_entry, @function
early_exc_entry:
    wfi
    j early_exc_entry

.align 6
.global irq_entry
.type irq_entry, @function
irq_entry:
    wfi
    j irq_entry
