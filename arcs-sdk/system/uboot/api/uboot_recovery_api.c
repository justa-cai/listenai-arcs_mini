#include "uboot_recovery_api.h"

#include "../src/boot_config.h"

#ifndef UBOOT_RECOVERY_API_HOST_TEST
#include "arcs_ap.h"
#endif

__attribute__((weak)) volatile uint32_t *uboot_recovery_api_boot_info_reg(void)
{
#ifdef UBOOT_RECOVERY_API_HOST_TEST
    return NULL;
#else
    return &IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
#endif
}

static int uboot_recovery_request_soft(void)
{
    volatile uint32_t *boot_info_raw = uboot_recovery_api_boot_info_reg();
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};

    if (boot_info_raw == NULL) {
        return -1;
    }

    bits.raw = *boot_info_raw;
    bits.info.recover_reason = RECOVER_REASON_SOFT_REQ;
    *boot_info_raw = bits.raw;
    boot_recovery_software_enter();
    return 0;
}

int uboot_recovery_request(uboot_recovery_mode_t mode)
{
    switch (mode) {
    case UBOOT_RECOVERY_MODE_SOFT:
        return uboot_recovery_request_soft();
    case UBOOT_RECOVERY_MODE_HARD:
        return boot_recovery_hardware_enter();
    default:
        return -1;
    }
}
