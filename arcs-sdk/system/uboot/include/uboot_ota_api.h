#ifndef __UBOOT_OTA_API_H__
#define __UBOOT_OTA_API_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UBOOT_OTA_SOURCE_FLASH = 0,
    UBOOT_OTA_SOURCE_TF = 1,
} uboot_ota_source_t;

typedef enum {
    UBOOT_OTA_FAILURE_NONE = 0,
    UBOOT_OTA_FAILURE_BAD_REQUEST,
    UBOOT_OTA_FAILURE_SOURCE_UNAVAILABLE,
    UBOOT_OTA_FAILURE_SOURCE_IO,
    UBOOT_OTA_FAILURE_PACKAGE_INVALID,
    UBOOT_OTA_FAILURE_MANIFEST_INVALID,
    UBOOT_OTA_FAILURE_IMAGE_MISSING,
    UBOOT_OTA_FAILURE_IMAGE_VERIFY_FAILED,
    UBOOT_OTA_FAILURE_APPLY_FAILED,
    UBOOT_OTA_FAILURE_INTERNAL,
} uboot_ota_failure_reason_t;

typedef enum {
    UBOOT_OTA_FAILURE_DETAIL_NONE = 0,
    UBOOT_OTA_FAILURE_DETAIL_REQUEST_CONVERT_FAILED,
    UBOOT_OTA_FAILURE_DETAIL_FLASH_RANGE_INVALID,
    UBOOT_OTA_FAILURE_DETAIL_TF_PATH_INVALID,
    UBOOT_OTA_FAILURE_DETAIL_TF_OPEN_FAILED,
    UBOOT_OTA_FAILURE_DETAIL_SOURCE_READ_FAILED,
    UBOOT_OTA_FAILURE_DETAIL_PACKAGE_FORMAT_UNSUPPORTED,
    UBOOT_OTA_FAILURE_DETAIL_PACKAGE_TRUNCATED,
    UBOOT_OTA_FAILURE_DETAIL_TAR_PARSE_FAILED,
    UBOOT_OTA_FAILURE_DETAIL_MANIFEST_MISSING,
    UBOOT_OTA_FAILURE_DETAIL_MANIFEST_PARSE_FAILED,
    UBOOT_OTA_FAILURE_DETAIL_MANIFEST_SCHEMA_INVALID,
    UBOOT_OTA_FAILURE_DETAIL_IMAGE_ENTRY_MISSING,
    UBOOT_OTA_FAILURE_DETAIL_IMAGE_INCOMPLETE,
    UBOOT_OTA_FAILURE_DETAIL_IMAGE_MD5_NOT_VERIFIED,
    UBOOT_OTA_FAILURE_DETAIL_IMAGE_MD5_MISMATCH,
    UBOOT_OTA_FAILURE_DETAIL_PARTITION_NOT_FOUND,
    UBOOT_OTA_FAILURE_DETAIL_FLASH_ERASE_FAILED,
    UBOOT_OTA_FAILURE_DETAIL_FLASH_WRITE_FAILED,
    UBOOT_OTA_FAILURE_DETAIL_FLASH_VERIFY_FAILED,
    UBOOT_OTA_FAILURE_DETAIL_UNKNOWN,
} uboot_ota_failure_detail_t;

typedef struct uboot_ota_failure_info_t {
    uboot_ota_source_t source;
    uboot_ota_failure_reason_t reason;
    uboot_ota_failure_detail_t detail;
} uboot_ota_failure_info_t;

#define UBOOT_OTA_PATH_MAX 128

typedef struct {
    uboot_ota_source_t source;
    uint32_t package_size;
    union {
        struct {
            uint32_t flash_offset;
        } flash;
        struct {
            char path[UBOOT_OTA_PATH_MAX];
        } tf;
    };
} uboot_ota_request_t;

int uboot_ota_start(const uboot_ota_request_t *req);
int uboot_ota_start_from_flash(uint32_t flash_offset, uint32_t package_size);
int uboot_ota_start_from_tf(const char *path);
int uboot_ota_start_from_ota_partition(void);
int uboot_ota_get_last_failure(uboot_ota_failure_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* __UBOOT_OTA_API_H__ */
