#include "uboot_features_api.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef CONFIG_BOOT_FEATURES_ADDR
#error "CONFIG_BOOT_FEATURES_ADDR must be defined to use uboot_features_api"
#endif

__attribute__((weak)) const struct uboot_features_desc *uboot_features_api_desc(void)
{
    return (const struct uboot_features_desc *)(uintptr_t)CONFIG_BOOT_FEATURES_ADDR;
}

static bool s_cached = false;
static uint32_t s_features = 0u;

uint32_t uboot_features_query(void)
{
    if (s_cached) {
        return s_features;
    }

    const struct uboot_features_desc *desc = uboot_features_api_desc();
    uint32_t bits = 0u;

    if (desc != NULL
        && desc->magic == UBOOT_FEATURES_MAGIC
        && desc->version == UBOOT_FEATURES_VERSION
        && desc->size == sizeof(struct uboot_features_desc)) {
        bits = desc->features;
    }

    s_features = bits;
    s_cached = true;
    return bits;
}

bool uboot_features_has(uint32_t feature)
{
    return (uboot_features_query() & feature) == feature;
}
