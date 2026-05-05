#include "boot_ota_source.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "boot_ota_request.h"

#if defined(CONFIG_BOOT_OTA_SOURCE_TF) || defined(BOOT_OTA_SOURCE_UNIT_TEST)
#include "lisa_device.h"
#include "lisa_sdmmc.h"
#include "lsfs.h"
#define BOOT_OTA_SOURCE_HAS_TF_IO 1

extern int disk_init(const void *dev);
extern int lsfs_init(void);

static struct lsfs_mount_t g_boot_ota_tf_mount = {
    .type = LSFS_FATFS,
    .mnt_point = BOOT_OTA_TF_DEFAULT_MOUNT_POINT,
    .fs_data = NULL,
};
static struct lsfs_file_t g_boot_ota_tf_file;
static boot_ota_source_t *g_boot_ota_tf_owner;
static bool g_boot_ota_tf_opened;
#endif

#ifndef CONFIG_MEM_FLASH_BASE
#define CONFIG_MEM_FLASH_BASE 0x30000000UL
#endif

static int boot_ota_source_set_error(boot_ota_source_t *source, int error)
{
    if (source != NULL) {
        source->last_error = error;
    }

    return error;
}

static uboot_ota_source_t boot_ota_source_failure_source(const uboot_ota_request_t *request)
{
    if (request != NULL && request->source == UBOOT_OTA_SOURCE_TF) {
        return UBOOT_OTA_SOURCE_TF;
    }

    return UBOOT_OTA_SOURCE_FLASH;
}

static void boot_ota_source_set_failure(uboot_ota_failure_info_t *failure,
                                        uboot_ota_source_t source,
                                        uboot_ota_failure_reason_t reason,
                                        uboot_ota_failure_detail_t detail)
{
    if (failure == NULL) {
        return;
    }

    failure->source = source;
    failure->reason = reason;
    failure->detail = detail;
}

static int boot_ota_source_copy_path(char *dst, size_t dst_size, const char *src)
{
    size_t src_len;

    if (dst == NULL || dst_size == 0U || src == NULL) {
        return -1;
    }

    src_len = strlen(src);
    if (src_len + 1U > dst_size) {
        return -1;
    }

    memcpy(dst, src, src_len + 1U);
    return 0;
}

static int boot_ota_tf_normalize_path(const char *input, char out[UBOOT_OTA_PATH_MAX])
{
    size_t mount_len;
    int written;

    if (input == NULL || input[0] == '\0' || out == NULL) {
        return -1;
    }

    mount_len = strlen(BOOT_OTA_TF_DEFAULT_MOUNT_POINT);
    if (strncmp(input, BOOT_OTA_TF_DEFAULT_MOUNT_POINT, mount_len) == 0 &&
        (input[mount_len] == '/' || input[mount_len] == '\0')) {
        return boot_ota_source_copy_path(out, UBOOT_OTA_PATH_MAX, input);
    }

    if (input[0] == '/') {
        written = snprintf(out, UBOOT_OTA_PATH_MAX, "%s%s", BOOT_OTA_TF_DEFAULT_MOUNT_POINT, input);
    } else {
        written = snprintf(out, UBOOT_OTA_PATH_MAX, "%s/%s", BOOT_OTA_TF_DEFAULT_MOUNT_POINT, input);
    }

    return (written > 0 && written < UBOOT_OTA_PATH_MAX) ? 0 : -1;
}

