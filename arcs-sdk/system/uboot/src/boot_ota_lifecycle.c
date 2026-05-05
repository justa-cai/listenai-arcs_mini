#include "boot_ota_lifecycle.h"

#include <stddef.h>

#include "boot_control_store.h"
#include "boot_ota_request.h"

static int boot_ota_lifecycle_clear_request_and_restore_normal(const uboot_ota_request_t *request)
{
    if (request == NULL) {
        return BOOT_OTA_LIFECYCLE_FAILED;
    }

    if (boot_ota_request_clear() != 0) {
        boot_ota_request_set_mode(BOOT_MODE_UPDATE);
        return BOOT_OTA_LIFECYCLE_FAILED;
    }

    if (boot_ota_request_set_mode(BOOT_MODE_NORMAL) == 0) {
        return 0;
    }

    boot_ota_request_save(request);
    boot_ota_request_set_mode(BOOT_MODE_UPDATE);
    return BOOT_OTA_LIFECYCLE_FAILED;
}

int boot_ota_lifecycle_run(boot_ota_lifecycle_handler_t handler, void *ctx)
{
    int handler_result;
    uboot_ota_request_t request;
    uboot_ota_failure_info_t failure;

    if (boot_ota_request_get_mode() != BOOT_MODE_UPDATE) {
        return BOOT_OTA_LIFECYCLE_IDLE;
    }

    if (handler == NULL) {
        boot_ota_request_set_mode(BOOT_MODE_UPDATE);
        return BOOT_OTA_LIFECYCLE_FAILED;
    }

    if (boot_ota_request_load(&request) != 0) {
        boot_ota_request_set_mode(BOOT_MODE_UPDATE);
        return BOOT_OTA_LIFECYCLE_FAILED;
    }

    failure.source = request.source;
    failure.reason = UBOOT_OTA_FAILURE_INTERNAL;
    failure.detail = UBOOT_OTA_FAILURE_DETAIL_UNKNOWN;

    handler_result = handler(&request, &failure, ctx);
    if (handler_result == 0) {
        if (boot_ota_lifecycle_clear_request_and_restore_normal(&request) != 0) {
            return BOOT_OTA_LIFECYCLE_FAILED;
        }
        if (boot_control_store_clear_failure() != 0) {
            return BOOT_OTA_LIFECYCLE_FAILED;
        }
        return BOOT_OTA_LIFECYCLE_UPDATED;
    }

    if (boot_control_store_set_failure(&failure) != 0) {
        boot_ota_request_set_mode(BOOT_MODE_UPDATE);
        return BOOT_OTA_LIFECYCLE_FAILED;
    }

    if (handler_result == BOOT_OTA_LIFECYCLE_FAILED_DISCARD_REQUEST) {
        if (boot_ota_lifecycle_clear_request_and_restore_normal(&request) != 0) {
            return BOOT_OTA_LIFECYCLE_FAILED;
        }
        return BOOT_OTA_LIFECYCLE_DISCARDED;
    }

    boot_ota_request_set_mode(BOOT_MODE_UPDATE);
    return BOOT_OTA_LIFECYCLE_FAILED;
}
