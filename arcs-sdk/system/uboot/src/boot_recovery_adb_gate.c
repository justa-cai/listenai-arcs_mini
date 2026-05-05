#include "boot_recovery_adb_gate.h"

#include "boot_config.h"

static bool boot_recovery_adb_reason_allows_start(uint8_t recover_reason)
{
    switch (recover_reason) {
    case RECOVER_REASON_SOFT_REQ:
    case RECOVER_REASON_HARD_REQ:
    case RECOVER_REASON_APP_INVALID:
    case RECOVER_REASON_AP_WDT_TIMEOUT:
    case RECOVER_REASON_BOOT_WDT_TIMEOUT:
        return true;
    default:
        return false;
    }
}

bool boot_recovery_adb_should_start(uint32_t boot_info_raw)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {
        .raw = boot_info_raw,
    };

    /* Only explicit recovery metadata opens ADB; OTA/pending entry bits alone do not. */
    return boot_recovery_adb_reason_allows_start(bits.info.recover_reason);
}
