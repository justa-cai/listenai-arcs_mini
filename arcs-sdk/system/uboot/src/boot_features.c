#include "uboot_features.h"

#ifdef CONFIG_BOOT_FEATURES

__attribute__((section(".uboot_features"), used))
const struct uboot_features_desc boot_features_desc = {
    .magic = UBOOT_FEATURES_MAGIC,
    .version = UBOOT_FEATURES_VERSION,
    .size = sizeof(struct uboot_features_desc),
    .features =
#ifdef CONFIG_BOOT_OTA_PACKAGE
        UBOOT_FEATURE_OTA |
#endif
#ifdef CONFIG_BOOT_POWER_GUARD
        UBOOT_FEATURE_POWER_GUARD |
#endif
        0u,
    .reserved = {0, 0, 0, 0},
};

#endif
