#include "uboot_power_api.h"

#include "../src/boot_config.h"

#ifndef UBOOT_POWER_API_HOST_TEST
#include "arcs_ap.h"
#include "PowerManager.h"
#endif

__attribute__((weak)) volatile uint32_t *uboot_power_api_boot_info_reg(void)
{
#ifdef UBOOT_POWER_API_HOST_TEST
    return NULL;
#else
    return &IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
#endif
}

void uboot_shutdown_request(void)
{
    volatile uint32_t *boot_info_raw = uboot_power_api_boot_info_reg();

    if (boot_info_raw != NULL) {
        union {
            struct boot_info info;
            uint32_t raw;
        } bits = {.raw = *boot_info_raw};

        bits.info.shutdown_req = 1;
        *boot_info_raw = bits.raw;
    }

#ifndef UBOOT_POWER_API_HOST_TEST
    /* AON_SW_RESET（sys_platform_sw_full_reset）是 HARD reboot，会
     * 把 REG_AON_DIG_RSVD4 一并清掉，stage0 读不到我们置的
     * shutdown_req；CMN SW reset（__HAL_PMU_WholeChip_RST_ENABLE）
     * 只复位 AP/CP/CMN、保留 AON，boot_info 才能留给 stage0。
     * boot_watchdog_handler 要让 boot_wdt/req 活到下一次引导也是
     * 用的这条 CMN reset。*/
    __HAL_PMU_WholeChip_RST_ENABLE();
#endif

    while (1) {
        __asm__ volatile("wfi");
    }
}
