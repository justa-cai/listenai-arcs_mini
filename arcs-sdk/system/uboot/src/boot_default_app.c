#include "boot_default_app.h"

#include <string.h>

#include "boot_config.h"

#define BOOT_DEFAULT_APP_HEADER_OFFSET       0x140U
#define BOOT_DEFAULT_APP_LOAD_ADDR_OFFSET    8U

bool boot_default_app_image_is_valid(const uint8_t *image_base, uint32_t expected_base)
{
    const uint8_t *header;
    uint32_t load_addr;

    if (image_base == NULL) {
        return false;
    }

    header = image_base + BOOT_DEFAULT_APP_HEADER_OFFSET;
    if (header[0] != 'H' || header[1] != 'r') {
        return false;
    }

    memcpy(&load_addr, header + BOOT_DEFAULT_APP_LOAD_ADDR_OFFSET, sizeof(load_addr));
    return load_addr == expected_base;
}

bool boot_default_app_is_valid(void)
{
    uint32_t app_addr = boot_default_app_addr_get();

    return boot_default_app_image_is_valid((const uint8_t *)(uintptr_t)app_addr, app_addr);
}

void boot_default_app_prepare_recovery(uint32_t *boot_info_raw, bool app_valid)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};

    if (boot_info_raw == NULL || app_valid) {
        return;
    }

    bits.raw = *boot_info_raw;
    bits.info.req = 1u;
    if (bits.info.recover_reason == RECOVER_REASON_NONE) {
        bits.info.recover_reason = RECOVER_REASON_APP_INVALID;
    }
    *boot_info_raw = bits.raw;
}
