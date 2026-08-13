/*
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include "PowerManager.h"
#include "venusa_ap.h"

#include "sys/reset_reason.h"

/*
 * 缓存上电时读取的 raw cause。snapshot 是 destructive 操作（读寄存器并
 * 清硬件），因此必须只执行一次；后续 sys_arch_reset_reason_get() 全部
 * 走缓存。
 */
static uint32_t s_raw_cause;

void sys_arch_reset_reason_snapshot(void)
{
    s_raw_cause = IP_AON_CTRL->REG_SYSRST_STATUS.all;
    HAL_PMU_ClearSysResetCause();
}

/*
 * VenusA SYSRST_STATUS bit -> SDK 抽象类别映射。
 *
 *   - PMU_RST_POR              POR + PAD reset    -> POR | PIN
 *   - PMU_RST_AON              AON 软件复位 + WDT -> WATCHDOG | SOFTWARE
 *   - PMU_RST_SYSRESETREQ_*    core sysresetreq   -> SOFTWARE
 *   - PMU_RST_SW*              cmn 软件复位       -> SOFTWARE
 *   - PMU_RST_WDT_CORE*        core watchdog      -> WATCHDOG
 *
 * 任何未识别的 raw 位都视为"硬件有原因但不认识"，附加置位
 * SYS_RESET_REASON_UNKNOWN，避免静默丢位。
 */
#define VENUSA_KNOWN_RAW_MASK                  \
    ((1u << PMU_RST_POR)                       \
     | (1u << PMU_RST_AON)                     \
     | (1u << PMU_RST_SYSRESETREQ_CORE1)       \
     | (1u << PMU_RST_SYSRESETREQ_CORE0)       \
     | (1u << PMU_RST_SW1)                     \
     | (1u << PMU_RST_SW0)                     \
     | (1u << PMU_RST_WDT_CORE1)               \
     | (1u << PMU_RST_WDT_CORE0))

uint32_t sys_arch_reset_reason_get(void)
{
    uint32_t raw = s_raw_cause;
    uint32_t r = 0;

    if (raw & (1u << PMU_RST_POR)) {
        r |= SYS_RESET_REASON_POR | SYS_RESET_REASON_PIN;
    }
    if (raw & (1u << PMU_RST_AON)) {
        r |= SYS_RESET_REASON_WATCHDOG | SYS_RESET_REASON_SOFTWARE;
    }
    if (raw & ((1u << PMU_RST_SYSRESETREQ_CORE0)
             | (1u << PMU_RST_SYSRESETREQ_CORE1)
             | (1u << PMU_RST_SW0)
             | (1u << PMU_RST_SW1))) {
        r |= SYS_RESET_REASON_SOFTWARE;
    }
    if (raw & ((1u << PMU_RST_WDT_CORE0)
             | (1u << PMU_RST_WDT_CORE1))) {
        r |= SYS_RESET_REASON_WATCHDOG;
    }

    if (raw & ~VENUSA_KNOWN_RAW_MASK) {
        r |= SYS_RESET_REASON_UNKNOWN;
    }

    return r;
}
