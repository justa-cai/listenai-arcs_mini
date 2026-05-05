#ifndef __BOOT_RESET_CAUSE_H__
#define __BOOT_RESET_CAUSE_H__

#include <stdint.h>

/*
 * Keep host tests self-contained: these reset source IDs are stable SoC ABI
 * values and match soc/arcs/hal/chip/arcs/include/PowerManager.h.
 */
#ifndef PMU_RST_CP_WDT
#define PMU_RST_CP_WDT 16U
#endif

#ifndef PMU_RST_AP_SW_WDT
#define PMU_RST_AP_SW_WDT 19U
#endif

void boot_reset_cause_apply(uint32_t *boot_info_raw, uint32_t sysrst_status);

#endif
