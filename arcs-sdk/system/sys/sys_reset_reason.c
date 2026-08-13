/*
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdio.h>

#include "sys/reset_reason.h"

uint32_t sys_arch_reset_reason_get(void);
void sys_arch_reset_reason_snapshot(void);

sys_reset_reason_t sys_reset_reason_get(void)
{
    return sys_arch_reset_reason_get();
}

/*
 * Idempotent guard：snapshot 是 destructive 操作（读寄存器并清硬件），
 * 必须只执行一次。重复调用会读到已被清零的硬件，把 boot 时的 cause
 * 覆盖成 0，让后续 sys_reset_reason_get() 永远丢失复位原因。
 */
void sys_reset_reason_snapshot(void)
{
    static bool done;

    if (done) {
        return;
    }
    sys_arch_reset_reason_snapshot();
    done = true;
}

void sys_reset_reason_banner(void)
{
    sys_reset_reason_t r = sys_reset_reason_get();

    printf("Reset reason: 0x%08lx", (unsigned long)r);
    if (r & SYS_RESET_REASON_POR)      printf(" POR");
    if (r & SYS_RESET_REASON_PIN)      printf(" PIN");
    if (r & SYS_RESET_REASON_WATCHDOG) printf(" WATCHDOG");
    if (r & SYS_RESET_REASON_SOFTWARE) printf(" SOFTWARE");
    if (r & SYS_RESET_REASON_UNKNOWN)  printf(" UNKNOWN");
    printf("\n");
}
