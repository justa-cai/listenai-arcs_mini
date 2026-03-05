/*
 * wakeup_entry.S
 *
 *  Created on: 2024年12月6日
 *      Author: USER
 */
#include "riscv_encoding.h"

.extern pm_sleep_startup

.section .ramcode
.globl __light_sleep_entry
.type __light_sleep_entry, @function
__light_sleep_entry:
    /* ===== Startup Stage 1 ===== */
    /* Disable Global Interrupt */
    csrc CSR_MSTATUS, MSTATUS_MIE

    /* Initialize GP and TP */
    .option push
    .option norelax
    la gp, __global_pointer$
    la tp, __tls_base
    .option pop

    la sp, _sp

    /*
     * Set the the NMI base mnvec to share
     * with mtvec by setting CSR_MMISC_CTL
     * bit 9 NMI_CAUSE_FFF to 1
     */
    li t0, MMISC_CTL_NMI_CAUSE_FFF
    csrs CSR_MMISC_CTL, t0

    /*
     * Intialize ECLIC vector interrupt
     * base address mtvt to vector_base
     */
    la t0, vector_base /* vector_base */
    csrw CSR_MTVT, t0

    /*
     * Set ECLIC non-vector entry to be controlled
     * by mtvt2 CSR register.
     * Intialize ECLIC non-vector interrupt
     * base address mtvt2 to irq_entry.
     */
    la t0, irq_entry
    csrw CSR_MTVT2, t0
    csrs CSR_MTVT2, 0x1

    /*
     * When all initialization steps done
     * set exception entry to correct exception
     * entry and jump to main.
     * And set the interrupt processing mode to
     * ECLIC mode
     */
    la t0, exc_entry
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

#if defined(SMP_CPU_CNT) && (SMP_CPU_CNT > 1)
    csrr a0, CSR_MHARTID
    andi a0, a0, 0xFF
    li a1, BOOT_HARTID
    bne a0, a1, __skip_init
#endif
#if 0
    /* Call global constructors */
    la a0, __libc_fini_array

    call atexit

    /* Call C/C++ constructor start up code */
    call __libc_init_array
#endif
    /* BPU cold bringup need time, so enable BPU before enter to main */
    li t0, MMISC_CTL_BPU
    csrs CSR_MMISC_CTL, t0

    call pm_sleep_startup

.extern __stack_store_repo
.section .ramcode
.globl __idle_save
.type __idle_save, @function
__idle_save:
    add	sp,sp,-104
    sw ra,0(sp)
    sw tp,4(sp)
    sw t0,8(sp)
    sw t1,12(sp)
    sw t2,16(sp)
    sw a0,20(sp)
    sw a1,24(sp)
    sw a2,28(sp)
    sw a3,32(sp)
    sw a4,36(sp)
    sw a5,40(sp)
    sw a6,44(sp)
    sw a7,48(sp)
    sw t3,52(sp)
    sw t4,56(sp)
    sw t5,60(sp)
    sw t6,64(sp)
    sw s0,68(sp)
    sw s1,72(sp)
    sw s2,76(sp)
    sw s3,80(sp)
    sw s4,84(sp)
    sw s5,88(sp)
    sw s6,92(sp)
    sw s7,96(sp)
    sw s8,100(sp)
    la t0, __stack_store_repo
    sw sp,0(t0)
    wfi

.section .ramcode
.globl __idle_restore
.type __idle_restore, @function
__idle_restore:
    la a0, __stack_store_repo
    la sp, _sp
    csrw mscratch, sp
    lw sp, 0(a0)
    lw ra,0(sp)
    lw tp,4(sp)
    lw t0,8(sp)
    lw t1,12(sp)
    lw t2,16(sp)
    lw a0,20(sp)
    lw a1,24(sp)
    lw a2,28(sp)
    lw a3,32(sp)
    lw a4,36(sp)
    lw a5,40(sp)
    lw a6,44(sp)
    lw a7,48(sp)
    lw t3,52(sp)
    lw t4,56(sp)
    lw t5,60(sp)
    lw t6,64(sp)
    lw s0,68(sp)
    lw s1,72(sp)
    lw s2,76(sp)
    lw s3,80(sp)
    lw s4,84(sp)
    lw s5,88(sp)
    lw s6,92(sp)
    lw s7,96(sp)
    lw s8,100(sp)
    add sp,sp,104
    ret

    .section .text.mem_copy
    .globl mem_copy
mem_copy:
    mv      t2, a0
    beqz    a2, .done

    srli    t3, a2, 5
    andi    t4, a2, 31
    beqz    t3, .tail

.big_loop:
    lw      t0,   0(a1)
    sw      t0,   0(a0)
    lw      t0,   4(a1)
    sw      t0,   4(a0)
    lw      t0,   8(a1)
    sw      t0,   8(a0)
    lw      t0,  12(a1)
    sw      t0,  12(a0)
    lw      t0,  16(a1)
    sw      t0,  16(a0)
    lw      t0,  20(a1)
    sw      t0,  20(a0)
    lw      t0,  24(a1)
    sw      t0,  24(a0)
    lw      t0,  28(a1)
    sw      t0,  28(a0)

    addi    a0, a0, 32
    addi    a1, a1, 32
    addi    t3, t3, -1
    bnez    t3, .big_loop

.tail:
    beqz    t4, .done

    srli    t5, t4, 2
    beqz    t5, .byte_tail

.word_loop:
    lw      t0, 0(a1)
    sw      t0, 0(a0)
    addi    a0, a0, 4
    addi    a1, a1, 4
    addi    t5, t5, -1
    bnez    t5, .word_loop

.byte_tail:
    andi    t4, t4, 3
    beqz    t4, .done

.byte_loop:
    lbu     t0, 0(a1)
    sb      t0, 0(a0)
    addi    a0, a0, 1
    addi    a1, a1, 1
    addi    t4, t4, -1
    bnez    t4, .byte_loop

.done:
    mv      a0, t2
    ret