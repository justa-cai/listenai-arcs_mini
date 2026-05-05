#include "boot_app_target.h"

#include <string.h>

#define BOOT_APP_HEADER_OFFSET        0x140U
#define BOOT_APP_TARGET_CORE_OFFSET   32U
#define BOOT_APP_TARGET_CORE_MAGIC    0x5443U

boot_app_target_t boot_app_detect_target(uint32_t app_addr)
{
    const uint8_t *header = (const uint8_t *)(uintptr_t)app_addr + BOOT_APP_HEADER_OFFSET;

    if (header[0] != 'H' || header[1] != 'r') {
        return BOOT_APP_TARGET_UNKNOWN;
    }

    uint32_t target_core;
    memcpy(&target_core, header + BOOT_APP_TARGET_CORE_OFFSET, sizeof(target_core));

    if ((target_core >> 16) != BOOT_APP_TARGET_CORE_MAGIC) {
        return BOOT_APP_TARGET_UNKNOWN;
    }

    return (target_core & 0xFF) ? BOOT_APP_TARGET_CP : BOOT_APP_TARGET_AP;
}