int boot_ota_source_from_request(const uboot_ota_request_t *request,
                                 boot_ota_source_t *source,
                                 uboot_ota_failure_info_t *failure)
{
    uint32_t addr;
    uboot_ota_source_t source_id = boot_ota_source_failure_source(request);

    if (request == NULL || source == NULL) {
        boot_ota_source_set_failure(failure,
                                    source_id,
                                    UBOOT_OTA_FAILURE_BAD_REQUEST,
                                    UBOOT_OTA_FAILURE_DETAIL_REQUEST_CONVERT_FAILED);
        return -1;
    }

    if (boot_ota_request_is_valid(request) != 0) {
        boot_ota_source_set_failure(failure,
                                    source_id,
                                    UBOOT_OTA_FAILURE_BAD_REQUEST,
                                    source_id == UBOOT_OTA_SOURCE_TF ?
                                        UBOOT_OTA_FAILURE_DETAIL_TF_PATH_INVALID :
                                        UBOOT_OTA_FAILURE_DETAIL_FLASH_RANGE_INVALID);
        return -1;
    }

    memset(source, 0, sizeof(*source));
    source->last_error = BOOT_OTA_SOURCE_ERR_NONE;

    switch (request->source) {
    case UBOOT_OTA_SOURCE_FLASH:
        addr = CONFIG_MEM_FLASH_BASE + request->flash.flash_offset;
        if (addr < CONFIG_MEM_FLASH_BASE) {
            boot_ota_source_set_failure(failure,
                                        UBOOT_OTA_SOURCE_FLASH,
                                        UBOOT_OTA_FAILURE_BAD_REQUEST,
                                        UBOOT_OTA_FAILURE_DETAIL_FLASH_RANGE_INVALID);
            return -1;
        }

        source->kind = BOOT_OTA_SOURCE_KIND_FLASH;
        source->size = request->package_size;
        source->flash.addr = addr;
        return 0;
    case UBOOT_OTA_SOURCE_TF:
        if (boot_ota_tf_normalize_path(request->tf.path, source->tf.normalized_path) != 0) {
            boot_ota_source_set_failure(failure,
                                        UBOOT_OTA_SOURCE_TF,
                                        UBOOT_OTA_FAILURE_BAD_REQUEST,
                                        UBOOT_OTA_FAILURE_DETAIL_TF_PATH_INVALID);
            return -1;
        }

        source->kind = BOOT_OTA_SOURCE_KIND_TF;
        source->tf.path = source->tf.normalized_path;
        return 0;
    default:
        boot_ota_source_set_failure(failure,
                                    source_id,
                                    UBOOT_OTA_FAILURE_BAD_REQUEST,
                                    UBOOT_OTA_FAILURE_DETAIL_REQUEST_CONVERT_FAILED);
        return -1;
    }
}

#if defined(BOOT_OTA_SOURCE_HAS_TF_IO)
static void boot_ota_source_close_tf(void)
{
    if (g_boot_ota_tf_opened) {
        lsfs_close(&g_boot_ota_tf_file);
        g_boot_ota_tf_opened = false;
    }

    g_boot_ota_tf_owner = NULL;
}

static int boot_ota_source_probe_tf_card(void)
{
    lisa_device_t *sdmmc = lisa_device_get("sdmmc0");

    if (sdmmc == NULL) {
        return -1;
    }

    return lisa_sdmmc_probe(sdmmc);
}

static int boot_ota_source_prepare_tf_storage(void)
{
    if (boot_ota_source_probe_tf_card() != 0) {
        return -1;
    }
    if (disk_init(NULL) != 0) {
        return -1;
    }
    if (lsfs_init() != 0) {
        return -1;
    }
    if (lsfs_mount(&g_boot_ota_tf_mount) != 0) {
        return -1;
    }

    return 0;
}

