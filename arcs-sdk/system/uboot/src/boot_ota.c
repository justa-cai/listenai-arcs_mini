/*
 * Boot OTA升级模块
 * 
 * 支持TXZ压缩格式的固件升级
 * 移植自 lsboot/src/mods/partab/ptota.c 并适配新芯片
 */

#include "boot_ota.h"
#include "boot_md5.h"
#include "boot_ota_manifest.h"
#include "boot_nvs.h"
#include "syslog.h"
#include "sysheap.h"
#include "xzdec/include/xz.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef CONFIG_BOOT_DISPLAY
#include "boot_display.h"
#endif

extern int boot_watchdog_feed(void);

#define __boot_ota_ramcode__ __attribute__((section(".boot_f.ramcode")))
#define OTA_XZ_MAGIC_SIZE 6
#define OTA_MANIFEST_FILE_NAME "config.json"

typedef enum {
    BOOT_OTA_PACKAGE_FORMAT_UNKNOWN = 0,
    BOOT_OTA_PACKAGE_FORMAT_TAR,
    BOOT_OTA_PACKAGE_FORMAT_TXZ,
} boot_ota_package_format_t;

static const uint8_t g_boot_ota_xz_magic[OTA_XZ_MAGIC_SIZE] = {0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00};

typedef enum {
    BOOT_OTA_TAR_FILE_SKIP = 0,
    BOOT_OTA_TAR_FILE_IMAGE,
    BOOT_OTA_TAR_FILE_MANIFEST,
} boot_ota_tar_file_kind_t;

typedef enum {
    BOOT_OTA_PROCESS_VALIDATE = 0,
    BOOT_OTA_PROCESS_APPLY,
} boot_ota_process_mode_t;


static uboot_ota_failure_info_t *g_boot_ota_failure;
static uboot_ota_source_t g_boot_ota_failure_source = UBOOT_OTA_SOURCE_FLASH;

static uboot_ota_source_t boot_ota_failure_source_from_descriptor(const boot_ota_source_t *source)
{
    if (source != NULL && source->kind == BOOT_OTA_SOURCE_KIND_TF) {
        return UBOOT_OTA_SOURCE_TF;
    }

    return UBOOT_OTA_SOURCE_FLASH;
}

static void boot_ota_failure_begin(uboot_ota_failure_info_t *failure, const boot_ota_source_t *source)
{
    g_boot_ota_failure = failure;
    g_boot_ota_failure_source = boot_ota_failure_source_from_descriptor(source);

    if (failure == NULL) {
        return;
    }

    failure->source = g_boot_ota_failure_source;
    failure->reason = UBOOT_OTA_FAILURE_INTERNAL;
    failure->detail = UBOOT_OTA_FAILURE_DETAIL_UNKNOWN;
}

static void boot_ota_failure_end(void)
{
    g_boot_ota_failure = NULL;
    g_boot_ota_failure_source = UBOOT_OTA_SOURCE_FLASH;
}

static void boot_ota_failure_set(uboot_ota_failure_reason_t reason,
                                 uboot_ota_failure_detail_t detail)
{
    if (g_boot_ota_failure == NULL) {
        return;
    }

    g_boot_ota_failure->source = g_boot_ota_failure_source;
    g_boot_ota_failure->reason = reason;
    g_boot_ota_failure->detail = detail;
}

