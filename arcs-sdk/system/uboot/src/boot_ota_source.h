#ifndef __BOOT_OTA_SOURCE_H__
#define __BOOT_OTA_SOURCE_H__

#include <stdint.h>

#include "uboot_ota_api.h"

#define BOOT_OTA_TF_DEFAULT_MOUNT_POINT "/SD:"

typedef enum {
    BOOT_OTA_SOURCE_KIND_NONE = 0,
    BOOT_OTA_SOURCE_KIND_FLASH = 1,
    BOOT_OTA_SOURCE_KIND_TF = 2,
} boot_ota_source_kind_t;

typedef enum {
    BOOT_OTA_SOURCE_ERR_NONE = 0,
    BOOT_OTA_SOURCE_ERR_INVALID = -1,
    BOOT_OTA_SOURCE_ERR_UNAVAILABLE = -2,
    BOOT_OTA_SOURCE_ERR_IO = -3,
} boot_ota_source_error_t;

typedef struct {
    boot_ota_source_kind_t kind;
    uint32_t size;
    int last_error;
    union {
        struct {
            uint32_t addr;
        } flash;
        struct {
            const char *path;
            char normalized_path[UBOOT_OTA_PATH_MAX];
        } tf;
    };
} boot_ota_source_t;

int boot_ota_source_from_request(const uboot_ota_request_t *request,
                                 boot_ota_source_t *source,
                                 uboot_ota_failure_info_t *failure);
int boot_ota_source_open(boot_ota_source_t *source);
int boot_ota_source_close(boot_ota_source_t *source);
int boot_ota_source_read(boot_ota_source_t *source, uint32_t offset, void *dst, uint32_t size);
int boot_ota_source_last_error(const boot_ota_source_t *source);

#endif /* __BOOT_OTA_SOURCE_H__ */
