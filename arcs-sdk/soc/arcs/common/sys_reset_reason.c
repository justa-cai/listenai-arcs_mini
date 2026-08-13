/*
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include "PowerManager.h"
#include "sys/reset_reason.h"

void sys_arch_reset_reason_snapshot(void)
{
    HAL_PMU_SnapshotResetCause();
}

/*
 * ARCS SYSRST_STATUS bit -> SDK 抽象类别映射。
 *
 * 硬件不可分辨的位采用 "两个都置" 策略，调用方按任一类别处理永远不会
 * 漏判：
 *   - PMU_RST_POR        硬件合并 POR + PAD reset      -> POR | PIN
 *   - PMU_RST_AON        AON 子系统软件复位 + AON WDT  -> WATCHDOG | SOFTWARE
 *   - PMU_RST_AP_SW_WDT  ap2soc：AP 软件复位 + AP WDT  -> WATCHDOG | SOFTWARE
 *
 * 任何未被本表识别的 raw 位都视为"硬件有原因但不认识"，附加置位
 * SYS_RESET_REASON_UNKNOWN —— 即使有已识别位共存，也提示调用方
 * 此次 raw 含有未映射信息（避免静默丢位）。
 */
#define ARCS_KNOWN_RAW_MASK \
    ((1u << PMU_RST_POR)       \
     | (1u << PMU_RST_AON)     \
     | (1u << PMU_RST_CP_WDT)  \
     | (1u << PMU_RST_CMN)     \
     | (1u << PMU_RST_CP_SW)   \
     | (1u << PMU_RST_AP_SW_WDT))

uint32_t sys_arch_reset_reason_get(void)
{
    uint32_t raw = HAL_PMU_GetSysResetCauseRaw();
    uint32_t r = 0;

    if (raw & (1u << PMU_RST_POR)) {
        r |= SYS_RESET_REASON_POR | SYS_RESET_REASON_PIN;
    }
    if (raw & (1u << PMU_RST_AON)) {
        r |= SYS_RESET_REASON_WATCHDOG | SYS_RESET_REASON_SOFTWARE;
    }
    if (raw & (1u << PMU_RST_CP_WDT)) {
        r |= SYS_RESET_REASON_WATCHDOG;
    }
    if (raw & (1u << PMU_RST_CMN)) {
        r |= SYS_RESET_REASON_SOFTWARE;
    }
    if (raw & (1u << PMU_RST_CP_SW)) {
        r |= SYS_RESET_REASON_SOFTWARE;
    }
    if (raw & (1u << PMU_RST_AP_SW_WDT)) {
        r |= SYS_RESET_REASON_WATCHDOG | SYS_RESET_REASON_SOFTWARE;
    }

    if (raw & ~ARCS_KNOWN_RAW_MASK) {
        r |= SYS_RESET_REASON_UNKNOWN;
    }

    return r;
}
