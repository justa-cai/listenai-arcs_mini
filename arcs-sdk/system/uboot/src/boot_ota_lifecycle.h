#ifndef __BOOT_OTA_LIFECYCLE_H__
#define __BOOT_OTA_LIFECYCLE_H__

#include "uboot_ota_api.h"

typedef enum {
    BOOT_OTA_LIFECYCLE_IDLE = 0,
    BOOT_OTA_LIFECYCLE_UPDATED = 1,
    BOOT_OTA_LIFECYCLE_DISCARDED = 2,
    BOOT_OTA_LIFECYCLE_FAILED = -1,
    BOOT_OTA_LIFECYCLE_FAILED_DISCARD_REQUEST = -2,
} boot_ota_lifecycle_result_t;

typedef int (*boot_ota_lifecycle_handler_t)(const uboot_ota_request_t *req,
                                            uboot_ota_failure_info_t *failure,
                                            void *ctx);

int boot_ota_lifecycle_run(boot_ota_lifecycle_handler_t handler, void *ctx);

#endif /* __BOOT_OTA_LIFECYCLE_H__ */