static void boot_ota_failure_set_from_source_error(const boot_ota_source_t *source, int error)
{
    g_boot_ota_failure_source = boot_ota_failure_source_from_descriptor(source);

    switch (error) {
    case BOOT_OTA_SOURCE_ERR_UNAVAILABLE:
        boot_ota_failure_set(UBOOT_OTA_FAILURE_SOURCE_UNAVAILABLE,
                             source != NULL && source->kind == BOOT_OTA_SOURCE_KIND_TF ?
                                 UBOOT_OTA_FAILURE_DETAIL_TF_OPEN_FAILED :
                                 UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
        return;
    case BOOT_OTA_SOURCE_ERR_IO:
        boot_ota_failure_set(UBOOT_OTA_FAILURE_SOURCE_IO,
                             UBOOT_OTA_FAILURE_DETAIL_SOURCE_READ_FAILED);
        return;
    case BOOT_OTA_SOURCE_ERR_INVALID:
        boot_ota_failure_set(UBOOT_OTA_FAILURE_BAD_REQUEST,
                             UBOOT_OTA_FAILURE_DETAIL_REQUEST_CONVERT_FAILED);
        return;
    default:
        boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                             UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
        return;
    }
}

/*
 * 八进制字符串转整数 (用于解析TAR头)
 */
static __boot_ota_ramcode__ int oct_to_int(const char *p, int width)
{
    int val = 0;
    while (width--) {
        char c = *p++;
        if (c == 0) {
            return val;
        }
        if (c >= '0' && c <= '7') {
            val = val * 8 + (c - '0');
        } else if (c == ' ') {
            /* 忽略空格 */
        } else {
            return -1;
        }
    }
    return val;
}

/*
 * 路径匹配 (简单通配符匹配)
 */
static __boot_ota_ramcode__ bool path_match(const char *path, const char *pattern)
{
    if (pattern == NULL || path == NULL) {
        return false;
    }

    /* 若 pattern 不含路径分隔符，则仅按文件名匹配；否则按完整路径匹配 */
    const char *path_use = path;
    if (!strchr(pattern, '/') && !strchr(pattern, '\\')) {
        const char *last_slash = strrchr(path, '/');
        const char *last_bslash = strrchr(path, '\\');
        const char *base = path;
        if (last_slash && last_bslash) {
            base = (last_slash > last_bslash) ? last_slash + 1 : last_bslash + 1;
        } else if (last_slash) {
            base = last_slash + 1;
        } else if (last_bslash) {
            base = last_bslash + 1;
        }
        path_use = base;
    }

    /* 归一化：转小写并去掉可选的 .bin 后缀，便于无视大小写及后缀比较 */
    char norm_path[128] = {0};
    char norm_pattern[128] = {0};

    size_t i = 0;
    for (; path_use[i] && i < sizeof(norm_path) - 1; i++) {
        norm_path[i] = (char)tolower((unsigned char)path_use[i]);
    }
    norm_path[i] = '\0';

    for (i = 0; pattern[i] && i < sizeof(norm_pattern) - 1; i++) {
        norm_pattern[i] = (char)tolower((unsigned char)pattern[i]);
    }
    norm_pattern[i] = '\0';

    /* 去掉末尾的 .bin */
    size_t lp = strlen(norm_path);
    if (lp > 4 && strcmp(&norm_path[lp - 4], ".bin") == 0) {
        norm_path[lp - 4] = '\0';
    }
    size_t lpat = strlen(norm_pattern);
    if (lpat > 4 && strcmp(&norm_pattern[lpat - 4], ".bin") == 0) {
        norm_pattern[lpat - 4] = '\0';
    }

    path = norm_path;
    pattern = norm_pattern;

    while (*pattern) {
        if (*pattern == '*') {
            /* 通配符: 匹配任意字符直到下一个pattern字符 */
            pattern++;
            while (*path && *path != *pattern) {
                path++;
            }
            if (!*path && *pattern) {
                return false;
            }
        } else if (*pattern == '/' || *pattern == '\\') {
            /* 路径分隔符: 匹配 / 或 \ */
            if (*path != '/' && *path != '\\') {
                return false;
            }
            pattern++;
            path++;
        } else {
            /* 普通字符: 精确匹配 */
            if (*path != *pattern) {
                return false;
            }
            pattern++;
            path++;
        }
    }

    return (*path == 0);
}

static bool boot_ota_path_is_manifest(const char *path)
{
    return path_match(path, OTA_MANIFEST_FILE_NAME);
}

/*
 * TAR解析状态
 */
typedef struct {
    ota_param_t *param;
    ota_file_t  *current_file;
    boot_ota_tar_file_kind_t current_kind;
    boot_ota_process_mode_t mode;
    char        *manifest_buffer;
    boot_md5_context_t current_md5;
    bool        is_header;
    bool        archive_complete;
    bool        manifest_loaded;
    bool        current_md5_active;
    uint32_t    zero_block_count;
    int         remaining_size;
    int         write_pos;
    uint32_t    erased_upto;     /* APPLY: 已擦到的 part_base 相对偏移（sector 对齐） */
} tar_state_t;

static tar_state_t g_tar_state;

static __boot_ota_ramcode__ int process_tar_data(uintptr_t data_addr, uint32_t data_size);

static __boot_ota_ramcode__ int boot_ota_read_from_source(boot_ota_source_t *source, uint32_t offset,
                                                          void *dst, uint32_t size)
{
    uintptr_t flash_addr;

    if (source == NULL || dst == NULL || size == 0 || offset > source->size || size > source->size - offset) {
        return -1;
    }

    if (source->kind == BOOT_OTA_SOURCE_KIND_FLASH) {
        flash_addr = (uintptr_t)source->flash.addr + offset;
        boot_nvs_read(dst, (const void *)flash_addr, size);
        return 0;
    }

    if (source->kind == BOOT_OTA_SOURCE_KIND_TF) {
        return boot_ota_source_read(source, offset, dst, size);
    }

    return -1;
}

static ota_file_t *boot_ota_find_file(ota_param_t *param, const char *path)
{
    uint32_t i;

    if (param == NULL || path == NULL) {
        return NULL;
    }

    for (i = 0; i < param->file_count; i++) {
        if (param->files[i].path_match[0] != '\0' && path_match(path, param->files[i].path_match)) {
            return &param->files[i];
        }
    }

    return NULL;
}

static void boot_ota_md5_to_hex(const uint8_t digest[16], char hex[33])
{
    static const char digits[] = "0123456789abcdef";
    size_t i;

    if (digest == NULL || hex == NULL) {
        return;
    }

    for (i = 0; i < 16U; ++i) {
        hex[i * 2U] = digits[digest[i] >> 4];
        hex[i * 2U + 1U] = digits[digest[i] & 0x0FU];
    }
    hex[32] = '\0';
}

#define BOOT_OTA_FLASH_MD5_CHUNK 256U

/*
 * Compare manifest md5 against the current flash content at the target
 * partition. Return 0 when the flash already holds the exact bytes (so the
 * caller can skip erase + write), negative otherwise.
 */
static __boot_ota_ramcode__ int boot_ota_flash_matches_expected(const ota_file_t *file)
{
    boot_md5_context_t ctx;
    uint8_t digest[BOOT_MD5_DIGEST_SIZE];
    uint8_t buf[BOOT_OTA_FLASH_MD5_CHUNK];
    uint32_t offset = 0;
    uint32_t remaining;

    if (file == NULL || file->file_size == 0U) {
        return -1;
    }

    remaining = file->file_size;
    boot_md5_init(&ctx);

    while (remaining > 0U) {
        uint32_t chunk = remaining < sizeof(buf) ? remaining : sizeof(buf);

        /* 分区可能最大到数 MB，整个循环在看门狗周期内跑不完，必须分段喂狗 */
        boot_watchdog_feed();

        boot_nvs_read(buf, (const void *)(uintptr_t)(file->part_base + offset), chunk);
        boot_md5_update(&ctx, buf, chunk);
        remaining -= chunk;
        offset += chunk;
    }

    boot_md5_finish(&ctx, digest);
    return memcmp(digest, file->expected_md5, BOOT_MD5_DIGEST_SIZE) == 0 ? 0 : -1;
}

static void boot_ota_reset_file_runtime_state(ota_file_t *file)
{
    if (file == NULL) {
        return;
    }

    file->file_size = 0;
    file->matched = false;
    file->verified = false;
    file->written = false;
}

static void boot_ota_reset_param_runtime_state(ota_param_t *param)
{
    uint32_t i;

    if (param == NULL) {
        return;
    }

    for (i = 0; i < param->file_count; ++i) {
        boot_ota_reset_file_runtime_state(&param->files[i]);
    }
}

static int boot_ota_verify_validation_complete(const ota_param_t *param)
{
    uint32_t i;

    if (param == NULL || param->file_count == 0U) {
        boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                             UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
        return -1;
    }

    for (i = 0; i < param->file_count; ++i) {
        if (!param->files[i].matched) {
            printk("ota: missing image %s\n", param->files[i].path_match);
            boot_ota_failure_set(UBOOT_OTA_FAILURE_IMAGE_MISSING,
                                 UBOOT_OTA_FAILURE_DETAIL_IMAGE_ENTRY_MISSING);
            return -1;
        }
        if (!param->files[i].verified) {
            printk("ota: md5 not verified %s\n", param->files[i].path_match);
            boot_ota_failure_set(UBOOT_OTA_FAILURE_IMAGE_VERIFY_FAILED,
                                 UBOOT_OTA_FAILURE_DETAIL_IMAGE_MD5_NOT_VERIFIED);
            return -1;
        }
    }

    return 0;
}

static int boot_ota_verify_write_complete(const ota_param_t *param)
{
    uint32_t i;

    if (param == NULL || param->file_count == 0U) {
        boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                             UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
        return -1;
    }

    for (i = 0; i < param->file_count; ++i) {
        if (!param->files[i].matched) {
            printk("ota: missing image %s\n", param->files[i].path_match);
            boot_ota_failure_set(UBOOT_OTA_FAILURE_IMAGE_MISSING,
                                 UBOOT_OTA_FAILURE_DETAIL_IMAGE_ENTRY_MISSING);
            return -1;
        }
        if (!param->files[i].written) {
            printk("ota: incomplete image %s\n", param->files[i].path_match);
            boot_ota_failure_set(UBOOT_OTA_FAILURE_APPLY_FAILED,
                                 UBOOT_OTA_FAILURE_DETAIL_IMAGE_INCOMPLETE);
            return -1;
        }
    }

    return 0;
}

static __boot_ota_ramcode__ bool tar_header_is_zero(const tar_blk_t *tar)
{
    int i;

    for (i = 0; i < TAR_BLOCK_SIZE; i++) {
        if ((uint8_t)tar->dat[i] != 0U) {
            return false;
        }
    }

    return true;
}

static __boot_ota_ramcode__ uint32_t tar_header_checksum_calc(const tar_blk_t *tar)
{
    uint32_t sum = 0;
    int i;

    for (i = 0; i < TAR_BLOCK_SIZE; i++) {
        if (i >= 148 && i < 156) {
            sum += (uint8_t)' ';
        } else {
            sum += (uint8_t)tar->dat[i];
        }
    }

    return sum;
}

static __boot_ota_ramcode__ bool tar_header_is_valid(const tar_blk_t *tar)
{
    int stored_sum;

    if (tar == NULL || tar_header_is_zero(tar)) {
        return false;
    }

    stored_sum = oct_to_int(tar->hdr.chksum, sizeof(tar->hdr.chksum));
    if (stored_sum < 0) {
        return false;
    }

    if ((uint32_t)stored_sum != tar_header_checksum_calc(tar)) {
        return false;
    }

    return memcmp(tar->hdr.magic, "ustar", 5) == 0;
}

static __boot_ota_ramcode__ boot_ota_package_format_t boot_ota_detect_package_format(boot_ota_source_t *source)
{
    uint8_t header[TAR_BLOCK_SIZE] = {0};

    if (source == NULL || source->size < OTA_XZ_MAGIC_SIZE) {
        return BOOT_OTA_PACKAGE_FORMAT_UNKNOWN;
    }

    if (source->size >= TAR_BLOCK_SIZE) {
        if (boot_ota_read_from_source(source, 0, header, TAR_BLOCK_SIZE) != 0) {
            return BOOT_OTA_PACKAGE_FORMAT_UNKNOWN;
        }
    } else {
        if (boot_ota_read_from_source(source, 0, header, source->size) != 0) {
            return BOOT_OTA_PACKAGE_FORMAT_UNKNOWN;
        }
    }

    if (memcmp(header, g_boot_ota_xz_magic, OTA_XZ_MAGIC_SIZE) == 0) {
        return BOOT_OTA_PACKAGE_FORMAT_TXZ;
    }

    if (source->size >= TAR_BLOCK_SIZE && tar_header_is_valid((const tar_blk_t *)header)) {
        return BOOT_OTA_PACKAGE_FORMAT_TAR;
    }

    return BOOT_OTA_PACKAGE_FORMAT_UNKNOWN;
}

static __boot_ota_ramcode__ int boot_ota_prepare_param(const boot_ota_source_t *source, ota_param_t *param)
{
    if (source == NULL || param == NULL || source->size == 0) {
        return -1;
    }

    memset(param, 0, sizeof(*param));
    param->src_addr = source->kind == BOOT_OTA_SOURCE_KIND_FLASH ? source->flash.addr : 0U;
    param->src_size = source->size;
    return 0;
}

static void boot_ota_tar_state_release_manifest_buffer(void)
{
    if (g_tar_state.manifest_buffer != NULL) {
        free(g_tar_state.manifest_buffer);
        g_tar_state.manifest_buffer = NULL;
    }
}

static void boot_ota_tar_state_release_hash(void)
{
    memset(&g_tar_state.current_md5, 0, sizeof(g_tar_state.current_md5));
    g_tar_state.current_md5_active = false;
}

static __boot_ota_ramcode__ void boot_ota_tar_state_reset(ota_param_t *param,
                                                          boot_ota_process_mode_t mode,
                                                          bool manifest_loaded)
{
    boot_ota_tar_state_release_manifest_buffer();
    boot_ota_tar_state_release_hash();
    memset(&g_tar_state, 0, sizeof(g_tar_state));
    g_tar_state.param = param;
    g_tar_state.mode = mode;
    g_tar_state.is_header = true;
    g_tar_state.manifest_loaded = manifest_loaded;
    g_tar_state.current_kind = BOOT_OTA_TAR_FILE_SKIP;
}

static __boot_ota_ramcode__ int boot_ota_write_chunked(const ota_file_t *file, int write_pos,
                                                       const uint8_t *data, int size)
{
    int offset = 0;

    while (offset < size) {
        int chunk = size - offset;
        int ret;

        if (chunk > TAR_BLOCK_SIZE) {
            chunk = TAR_BLOCK_SIZE;
        }

        ret = boot_nvs_write(file->part_base + write_pos + offset, data + offset, chunk);
        if (ret != 0) {
            return ret;
        }

        offset += chunk;
    }

    return 0;
}

static __boot_ota_ramcode__ int boot_ota_validate_image_md5(const ota_file_t *file,
                                                            const uint8_t digest[16])
{
    char actual_hex[33];
    char expected_hex[33];

    if (file == NULL || digest == NULL) {
        boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                             UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
        return -1;
    }

    if (memcmp(digest, file->expected_md5, sizeof(file->expected_md5)) != 0) {
        boot_ota_md5_to_hex(file->expected_md5, expected_hex);
        boot_ota_md5_to_hex(digest, actual_hex);
        printk("ota: md5 mismatch %s expect=%s actual=%s\n",
               file->path_match, expected_hex, actual_hex);
        boot_ota_failure_set(UBOOT_OTA_FAILURE_IMAGE_VERIFY_FAILED,
                             UBOOT_OTA_FAILURE_DETAIL_IMAGE_MD5_MISMATCH);
        return -1;
    }

    printk("ota: md5 ok %s\n", file->path_match);
    return 0;
}

static __boot_ota_ramcode__ int boot_ota_process_image_begin(tar_state_t *state, const char *path)
{
    ota_file_t *file;

    if (state == NULL || path == NULL) {
        boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                             UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
        return -1;
    }

    file = boot_ota_find_file(state->param, path);
    if (file == NULL) {
        printk("ota: skip %s (%d bytes)\n", path, state->remaining_size);
        state->current_kind = BOOT_OTA_TAR_FILE_SKIP;
        state->current_file = NULL;
        return 0;
    }

    if (file->matched) {
        printk("ota: duplicate image %s\n", path);
        boot_ota_failure_set(UBOOT_OTA_FAILURE_PACKAGE_INVALID,
                             UBOOT_OTA_FAILURE_DETAIL_TAR_PARSE_FAILED);
        return -1;
    }
    if ((uint32_t)state->remaining_size > file->part_size) {
        printk("ota: image too large %s (%d > %u)\n", path, state->remaining_size, file->part_size);
        boot_ota_failure_set(UBOOT_OTA_FAILURE_APPLY_FAILED,
                             UBOOT_OTA_FAILURE_DETAIL_FLASH_RANGE_INVALID);
        return -1;
    }

    file->matched = true;
    file->file_size = (uint32_t)state->remaining_size;
    state->current_kind = BOOT_OTA_TAR_FILE_IMAGE;
    state->current_file = file;

    printk("ota: match %s -> 0x%08x (%d bytes)\n", path, file->part_base, state->remaining_size);

    if (state->mode == BOOT_OTA_PROCESS_VALIDATE) {
        boot_md5_init(&state->current_md5);
        state->current_md5_active = true;
        return 0;
    }

    if (boot_ota_flash_matches_expected(file) == 0) {
        printk("ota: skip %s (flash already matches)\n", path);
        file->written = true;
        state->current_kind = BOOT_OTA_TAR_FILE_SKIP;
        state->current_file = NULL;
        return 0;
    }

    /* 擦除延后到 image_data 按 sector 边擦边写，避免整分区一次擦导致的长时间停滞 */
    state->erased_upto = 0;
    return 0;
}

static __boot_ota_ramcode__ int boot_ota_process_image_data(tar_state_t *state,
                                                            const uint8_t *data, int size)
{
    if (state == NULL || state->current_file == NULL || data == NULL || size < 0) {
        boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                             UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
        return -1;
    }

    if (state->mode == BOOT_OTA_PROCESS_VALIDATE) {
        if (!state->current_md5_active) {
            boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                                 UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
            return -1;
        }
        boot_md5_update(&state->current_md5, data, (size_t)size);
        state->write_pos += size;
        return 0;
    }

    {
        uint32_t need_end = (uint32_t)state->write_pos + (uint32_t)size;
        uint32_t need_end_aligned =
            (need_end + NVS_SECTOR_SIZE - 1U) & ~((uint32_t)NVS_SECTOR_SIZE - 1U);
        if (state->erased_upto < need_end_aligned) {
            uint32_t erase_off = state->erased_upto;
            uint32_t erase_len = need_end_aligned - erase_off;
            if (boot_nvs_erase(state->current_file->part_base + erase_off, erase_len) != 0) {
                printk("ota: erase failed at 0x%08x (+%u, %u bytes)\n",
                       state->current_file->part_base, erase_off, erase_len);
                boot_ota_failure_set(UBOOT_OTA_FAILURE_APPLY_FAILED,
                                     UBOOT_OTA_FAILURE_DETAIL_FLASH_ERASE_FAILED);
                return -1;
            }
            state->erased_upto = need_end_aligned;
        }
    }

    printk("ota: write start 0x%08x (+%d, %d bytes)\n",
           state->current_file->part_base, state->write_pos, size);
    if (boot_ota_write_chunked(state->current_file, state->write_pos, data, size) != 0) {
        printk("ota: write failed at 0x%08x (+%d, %d bytes)\n",
               state->current_file->part_base, state->write_pos, size);
        boot_ota_failure_set(UBOOT_OTA_FAILURE_APPLY_FAILED,
                             UBOOT_OTA_FAILURE_DETAIL_FLASH_WRITE_FAILED);
        return -1;
    }
    printk("ota: write done 0x%08x (+%d, %d bytes)\n",
           state->current_file->part_base, state->write_pos, size);
    state->write_pos += size;
    return 0;
}

static __boot_ota_ramcode__ int boot_ota_finish_current_entry(tar_state_t *state)
{
    uint8_t digest[16];

    if (state == NULL) {
        boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                             UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
        return -1;
    }

    if (state->current_kind == BOOT_OTA_TAR_FILE_MANIFEST && state->manifest_buffer != NULL) {
        if (state->mode == BOOT_OTA_PROCESS_VALIDATE) {
            state->manifest_buffer[state->write_pos] = '\0';
            if (boot_ota_manifest_parse_json(state->manifest_buffer, state->param) != 0) {
                printk("ota: manifest parse failed\n");
                boot_ota_failure_set(UBOOT_OTA_FAILURE_MANIFEST_INVALID,
                                     UBOOT_OTA_FAILURE_DETAIL_MANIFEST_PARSE_FAILED);
                return -1;
            }
            state->manifest_loaded = true;
        }
        boot_ota_tar_state_release_manifest_buffer();
        return 0;
    }

    if (state->current_kind != BOOT_OTA_TAR_FILE_IMAGE || state->current_file == NULL) {
        return 0;
    }

    if (state->mode == BOOT_OTA_PROCESS_VALIDATE) {
        if (!state->current_md5_active) {
            boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                                 UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
            return -1;
        }
        boot_md5_finish(&state->current_md5, digest);
        boot_ota_tar_state_release_hash();
        if (boot_ota_validate_image_md5(state->current_file, digest) != 0) {
            return -1;
        }
        state->current_file->verified = true;
        return 0;
    }

    state->current_file->written = true;
    return 0;
}

/*
 * 处理TAR数据块
 */
static __boot_ota_ramcode__ int process_tar_data(uintptr_t data_addr, uint32_t data_size)
{
    tar_state_t *state = &g_tar_state;
    uintptr_t data_end = data_addr + data_size;

    for (tar_blk_t *tar = (tar_blk_t *)data_addr;
         (uintptr_t)tar < data_end;
         tar++) {
        if (state->archive_complete) {
            return 0;
        }

        if (state->is_header) {
            if (tar_header_is_zero(tar)) {
                state->zero_block_count++;
                if (state->zero_block_count >= 2U) {
                    state->archive_complete = true;
                    printk("ota: tar eof\n");
                    return 0;
                }
                continue;
            }
            state->zero_block_count = 0;

            switch (tar->hdr.typeflag) {
            case TAR_TYPE_DIRTYPE:
                printk("ota: dir(%s)\n", tar->hdr.name);
                continue;
            case TAR_TYPE_REGTYPE:
            case TAR_TYPE_AREGTYPE:
                break;
            default:
                printk("ota: unknown type(%d)\n", tar->hdr.typeflag);
                boot_ota_failure_set(UBOOT_OTA_FAILURE_PACKAGE_INVALID,
                                     UBOOT_OTA_FAILURE_DETAIL_TAR_PARSE_FAILED);
                return -1;
            }

            state->remaining_size = oct_to_int(tar->hdr.size, 12);
            if (state->remaining_size <= 0) {
                continue;
            }

            state->current_kind = BOOT_OTA_TAR_FILE_SKIP;
            state->current_file = NULL;
            state->write_pos = 0;
            if (boot_ota_path_is_manifest(tar->hdr.name)) {
                if (state->mode == BOOT_OTA_PROCESS_APPLY) {
                    state->current_kind = BOOT_OTA_TAR_FILE_SKIP;
                } else {
                    if (state->manifest_loaded) {
                        printk("ota: duplicate manifest %s\n", tar->hdr.name);
                        boot_ota_failure_set(UBOOT_OTA_FAILURE_MANIFEST_INVALID,
                                             UBOOT_OTA_FAILURE_DETAIL_MANIFEST_PARSE_FAILED);
                        return -1;
                    }
                    state->manifest_buffer = malloc((size_t)state->remaining_size + 1U);
                    if (state->manifest_buffer == NULL) {
                        printk("ota: manifest alloc failed\n");
                        boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                                             UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
                        return -1;
                    }
                    state->current_kind = BOOT_OTA_TAR_FILE_MANIFEST;
                }
            } else if (!state->manifest_loaded) {
                printk("ota: manifest required before %s\n", tar->hdr.name);
                boot_ota_failure_set(UBOOT_OTA_FAILURE_MANIFEST_INVALID,
                                     UBOOT_OTA_FAILURE_DETAIL_MANIFEST_MISSING);
                return -1;
            } else if (boot_ota_process_image_begin(state, tar->hdr.name) != 0) {
                return -1;
            }

            state->is_header = false;
        } else {
            int left = (int)(data_end - (uintptr_t)tar);
            int to_process = (left < state->remaining_size) ? left : state->remaining_size;

            if (state->current_kind == BOOT_OTA_TAR_FILE_MANIFEST && state->manifest_buffer != NULL) {
                memcpy(state->manifest_buffer + state->write_pos, tar, (size_t)to_process);
                state->write_pos += to_process;
            } else if (state->current_kind == BOOT_OTA_TAR_FILE_IMAGE && state->current_file != NULL &&
                       state->current_file->file_size > 0) {
                if (boot_ota_process_image_data(state, (const uint8_t *)tar, to_process) != 0) {
                    return -1;
                }
            }

            state->remaining_size -= to_process;

            if (state->remaining_size <= 0) {
                if (boot_ota_finish_current_entry(state) != 0) {
                    return -1;
                }

                state->is_header = true;
                state->current_kind = BOOT_OTA_TAR_FILE_SKIP;
                state->current_file = NULL;
                state->write_pos = 0;
                {
                    int skip = (to_process + TAR_BLOCK_SIZE - 1) / TAR_BLOCK_SIZE - 1;

                    tar += skip;
                }
            } else {
                break;
            }
        }
    }

    return 0;
}

static __boot_ota_ramcode__ int boot_ota_tar_pass_from_source(boot_ota_source_t *source,
                                                              ota_param_t *param,
                                                              boot_ota_process_mode_t mode)
{
    int ret = 0;
    uint8_t *ibuf = NULL;
    uint32_t src_pos = 0;

    if (source == NULL || param == NULL || source->size == 0) {
        printk("ota: unsupported source\n");
        boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                             UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
        return -1;
    }

    boot_ota_tar_state_reset(param, mode, mode == BOOT_OTA_PROCESS_APPLY);

    ibuf = malloc(TXZ_INPUT_BUF_SIZE);
    if (ibuf == NULL) {
        printk("ota: buffer alloc failed\n");
        boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                             UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
        return -1;
    }

    while (src_pos < param->src_size) {
        uint32_t chunk = param->src_size - src_pos;

        /* 和 txz 路径一样，tar 走一遍完整包也要跨秒，逐轮喂狗 */
        boot_watchdog_feed();

        if (chunk > TXZ_INPUT_BUF_SIZE) {
            chunk = TXZ_INPUT_BUF_SIZE;
        }
        if (chunk > TAR_BLOCK_SIZE) {
            chunk -= chunk % TAR_BLOCK_SIZE;
        }
        if (chunk == 0) {
            chunk = param->src_size - src_pos;
        }

        if (boot_ota_read_from_source(source, src_pos, ibuf, chunk) != 0) {
            printk("ota: source read failed at +0x%x (%u bytes)\n", src_pos, chunk);
            boot_ota_failure_set_from_source_error(source, boot_ota_source_last_error(source));
            ret = -1;
            break;
        }

        ret = process_tar_data((uintptr_t)ibuf, chunk);
        if (ret != 0 || g_tar_state.archive_complete) {
            break;
        }

        src_pos += chunk;
    }

    if (ret == 0) {
        ret = (mode == BOOT_OTA_PROCESS_VALIDATE) ?
            boot_ota_verify_validation_complete(param) :
            boot_ota_verify_write_complete(param);
    }

    boot_ota_tar_state_release_manifest_buffer();
    free(ibuf);
    return ret;
}

static __boot_ota_ramcode__ int boot_ota_tar_update_from_source(boot_ota_source_t *source)
{
    int ret;
    ota_param_t param;

    printk("ota: tar update start\n");
    if (source == NULL || source->size == 0) {
        printk("ota: unsupported source\n");
        boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                             UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
        return -1;
    }

    if (source->kind == BOOT_OTA_SOURCE_KIND_FLASH) {
        printk("ota: src=0x%08x size=0x%x\n", source->flash.addr, source->size);
    } else {
        printk("ota: src=%s size=0x%x\n", source->tf.path, source->size);
    }

    ret = boot_ota_prepare_param(source, &param);
    if (ret != 0) {
        printk("ota: prepare param failed\n");
        boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                             UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
        return -1;
    }

    ret = boot_ota_tar_pass_from_source(source, &param, BOOT_OTA_PROCESS_VALIDATE);
    if (ret != 0) {
        return ret;
    }

    boot_ota_reset_param_runtime_state(&param);
    ret = boot_ota_tar_pass_from_source(source, &param, BOOT_OTA_PROCESS_APPLY);
    if (ret == 0) {
        printk("ota: tar update success\n");
    }
    return ret;
}

static __boot_ota_ramcode__ int boot_ota_txz_pass_from_source(boot_ota_source_t *source,
                                                              ota_param_t *param,
                                                              boot_ota_process_mode_t mode,
                                                              uint8_t *obuf)
{
    int ret = 0;
    struct xz_dec *dec = NULL;
    uint8_t *ibuf = NULL;
    struct xz_buf buf;
    uint32_t src_pos = 0;

    if (source == NULL || param == NULL || obuf == NULL || source->size == 0) {
        printk("ota: unsupported source\n");
        boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                             UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
        return -1;
    }

    boot_ota_tar_state_reset(param, mode, mode == BOOT_OTA_PROCESS_APPLY);

    dec = xz_dec_init(XZ_PREALLOC, TXZ_DICT_MAX);
    if (dec == NULL) {
        printk("ota: xz_dec_init failed\n");
        boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                             UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
        return -1;
    }

    ibuf = malloc(TXZ_INPUT_BUF_SIZE);
    if (ibuf == NULL) {
        printk("ota: buffer alloc failed\n");
        boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                             UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
        ret = -1;
        goto cleanup;
    }

    memset(&buf, 0, sizeof(buf));
    buf.in = ibuf;
    buf.out = obuf;
    buf.out_size = TXZ_OUTPUT_BUF_SIZE;

    while (1) {
        /* 解压 + tar 解析 + md5（VALIDATE 阶段）或 flash 写入（APPLY 阶段）
         * 要覆盖完整个 ota.txz，2~3MB 级别。整圈跑完远超 1s 看门狗周期，
         * 必须在驱动循环里逐轮喂狗。 */
        boot_watchdog_feed();

        if (buf.in_pos == buf.in_size) {
            uint32_t left = param->src_size - src_pos;

            if (left > 0) {
                buf.in_pos = 0;
                buf.in_size = (left < TXZ_INPUT_BUF_SIZE) ? left : TXZ_INPUT_BUF_SIZE;
                if (boot_ota_read_from_source(source, src_pos, (void *)buf.in, buf.in_size) != 0) {
                    printk("ota: source read failed at +0x%x (%u bytes)\n", src_pos, buf.in_size);
                    boot_ota_failure_set_from_source_error(source, boot_ota_source_last_error(source));
                    ret = -1;
                    break;
                }
                src_pos += buf.in_size;
#ifdef CONFIG_BOOT_DISPLAY
                /* VALIDATE 占 0-50%，APPLY 占 50-100%。两趟都跑完整个包，
                 * 合成一条从 0% 走到 100% 的进度。*/
                if (param->src_size > 0u) {
                    uint32_t phase_pct = (uint32_t)src_pos * 50u / param->src_size;
                    uint32_t total_pct = (mode == BOOT_OTA_PROCESS_APPLY)
                                             ? (50u + phase_pct)
                                             : phase_pct;
                    boot_display_ota_progress((uint8_t)total_pct);
                }
#endif
            }
        }

        {
            enum xz_ret xret = xz_dec_run(dec, &buf);

            if (xret != XZ_OK && xret != XZ_STREAM_END) {
                printk("ota: xz_dec_run error %d\n", xret);
                boot_ota_failure_set(UBOOT_OTA_FAILURE_PACKAGE_INVALID,
                                     UBOOT_OTA_FAILURE_DETAIL_PACKAGE_TRUNCATED);
                ret = -1;
                break;
            }

            if (buf.out_pos == buf.out_size) {
                ret = process_tar_data((uintptr_t)buf.out, buf.out_pos);
                if (ret != 0 || g_tar_state.archive_complete) {
                    break;
                }
                buf.out_pos = 0;
                buf.out = obuf;
                buf.out_size = TXZ_OUTPUT_BUF_SIZE;
            }

            if (xret == XZ_STREAM_END) {
                if (buf.out_pos > 0) {
                    ret = process_tar_data((uintptr_t)buf.out, buf.out_pos);
                }
                break;
            }
        }
    }

    if (ret == 0) {
        ret = (mode == BOOT_OTA_PROCESS_VALIDATE) ?
            boot_ota_verify_validation_complete(param) :
            boot_ota_verify_write_complete(param);
    }

cleanup:
    boot_ota_tar_state_release_manifest_buffer();
    if (dec != NULL) {
        xz_dec_end(dec);
    }
    if (ibuf != NULL) {
        free(ibuf);
    }

    return ret;
}

/*
 * TXZ解压升级
 */
__boot_ota_ramcode__ int boot_ota_txz_update_from_source(boot_config_t *cfg,
                                    boot_ota_source_t *source,
                                    uboot_ota_failure_info_t *failure)
{
    int ret;
    int source_error;
    ota_param_t param;
    boot_ota_package_format_t format;
    uint8_t *obuf = NULL;

    (void)cfg;

    boot_ota_failure_begin(failure, source);

    ret = boot_ota_source_open(source);
    if (ret != 0) {
        boot_ota_failure_set_from_source_error(source, ret);
        goto cleanup;
    }

    format = boot_ota_detect_package_format(source);
    if (format == BOOT_OTA_PACKAGE_FORMAT_TAR) {
        ret = boot_ota_tar_update_from_source(source);
        goto cleanup;
    }
    if (format != BOOT_OTA_PACKAGE_FORMAT_TXZ) {
        source_error = boot_ota_source_last_error(source);
        if (source_error != BOOT_OTA_SOURCE_ERR_NONE) {
            boot_ota_failure_set_from_source_error(source, source_error);
        } else {
            printk("ota: unsupported package format\n");
            boot_ota_failure_set(UBOOT_OTA_FAILURE_PACKAGE_INVALID,
                                 UBOOT_OTA_FAILURE_DETAIL_PACKAGE_FORMAT_UNSUPPORTED);
        }
        ret = -1;
        goto cleanup;
    }

    printk("ota: txz update start\n");
    if (source->kind == BOOT_OTA_SOURCE_KIND_FLASH) {
        printk("ota: src=0x%08x size=0x%x\n", source->flash.addr, source->size);
    } else {
        printk("ota: src=%s size=0x%x\n", source->tf.path, source->size);
    }

    ret = boot_ota_prepare_param(source, &param);
    if (ret != 0) {
        printk("ota: prepare param failed\n");
        boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                             UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
        ret = -1;
        goto cleanup;
    }

    obuf = inram_malloc(32, TXZ_OUTPUT_BUF_SIZE);
    if (obuf == NULL) {
        printk("ota: buffer alloc failed\n");
        boot_ota_failure_set(UBOOT_OTA_FAILURE_INTERNAL,
                             UBOOT_OTA_FAILURE_DETAIL_UNKNOWN);
        ret = -1;
        goto cleanup;
    }

    ret = boot_ota_txz_pass_from_source(source, &param, BOOT_OTA_PROCESS_VALIDATE, obuf);
    if (ret != 0) {
        goto cleanup;
    }

    boot_ota_reset_param_runtime_state(&param);
    ret = boot_ota_txz_pass_from_source(source, &param, BOOT_OTA_PROCESS_APPLY, obuf);
    if (ret == 0) {
        printk("ota: txz update success\n");
    }

cleanup:
    if (obuf != NULL) {
        inram_free(obuf);
    }
    if (source != NULL) {
        boot_ota_source_close(source);
    }
    boot_ota_failure_end();
    return ret;
}

int boot_ota_txz_update(boot_config_t *cfg, partition_t *ota_part)
{
    boot_ota_source_t source;

    if (ota_part == NULL) {
        return -1;
    }

    memset(&source, 0, sizeof(source));
    source.kind = BOOT_OTA_SOURCE_KIND_FLASH;
    source.size = ota_part->size;
    source.flash.addr = ota_part->base;

    return boot_ota_txz_update_from_source(cfg, &source, NULL);
}
