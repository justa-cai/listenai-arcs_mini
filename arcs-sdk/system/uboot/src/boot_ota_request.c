#include "boot_ota_request.h"

#include <stddef.h>
#include <string.h>

#include "boot_control_store.h"

static int boot_ota_request_has_path(const char *path)
{
    if (path == NULL) {
        return 0;
    }

    return path[0] != '\0';
}

int boot_ota_request_is_valid(const uboot_ota_request_t *req)
{
    if (req == NULL) {
        return -1;
    }

    switch (req->source) {
    case UBOOT_OTA_SOURCE_FLASH:
        return req->package_size > 0 ? 0 : -1;
    case UBOOT_OTA_SOURCE_TF:
        return boot_ota_request_has_path(req->tf.path) ? 0 : -1;
    default:
        return -1;
    }
}

int boot_ota_request_save(const uboot_ota_request_t *req)
{
    boot_ota_request_record_t record;

    if (boot_ota_request_is_valid(req) != 0) {
        return -1;
    }

    memset(&record, 0, sizeof(record));
    record.magic = BOOT_OTA_REQUEST_MAGIC;
    record.version = BOOT_OTA_REQUEST_VERSION;
    record.request = *req;

    return boot_control_store_save(&record);
}

int boot_ota_request_load(uboot_ota_request_t *req)
{
    boot_ota_request_record_t record;

    if (req == NULL) {
        return -1;
    }

    memset(&record, 0, sizeof(record));
    if (boot_control_store_load(&record) != 0) {
        return -1;
    }

    if (record.magic != BOOT_OTA_REQUEST_MAGIC || record.version != BOOT_OTA_REQUEST_VERSION) {
        return -1;
    }

    if (boot_ota_request_is_valid(&record.request) != 0) {
        return -1;
    }

    *req = record.request;
    return 0;
}

int boot_ota_request_clear(void)
{
    return boot_control_store_clear();
}

boot_mode_t boot_ota_request_get_mode(void)
{
    boot_mode_t mode = BOOT_MODE_NORMAL;

    if (boot_control_store_get_mode(&mode) != 0) {
        return BOOT_MODE_NORMAL;
    }

    return mode;
}

int boot_ota_request_set_mode(boot_mode_t mode)
{
    return boot_control_store_set_mode(mode);
}

int boot_ota_request_mark_update_mode(void)
{
    return boot_ota_request_set_mode(BOOT_MODE_UPDATE);
}

bool boot_ota_request_has_pending(void)
{
    uboot_ota_request_t req;
    return boot_ota_request_load(&req) == 0;
}
