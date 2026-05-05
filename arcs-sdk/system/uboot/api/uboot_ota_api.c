#include "uboot_ota_api.h"

#include <string.h>

#include "../src/boot_control_store.h"
#include "../src/boot_ota_handoff.h"
#include "../src/boot_ota_request.h"
#include "../src/boot_partab.h"

#define UBOOT_OTA_SOURCE_PARTITION_PRIMARY   "OTA_TXZ"
#define UBOOT_OTA_SOURCE_PARTITION_LEGACY    "OTA"

__attribute__((weak)) int boot_ota_handoff_request_upgrade_reboot(void)
{
    return 0;
}

__attribute__((weak)) void boot_ota_handoff_clear_pending_update(void)
{
}

__attribute__((weak)) const boot_config_t *uboot_ota_partab_get(void)
{
    return (const boot_config_t *)BOOT_CONFIG_BASE;
}

static int uboot_ota_partab_is_valid(const boot_config_t *cfg)
{
    if (cfg == NULL) {
        return -1;
    }

    if (cfg->magic != PARTAB_MAGIC || cfg->size == 0U || cfg->size > sizeof(*cfg) ||
        cfg->part_count == 0U || cfg->part_count > MAX_PARTITIONS) {
        return -1;
    }

    return 0;
}

static const partition_t *uboot_ota_find_partition(const boot_config_t *cfg, const char *name)
{
    uint32_t i;

    if (cfg == NULL || name == NULL) {
        return NULL;
    }

    for (i = 0; i < cfg->part_count; i++) {
        const partition_t *part = &cfg->partitions[i];

        if ((part->flags & PART_FLAG_VALID) == 0) {
            continue;
        }
        if (strncmp(part->name, name, PART_NAME_SIZE) == 0) {
            return part;
        }
    }

    return NULL;
}

static const partition_t *uboot_ota_find_source_partition(const boot_config_t *cfg)
{
    const partition_t *part = uboot_ota_find_partition(cfg, UBOOT_OTA_SOURCE_PARTITION_PRIMARY);

    if (part != NULL) {
        return part;
    }

    return uboot_ota_find_partition(cfg, UBOOT_OTA_SOURCE_PARTITION_LEGACY);
}

int uboot_ota_start(const uboot_ota_request_t *req)
{
    int ret = boot_ota_request_save(req);

    if (ret != 0) {
        return ret;
    }

    ret = boot_ota_request_mark_update_mode();
    if (ret != 0) {
        boot_ota_request_clear();
        return ret;
    }

    ret = boot_ota_handoff_request_upgrade_reboot();
    if (ret != 0) {
        boot_ota_request_set_mode(BOOT_MODE_NORMAL);
        boot_ota_request_clear();
        boot_ota_handoff_clear_pending_update();
        return ret;
    }

    return 0;
}

int uboot_ota_start_from_flash(uint32_t flash_offset, uint32_t package_size)
{
    uboot_ota_request_t req = {
        .source = UBOOT_OTA_SOURCE_FLASH,
        .package_size = package_size,
        .flash = {
            .flash_offset = flash_offset,
        },
    };

    return uboot_ota_start(&req);
}

int uboot_ota_start_from_tf(const char *path)
{
    uboot_ota_request_t req;

    memset(&req, 0, sizeof(req));
    req.source = UBOOT_OTA_SOURCE_TF;

    if (path != NULL) {
        strncpy(req.tf.path, path, sizeof(req.tf.path) - 1);
        req.tf.path[sizeof(req.tf.path) - 1] = '\0';
    }

    return uboot_ota_start(&req);
}

int uboot_ota_start_from_ota_partition(void)
{
    const boot_config_t *cfg = uboot_ota_partab_get();
    const partition_t *part;

    if (uboot_ota_partab_is_valid(cfg) != 0) {
        return -1;
    }

    part = uboot_ota_find_source_partition(cfg);
    if (part == NULL || part->size == 0U || part->base < CONFIG_MEM_FLASH_BASE) {
        return -1;
    }

    return uboot_ota_start_from_flash(part->base - CONFIG_MEM_FLASH_BASE, part->size);
}

int uboot_ota_get_last_failure(uboot_ota_failure_info_t *info)
{
    if (info == NULL) {
        return -1;
    }

    memset(info, 0, sizeof(*info));
    return boot_control_store_get_failure(info);
}
