#ifndef __BOOT_APP_TARGET_H__
#define __BOOT_APP_TARGET_H__

#include <stdint.h>

typedef enum {
    BOOT_APP_TARGET_UNKNOWN = 0,
    BOOT_APP_TARGET_AP,
    BOOT_APP_TARGET_CP,
} boot_app_target_t;

boot_app_target_t boot_app_detect_target(uint32_t app_addr);

#endif /* __BOOT_APP_TARGET_H__ */
