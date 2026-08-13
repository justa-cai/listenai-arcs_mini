/*
 * wakeup_entry.S
 *
 *  Created on: 2024年12月6日
 *      Author: USER
 */
#include "riscv_encoding.h"

.extern pm_sleep_startup
#ifdef CONFIG_PM_HEAP_IN_PSRAM
.extern pm_psram_exit_sleep
#endif


#if 1
.section .text
.globl __light_sleep_entry
.type __light_sleep_entry, @function
__light_sleep_entry:
#if PM_CORE_PRIMARY
    /* Set LDO_AON_VTRIM to 0.*/
    li      a0, 0x480000d4
    lw      a1, 0(a0)
    li      a2, 0x7E
    andn    a1, a1, a2
    sw      a1, 0(a0)

#endif
    /* ===== Startup Stage 1 ===== */
    /* Disable Global Interrupt */
    csrc CSR_MSTATUS, MSTATUS_MIE

    call  sleep_wakeup
#else

.section .pm.ramcode
.globl __light_sleep_entry
.type __light_sleep_entry, @function
__light_sleep_entry:
    /* Initialize GP and TP */
    .option push
    .option norelax
    la gp, __global_pointer$
    la tp, __tls_base
    .option pop
    la sp, _sp
#endif

#if PM_CORE_PRIMARY
.section .pm.ramcode
#else
.section .text
#endif
.type sleep_wakeup, @function
sleep_wakeup:

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

    /* BPU cold bringup need time, so enable BPU before enter to main */
    li t0, MMISC_CTL_BPU
    csrs CSR_MMISC_CTL, t0

    call pm_sleep_startup


.section .pm.startup_bss,"aw",@nobits
.align 2
__light_sleep_stash:
#ifdef CONFIG_PM_HEAP_IN_PSRAM
    .space 60
#else
    .space 4
#endif

.section .pm.ramcode
.globl __light_sleep_save
.type __light_sleep_save, @function
__light_sleep_save:
#ifdef CONFIG_PM_HEAP_IN_PSRAM
    la   t0, __light_sleep_stash
    sw   ra, 0(t0)
    sw   tp, 4(t0)
    sw   s0, 8(t0)
    sw   s1, 12(t0)
    sw   s2, 16(t0)
    sw   s3, 20(t0)
    sw   s4, 24(t0)
    sw   s5, 28(t0)
    sw   s6, 32(t0)
    sw   s7, 36(t0)
    sw   s8, 40(t0)
    sw   s9, 44(t0)
    sw   s10,48(t0)
    sw   s11,52(t0)
    sw   sp, 56(t0)
#if PM_CORE_PRIMARY
    li      t0, 0x480000d4
    lw      t1, 0(t0)
    li      t2, 0x7e
    andn    t1, t1, t2
    ori     t1, t1, 0x44
    sw      t1, 0(t0)
    call  __set_aon_and_sleep
#else
    wfi
    ret  /* SP 未改,无帧可弹 */
#endif
#else  /* !CONFIG_PM_HEAP_IN_PSRAM*/
    add	sp,sp,-64
    sw ra,0(sp)
    sw tp,4(sp)
    sw s0,8(sp)
    sw s1,12(sp)
    sw s2,16(sp)
    sw s3,20(sp)
    sw s4,24(sp)
    sw s5,28(sp)
    sw s6,32(sp)
    sw s7,36(sp)
    sw s8,40(sp)
    sw s9,44(sp)
    sw s10,48(sp)
    sw s11,52(sp)
    la t0, __light_sleep_stash
    sw sp,0(t0)
#if PM_CORE_PRIMARY
    li      t0, 0x480000d4
    lw      t1, 0(t0)
    li      t2, 0x7e            /* bit[6:1] */
    andn    t1, t1, t2          /* t1 &= ~0x7e */
    ori     t1, t1, 0x44        /* 0x22 << 1 */
    sw      t1, 0(t0)
    call  __set_aon_and_sleep
#else
    wfi
    add sp,sp,64
    ret
#endif
#endif /* CONFIG_PM_HEAP_IN_PSRAM */

.section .pm.ramcode
.globl __light_sleep_restore
.type __light_sleep_restore, @function
__light_sleep_restore:
#ifdef CONFIG_PM_HEAP_IN_PSRAM
    la a0, __light_sleep_stash
    la t0, _sp
    csrw mscratch, t0
    lw sp, 56(a0)
    lw ra, 0(a0)
    lw tp, 4(a0)
    lw s0, 8(a0)
    lw s1, 12(a0)
    lw s2, 16(a0)
    lw s3, 20(a0)
    lw s4, 24(a0)
    lw s5, 28(a0)
    lw s6, 32(a0)
    lw s7, 36(a0)
    lw s8, 40(a0)
    lw s9, 44(a0)
    lw s10,48(a0)
    lw s11,52(a0)
    ret
#else  /* !CONFIG_PM_HEAP_IN_PSRAM*/
    la a0, __light_sleep_stash
    la t0, _sp
    csrw mscratch, t0
    lw sp, 0(a0)
    lw ra,0(sp)
    lw tp,4(sp)
    lw s0,8(sp)
    lw s1,12(sp)
    lw s2,16(sp)
    lw s3,20(sp)
    lw s4,24(sp)
    lw s5,28(sp)
    lw s6,32(sp)
    lw s7,36(sp)
    lw s8,40(sp)
    lw s9,44(sp)
    lw s10,48(sp)
    lw s11,52(sp)
    add sp,sp,64
    ret
#endif /* CONFIG_PM_HEAP_IN_PSRAM */

#if PM_CORE_PRIMARY
.section .pm.ilm
.type __set_aon_and_sleep, @function
__set_aon_and_sleep:
    wfi
    call __restore_from_wfi

.section .text
.type __restore_from_wfi, @function
__restore_from_wfi:
    li      a0, 0x480000d4
    lw      a1, 0(a0)
    li      a2, 0x7E
    andn    a1, a1, a2
    sw      a1, 0(a0)

    li      t0, 5000
loop:
    addi    t0, t0, -1
    bnez    t0, loop

#ifdef CONFIG_PM_HEAP_IN_PSRAM
    /* Warm wakeup, temporarily use the internal stack */
    la   sp, _sp
    call pm_psram_exit_sleep
    la   t0, __light_sleep_stash
    lw   ra, 0(t0)
    lw   sp, 56(t0)
    ret
#else
    lw ra,0(sp)
    add sp,sp,64
    ret
#endif /*CONFIG_PM_HEAP_IN_PSRAM*/
#endif /*PM_CORE_PRIMARY*/

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