static int boot_ota_source_open_tf(boot_ota_source_t *source)
{
    off_t size;

    boot_ota_source_close_tf();

    if (boot_ota_source_prepare_tf_storage() != 0) {
        return boot_ota_source_set_error(source, BOOT_OTA_SOURCE_ERR_UNAVAILABLE);
    }

    lsfs_file_t_init(&g_boot_ota_tf_file);
    if (lsfs_open(&g_boot_ota_tf_file, source->tf.path, LSFS_O_READ) != 0) {
        return boot_ota_source_set_error(source, BOOT_OTA_SOURCE_ERR_UNAVAILABLE);
    }
    g_boot_ota_tf_opened = true;

    if (lsfs_seek(&g_boot_ota_tf_file, 0, LSFS_SEEK_END) != 0) {
        boot_ota_source_close_tf();
        return boot_ota_source_set_error(source, BOOT_OTA_SOURCE_ERR_UNAVAILABLE);
    }

    size = lsfs_tell(&g_boot_ota_tf_file);
    if (size < 0 || (uint32_t)size != (uint64_t)size) {
        boot_ota_source_close_tf();
        return boot_ota_source_set_error(source, BOOT_OTA_SOURCE_ERR_UNAVAILABLE);
    }

    if (lsfs_seek(&g_boot_ota_tf_file, 0, LSFS_SEEK_SET) != 0) {
        boot_ota_source_close_tf();
        return boot_ota_source_set_error(source, BOOT_OTA_SOURCE_ERR_UNAVAILABLE);
    }

    source->size = (uint32_t)size;
    g_boot_ota_tf_owner = source;
    return boot_ota_source_set_error(source, BOOT_OTA_SOURCE_ERR_NONE);
}
#endif

int boot_ota_source_open(boot_ota_source_t *source)
{
    if (source == NULL) {
        return BOOT_OTA_SOURCE_ERR_INVALID;
    }

    switch (source->kind) {
    case BOOT_OTA_SOURCE_KIND_FLASH:
        return boot_ota_source_set_error(source, BOOT_OTA_SOURCE_ERR_NONE);
    case BOOT_OTA_SOURCE_KIND_TF:
#if defined(BOOT_OTA_SOURCE_HAS_TF_IO)
        return boot_ota_source_open_tf(source);
#else
        return boot_ota_source_set_error(source, BOOT_OTA_SOURCE_ERR_UNAVAILABLE);
#endif
    default:
        return boot_ota_source_set_error(source, BOOT_OTA_SOURCE_ERR_INVALID);
    }
}

int boot_ota_source_close(boot_ota_source_t *source)
{
    if (source == NULL) {
        return BOOT_OTA_SOURCE_ERR_INVALID;
    }

#if defined(BOOT_OTA_SOURCE_HAS_TF_IO)
    if (source->kind == BOOT_OTA_SOURCE_KIND_TF && g_boot_ota_tf_owner == source) {
        boot_ota_source_close_tf();
    }
#endif

    return boot_ota_source_set_error(source, BOOT_OTA_SOURCE_ERR_NONE);
}

int boot_ota_source_read(boot_ota_source_t *source, uint32_t offset, void *dst, uint32_t size)
{
    ssize_t read_size;

    if (source == NULL) {
        return BOOT_OTA_SOURCE_ERR_INVALID;
    }

    if (dst == NULL || size == 0U || offset > source->size || size > source->size - offset) {
        return BOOT_OTA_SOURCE_ERR_INVALID;
    }

    if (source->kind != BOOT_OTA_SOURCE_KIND_TF) {
        return boot_ota_source_set_error(source, BOOT_OTA_SOURCE_ERR_INVALID);
    }

#if !defined(BOOT_OTA_SOURCE_HAS_TF_IO)
    return boot_ota_source_set_error(source, BOOT_OTA_SOURCE_ERR_UNAVAILABLE);
#else
    if (g_boot_ota_tf_owner != source || !g_boot_ota_tf_opened) {
        return boot_ota_source_set_error(source, BOOT_OTA_SOURCE_ERR_INVALID);
    }

    if (lsfs_seek(&g_boot_ota_tf_file, (off_t)offset, LSFS_SEEK_SET) != 0) {
        return boot_ota_source_set_error(source, BOOT_OTA_SOURCE_ERR_IO);
    }

    read_size = lsfs_read(&g_boot_ota_tf_file, dst, size);
    if (read_size != (ssize_t)size) {
        return boot_ota_source_set_error(source, BOOT_OTA_SOURCE_ERR_IO);
    }

    return boot_ota_source_set_error(source, BOOT_OTA_SOURCE_ERR_NONE);
#endif
}

int boot_ota_source_last_error(const boot_ota_source_t *source)
{
    if (source == NULL) {
        return BOOT_OTA_SOURCE_ERR_INVALID;
    }

    return source->last_error;
}
