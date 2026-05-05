#include "boot_reset_cause.h"

#include "boot_config.h"

#define __boot_ramcode__ __attribute__((section(".boot_f.ramcode")))

__boot_ramcode__ void boot_reset_cause_apply(uint32_t *boot_info_raw, uint32_t sysrst_status)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};

    if (boot_info_raw == NULL) {
        return;
    }

    bits.raw = *boot_info_raw;

    if ((sysrst_status & (1UL << PMU_RST_CP_WDT)) != 0UL) {
        bits.info.req = 1u;
        bits.info.boot_wdt = 1u;
        *boot_info_raw = bits.raw;
        return;
    }

    if ((sysrst_status & (1UL << PMU_RST_AP_SW_WDT)) == 0UL) {
        return;
    }

    bits.info.reboot_cnt++;
    if (bits.info.reboot_cnt < BOOT_RECOVERY_REQUEST_MAX_COUNT) {
        *boot_info_raw = bits.raw;
        return;
    }

    bits.info.reboot_cnt = 0u;
    bits.info.recover_reason = RECOVER_REASON_AP_WDT_TIMEOUT;
    bits.info.req = 1u;
    *boot_info_raw = bits.raw;
}
