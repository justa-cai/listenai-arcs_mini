#include "boot_control_store.h"

#include <stddef.h>
#include <string.h>

int boot_control_store_save(const boot_ota_request_record_t *record)
{
    (void)record;
    return -1;
}

int boot_control_store_load(boot_ota_request_record_t *record)
{
    (void)record;
    return -1;
}

int boot_control_store_clear(void)
{
    return 0;
}

int boot_control_store_get_mode(boot_mode_t *mode)
{
    if (mode == NULL) {
        return -1;
    }

    *mode = BOOT_MODE_NORMAL;
    return 0;
}

int boot_control_store_set_mode(boot_mode_t mode)
{
    return mode == BOOT_MODE_NORMAL ? 0 : -1;
}

int boot_control_store_get_failure(uboot_ota_failure_info_t *info)
{
    if (info == NULL) {
        return -1;
    }

    memset(info, 0, sizeof(*info));
    info->reason = UBOOT_OTA_FAILURE_NONE;
    info->detail = UBOOT_OTA_FAILURE_DETAIL_NONE;
    return 0;
}

int boot_control_store_set_failure(const uboot_ota_failure_info_t *info)
{
    (void)info;
    return -1;
}

int boot_control_store_clear_failure(void)
{
    return 0;
}
