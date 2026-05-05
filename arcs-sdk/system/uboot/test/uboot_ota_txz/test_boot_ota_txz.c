#define _GNU_SOURCE

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "boot_partab.h"
#include "boot_md5.h"
#include "boot_ota.h"
#include "tinyprintf.h"
#include "xz.h"

#define FLASH_SIZE           (8U * 1024U * 1024U)
#define OTA_OFFSET           0x00600000U
#define APP_OFFSET           0x00016000U
#define MANIFEST_AP_OFFSET   0x00026000U
#define MANIFEST_CP_OFFSET   0x00226000U
#define FLASH_WRITE_LIMIT    512U

static uint8_t g_flash[FLASH_SIZE];
static size_t g_max_write_size;
static size_t g_write_calls;
static size_t g_erase_calls;
static size_t g_inram_alloc_calls;
static size_t g_inram_alloc_limit;
static size_t g_last_inram_alloc_size;
static size_t g_inram_alloc_max_size = (size_t)-1;
static int g_force_write_failure;

void xz_crc32_init(void);
uint32_t xz_crc32(const uint8_t *buf, size_t size, uint32_t crc);

static void *low_malloc(size_t size)
{
    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);

    return ptr == MAP_FAILED ? NULL : ptr;
}

static void low_free(void *ptr)
{
    (void)ptr;
}

#define malloc low_malloc
#define free low_free

void *inram_malloc(size_t align, size_t size)
{
    (void)align;
    if (g_inram_alloc_calls >= g_inram_alloc_limit) {
        return NULL;
    }
    if (size > g_inram_alloc_max_size) {
        return NULL;
    }
    g_inram_alloc_calls++;
    g_last_inram_alloc_size = size;
    return low_malloc(size);
}

void inram_free(void *ptr)
{
    low_free(ptr);
}

void boot_nvs_read(void *dst, const void *src, size_t len)
{
    uintptr_t addr = (uintptr_t)src;

    memcpy(dst, &g_flash[addr - CONFIG_MEM_FLASH_BASE], len);
}

int boot_nvs_erase(uint32_t addr, size_t size)
{
    g_erase_calls++;
    memset(&g_flash[addr - CONFIG_MEM_FLASH_BASE], 0xFF, size);
    return 0;
}

int boot_nvs_write(uint32_t addr, const void *data, size_t size)
{
    if (size > g_max_write_size) {
        g_max_write_size = size;
    }
    g_write_calls++;
    if (g_force_write_failure) {
        return -1;
    }

    if (size > FLASH_WRITE_LIMIT) {
        return -1;
    }

    memcpy(&g_flash[addr - CONFIG_MEM_FLASH_BASE], data, size);
    return 0;
}

uint32_t crc32_calc(const void *data, uint32_t size, uint32_t last)
{
    return xz_crc32((const uint8_t *)data, size, last);
}

int printk(const char *fmt, ...)
{
    (void)fmt;
    return 0;
}

int boot_ota_source_open(boot_ota_source_t *source)
{
    if (source == NULL) {
        return BOOT_OTA_SOURCE_ERR_INVALID;
    }

    source->last_error = BOOT_OTA_SOURCE_ERR_NONE;
    return BOOT_OTA_SOURCE_ERR_NONE;
}

int boot_ota_source_close(boot_ota_source_t *source)
{
    if (source == NULL) {
        return BOOT_OTA_SOURCE_ERR_INVALID;
    }

    source->last_error = BOOT_OTA_SOURCE_ERR_NONE;
    return BOOT_OTA_SOURCE_ERR_NONE;
}

int boot_ota_source_read(boot_ota_source_t *source, uint32_t offset, void *dst, uint32_t size)
{
    (void)offset;
    (void)dst;
    (void)size;

    if (source == NULL) {
        return BOOT_OTA_SOURCE_ERR_INVALID;
    }

    source->last_error = BOOT_OTA_SOURCE_ERR_INVALID;
    return BOOT_OTA_SOURCE_ERR_INVALID;
}

int boot_ota_source_last_error(const boot_ota_source_t *source)
{
    if (source == NULL) {
        return BOOT_OTA_SOURCE_ERR_INVALID;
    }

    return source->last_error;
}

#define snprintf test_boot_snprintf

static int test_boot_snprintf(char *str, size_t size, const char *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);
    ret = tfp_vsnprintf(str, size, format, args);
    va_end(args);

    return ret;
}

#include "../../src/boot_ota.c"

#undef snprintf
#undef malloc
#undef free

static void load_file(const char *path, uint8_t **out, size_t *out_size)
{
    FILE *fp = fopen(path, "rb");
    long size;

    assert(fp != NULL);
    assert(fseek(fp, 0, SEEK_END) == 0);
    size = ftell(fp);
    assert(size >= 0);
    assert(fseek(fp, 0, SEEK_SET) == 0);

    *out = malloc((size_t)size);
    assert(*out != NULL);
    assert(fread(*out, 1, (size_t)size, fp) == (size_t)size);
    fclose(fp);

    *out_size = (size_t)size;
}

static void setup_config(boot_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->part_count = 1;
    strncpy(cfg->partitions[0].name, "AP", PART_NAME_SIZE - 1);
    cfg->partitions[0].base = CONFIG_MEM_FLASH_BASE + APP_OFFSET;
    cfg->partitions[0].size = 0x40000;
    cfg->partitions[0].exec = CONFIG_MEM_FLASH_BASE + APP_OFFSET;
    cfg->partitions[0].flags = PART_FLAG_VALID | PART_FLAG_BOOTABLE;
}

static void setup_source(boot_ota_source_t *source, size_t package_size)
{
    memset(source, 0, sizeof(*source));
    source->kind = BOOT_OTA_SOURCE_KIND_FLASH;
    source->size = (uint32_t)package_size;
    source->flash.addr = CONFIG_MEM_FLASH_BASE + OTA_OFFSET;
}

static void assert_payload_written(const char *payload_path, uint32_t flash_offset)
{
    uint8_t *payload = NULL;
    size_t payload_size = 0;

    if (payload_path == NULL) {
        return;
    }

    load_file(payload_path, &payload, &payload_size);
    assert(memcmp(&g_flash[flash_offset], payload, payload_size) == 0);
}

static void assert_region_erased(uint32_t flash_offset, size_t size)
{
    size_t i;

    for (i = 0; i < size; ++i) {
        assert(g_flash[flash_offset + i] == 0xFF);
    }
}

static void assert_md5_matches(const char *text, const char *expected_hex)
{
    boot_md5_context_t ctx;
    uint8_t digest[BOOT_MD5_DIGEST_SIZE];
    char actual_hex[BOOT_MD5_DIGEST_SIZE * 2U + 1U];
    static const char digits[] = "0123456789abcdef";
    size_t i;
    size_t len = strlen(text);

    boot_md5_init(&ctx);
    boot_md5_update(&ctx, (const uint8_t *)text, len);
    boot_md5_finish(&ctx, digest);

    for (i = 0; i < BOOT_MD5_DIGEST_SIZE; ++i) {
        actual_hex[i * 2U] = digits[digest[i] >> 4];
        actual_hex[i * 2U + 1U] = digits[digest[i] & 0x0FU];
    }
    actual_hex[sizeof(actual_hex) - 1U] = '\0';

    assert(strcmp(actual_hex, expected_hex) == 0);
}

static void test_boot_md5_matches_rfc_vectors(void)
{
    assert_md5_matches("", "d41d8cd98f00b204e9800998ecf8427e");
    assert_md5_matches("abc", "900150983cd24fb0d6963f7d28e17f72");
    assert_md5_matches("message digest", "f96b697d7cb7938d525a2f31aaf161d0");
}

static int run_update_case_with_source_size(const char *ap_payload_path, uint32_t ap_flash_offset,
                                            const char *cp_payload_path, uint32_t cp_flash_offset,
                                            const char *package_path, size_t source_size,
                                            size_t inram_alloc_limit,
                                            uboot_ota_failure_info_t *failure)
{
    uint8_t *package = NULL;
    size_t package_size = 0;
    boot_config_t cfg = {0};
    boot_ota_source_t source = {0};

    memset(g_flash, 0xFF, sizeof(g_flash));
    g_max_write_size = 0;
    g_write_calls = 0;
    g_erase_calls = 0;
    g_inram_alloc_calls = 0;
    g_inram_alloc_limit = inram_alloc_limit;
    g_last_inram_alloc_size = 0;

    load_file(package_path, &package, &package_size);
    assert(source_size >= package_size);

    memcpy(&g_flash[OTA_OFFSET], package, package_size);

    setup_config(&cfg);
    setup_source(&source, source_size);

    xz_crc32_init();
    int ret = boot_ota_txz_update_from_source(&cfg, &source, failure);

    if (ret == 0) {
        assert_payload_written(ap_payload_path, ap_flash_offset);
        assert_payload_written(cp_payload_path, cp_flash_offset);
    }
    return ret;
}

static int run_update_case(const char *ap_payload_path, uint32_t ap_flash_offset,
                           const char *cp_payload_path, uint32_t cp_flash_offset,
                           const char *package_path, size_t inram_alloc_limit,
                           uboot_ota_failure_info_t *failure)
{
    uint8_t *package = NULL;
    size_t package_size = 0;

    load_file(package_path, &package, &package_size);
    return run_update_case_with_source_size(
        ap_payload_path,
        ap_flash_offset,
        cp_payload_path,
        cp_flash_offset,
        package_path,
        package_size,
        inram_alloc_limit,
        failure
    );
}

static void test_txz_update_chunks_flash_writes_and_preserves_small_payload(void)
{
    int ret = run_update_case(
        UBOOT_OTA_TXZ_TEST_SMALL_PAYLOAD_PATH,
        MANIFEST_AP_OFFSET,
        UBOOT_OTA_TXZ_TEST_SMALL_CP_PAYLOAD_PATH,
        MANIFEST_CP_OFFSET,
        UBOOT_OTA_TXZ_TEST_SMALL_TXZ_PATH,
        (size_t)-1,
        NULL
    );

    assert(ret == 0);
    assert(g_inram_alloc_calls >= 1);
    assert(g_write_calls > 0);
    assert(g_max_write_size <= FLASH_WRITE_LIMIT);
}

static void test_tar_update_preserves_small_payload_without_xz_decode(void)
{
    int ret = run_update_case(
        UBOOT_OTA_TXZ_TEST_SMALL_PAYLOAD_PATH,
        MANIFEST_AP_OFFSET,
        UBOOT_OTA_TXZ_TEST_SMALL_CP_PAYLOAD_PATH,
        MANIFEST_CP_OFFSET,
        UBOOT_OTA_TXZ_TEST_SMALL_TAR_PATH,
        (size_t)-1,
        NULL
    );

    assert(ret == 0);
    assert(g_write_calls > 0);
    assert(g_max_write_size <= FLASH_WRITE_LIMIT);
}

static void test_txz_update_preserves_large_payload_across_output_refills(void)
{
    int ret = run_update_case(
        UBOOT_OTA_TXZ_TEST_LARGE_PAYLOAD_PATH,
        MANIFEST_AP_OFFSET,
        NULL,
        0,
        UBOOT_OTA_TXZ_TEST_LARGE_TXZ_PATH,
        (size_t)-1,
        NULL
    );

    assert(ret == 0);
    assert(g_inram_alloc_calls >= 1);
    assert(g_write_calls > 0);
    assert(g_max_write_size <= FLASH_WRITE_LIMIT);
}

static void test_tar_update_preserves_large_payload_across_flash_reads(void)
{
    int ret = run_update_case(
        UBOOT_OTA_TXZ_TEST_LARGE_PAYLOAD_PATH,
        MANIFEST_AP_OFFSET,
        NULL,
        0,
        UBOOT_OTA_TXZ_TEST_LARGE_TAR_PATH,
        (size_t)-1,
        NULL
    );

    assert(ret == 0);
    assert(g_write_calls > 0);
    assert(g_max_write_size <= FLASH_WRITE_LIMIT);
}

static void test_txz_update_succeeds_with_single_internal_write_buffer(void)
{
    int ret = run_update_case(
        UBOOT_OTA_TXZ_TEST_SMALL_PAYLOAD_PATH,
        MANIFEST_AP_OFFSET,
        UBOOT_OTA_TXZ_TEST_SMALL_CP_PAYLOAD_PATH,
        MANIFEST_CP_OFFSET,
        UBOOT_OTA_TXZ_TEST_SMALL_TXZ_PATH,
        1,
        NULL
    );

    assert(ret == 0);
    assert(g_inram_alloc_calls == 1);
    assert(g_last_inram_alloc_size == TXZ_OUTPUT_BUF_SIZE);
    assert(g_write_calls > 0);
    assert(g_max_write_size <= FLASH_WRITE_LIMIT);
}

static void test_txz_update_succeeds_when_inram_cannot_provide_128k_chunk(void)
{
    int ret;

    g_inram_alloc_max_size = 32U * 1024U;
    ret = run_update_case(
        UBOOT_OTA_TXZ_TEST_SMALL_PAYLOAD_PATH,
        MANIFEST_AP_OFFSET,
        UBOOT_OTA_TXZ_TEST_SMALL_CP_PAYLOAD_PATH,
        MANIFEST_CP_OFFSET,
        UBOOT_OTA_TXZ_TEST_SMALL_TXZ_PATH,
        (size_t)-1,
        NULL
    );

    assert(ret == 0);
    assert(g_inram_alloc_calls >= 1);
    assert(g_last_inram_alloc_size <= g_inram_alloc_max_size);
    assert(g_write_calls > 0);
    g_inram_alloc_max_size = (size_t)-1;
}

static void test_tar_update_matches_lowercase_bin_name(void)
{
    int ret = run_update_case(
        UBOOT_OTA_TXZ_TEST_SMALL_PAYLOAD_PATH,
        MANIFEST_AP_OFFSET,
        NULL,
        0,
        UBOOT_OTA_TXZ_TEST_LOWERCASE_TAR_PATH,
        (size_t)-1,
        NULL
    );

    assert(ret == 0);
    assert(g_write_calls > 0);
}

static void test_tar_update_allows_unknown_file_before_image(void)
{
    int ret = run_update_case(
        UBOOT_OTA_TXZ_TEST_SMALL_PAYLOAD_PATH,
        MANIFEST_AP_OFFSET,
        NULL,
        0,
        UBOOT_OTA_TXZ_TEST_JUNK_FIRST_TAR_PATH,
        (size_t)-1,
        NULL
    );

    assert(ret == 0);
    assert(g_write_calls > 0);
}

static void test_tar_update_uses_manifest_target_address_instead_of_partab_mapping(void)
{
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    int ret = run_update_case(
        UBOOT_OTA_TXZ_TEST_SMALL_PAYLOAD_PATH,
        MANIFEST_AP_OFFSET,
        UBOOT_OTA_TXZ_TEST_SMALL_CP_PAYLOAD_PATH,
        MANIFEST_CP_OFFSET,
        UBOOT_OTA_TXZ_TEST_SMALL_TAR_PATH,
        (size_t)-1,
        NULL
    );

    assert(ret == 0);
    assert(g_write_calls > 0);

    load_file(UBOOT_OTA_TXZ_TEST_SMALL_PAYLOAD_PATH, &payload, &payload_size);
    assert(memcmp(&g_flash[APP_OFFSET], payload, payload_size) != 0);
}

static void test_tar_update_fails_when_config_json_is_missing(void)
{
    int ret = run_update_case(
        NULL,
        0,
        NULL,
        0,
        UBOOT_OTA_TXZ_TEST_MISSING_CONFIG_TAR_PATH,
        (size_t)-1,
        NULL
    );

    assert(ret != 0);
    assert(g_write_calls == 0);
}

static void test_tar_update_rejects_oversized_partition_image(void)
{
    int ret = run_update_case(
        NULL,
        0,
        NULL,
        0,
        UBOOT_OTA_TXZ_TEST_OVERSIZE_TAR_PATH,
        (size_t)-1,
        NULL
    );

    assert(ret != 0);
    assert(g_write_calls == 0);
}

static void test_tar_update_accepts_partition_sized_source_after_tar_eof(void)
{
    int ret = run_update_case_with_source_size(
        UBOOT_OTA_TXZ_TEST_SMALL_PAYLOAD_PATH,
        MANIFEST_AP_OFFSET,
        UBOOT_OTA_TXZ_TEST_SMALL_CP_PAYLOAD_PATH,
        MANIFEST_CP_OFFSET,
        UBOOT_OTA_TXZ_TEST_SMALL_TAR_PATH,
        0x40000,
        (size_t)-1,
        NULL
    );

    assert(ret == 0);
    assert(g_write_calls > 0);
}

static void test_tar_update_rejects_package_when_any_image_md5_mismatches(void)
{
    int ret = run_update_case(
        NULL,
        0,
        NULL,
        0,
        UBOOT_OTA_TXZ_TEST_BAD_MD5_TAR_PATH,
        (size_t)-1,
        NULL
    );

    assert(ret != 0);
    assert(g_erase_calls == 0);
    assert(g_write_calls == 0);
    assert_region_erased(MANIFEST_AP_OFFSET, FIXTURE_SMALL_SIZE);
    assert_region_erased(MANIFEST_CP_OFFSET, FIXTURE_SMALL_CP_SIZE);
}

static void test_txz_update_rejects_package_when_any_image_md5_mismatches(void)
{
    int ret = run_update_case(
        NULL,
        0,
        NULL,
        0,
        UBOOT_OTA_TXZ_TEST_BAD_MD5_TXZ_PATH,
        (size_t)-1,
        NULL
    );

    assert(ret != 0);
    assert(g_erase_calls == 0);
    assert(g_write_calls == 0);
    assert_region_erased(MANIFEST_AP_OFFSET, FIXTURE_SMALL_SIZE);
    assert_region_erased(MANIFEST_CP_OFFSET, FIXTURE_SMALL_CP_SIZE);
}

static void test_tar_update_reports_manifest_missing_failure(void)
{
    uboot_ota_failure_info_t failure = {0};
    int ret = run_update_case(
        NULL,
        0,
        NULL,
        0,
        UBOOT_OTA_TXZ_TEST_MISSING_CONFIG_TAR_PATH,
        (size_t)-1,
        &failure
    );

    assert(ret != 0);
    assert(failure.source == UBOOT_OTA_SOURCE_FLASH);
    assert(failure.reason == UBOOT_OTA_FAILURE_MANIFEST_INVALID);
    assert(failure.detail == UBOOT_OTA_FAILURE_DETAIL_MANIFEST_MISSING);
}

static void test_tar_update_reports_md5_mismatch_failure(void)
{
    uboot_ota_failure_info_t failure = {0};
    int ret = run_update_case(
        NULL,
        0,
        NULL,
        0,
        UBOOT_OTA_TXZ_TEST_BAD_MD5_TAR_PATH,
        (size_t)-1,
        &failure
    );

    assert(ret != 0);
    assert(failure.source == UBOOT_OTA_SOURCE_FLASH);
    assert(failure.reason == UBOOT_OTA_FAILURE_IMAGE_VERIFY_FAILED);
    assert(failure.detail == UBOOT_OTA_FAILURE_DETAIL_IMAGE_MD5_MISMATCH);
}

static void test_update_reports_package_format_failure_for_unknown_input(void)
{
    boot_config_t cfg = {0};
    boot_ota_source_t source = {0};
    uboot_ota_failure_info_t failure = {0};
    int ret;

    memset(g_flash, 0xA5, sizeof(g_flash));
    setup_config(&cfg);
    setup_source(&source, 64U);

    ret = boot_ota_txz_update_from_source(&cfg, &source, &failure);

    assert(ret != 0);
    assert(failure.source == UBOOT_OTA_SOURCE_FLASH);
    assert(failure.reason == UBOOT_OTA_FAILURE_PACKAGE_INVALID);
    assert(failure.detail == UBOOT_OTA_FAILURE_DETAIL_PACKAGE_FORMAT_UNSUPPORTED);
}

static void test_tar_update_skips_erase_and_write_when_flash_already_matches(void)
{
    uint8_t *package = NULL;
    size_t package_size = 0;
    uint8_t *ap_payload = NULL;
    size_t ap_payload_size = 0;
    uint8_t *cp_payload = NULL;
    size_t cp_payload_size = 0;
    boot_config_t cfg = {0};
    boot_ota_source_t source = {0};

    load_file(UBOOT_OTA_TXZ_TEST_SMALL_TAR_PATH, &package, &package_size);
    load_file(UBOOT_OTA_TXZ_TEST_SMALL_PAYLOAD_PATH, &ap_payload, &ap_payload_size);
    load_file(UBOOT_OTA_TXZ_TEST_SMALL_CP_PAYLOAD_PATH, &cp_payload, &cp_payload_size);

    memset(g_flash, 0xFF, sizeof(g_flash));
    g_max_write_size = 0;
    g_write_calls = 0;
    g_erase_calls = 0;
    g_inram_alloc_calls = 0;
    g_inram_alloc_limit = (size_t)-1;
    g_last_inram_alloc_size = 0;

    memcpy(&g_flash[OTA_OFFSET], package, package_size);
    /* Pre-seed both target partitions with the exact bytes the manifest expects. */
    memcpy(&g_flash[MANIFEST_AP_OFFSET], ap_payload, ap_payload_size);
    memcpy(&g_flash[MANIFEST_CP_OFFSET], cp_payload, cp_payload_size);

    setup_config(&cfg);
    setup_source(&source, package_size);

    xz_crc32_init();
    assert(boot_ota_txz_update_from_source(&cfg, &source, NULL) == 0);

    assert(g_erase_calls == 0);
    assert(g_write_calls == 0);
    assert_payload_written(UBOOT_OTA_TXZ_TEST_SMALL_PAYLOAD_PATH, MANIFEST_AP_OFFSET);
    assert_payload_written(UBOOT_OTA_TXZ_TEST_SMALL_CP_PAYLOAD_PATH, MANIFEST_CP_OFFSET);
}

static void test_tar_update_skips_matching_image_and_writes_the_other(void)
{
    uint8_t *package = NULL;
    size_t package_size = 0;
    uint8_t *ap_payload = NULL;
    size_t ap_payload_size = 0;
    boot_config_t cfg = {0};
    boot_ota_source_t source = {0};

    load_file(UBOOT_OTA_TXZ_TEST_SMALL_TAR_PATH, &package, &package_size);
    load_file(UBOOT_OTA_TXZ_TEST_SMALL_PAYLOAD_PATH, &ap_payload, &ap_payload_size);

    memset(g_flash, 0xFF, sizeof(g_flash));
    g_max_write_size = 0;
    g_write_calls = 0;
    g_erase_calls = 0;
    g_inram_alloc_calls = 0;
    g_inram_alloc_limit = (size_t)-1;
    g_last_inram_alloc_size = 0;

    memcpy(&g_flash[OTA_OFFSET], package, package_size);
    /* Only AP matches; CP stays 0xFF so it must still be erased + written. */
    memcpy(&g_flash[MANIFEST_AP_OFFSET], ap_payload, ap_payload_size);

    setup_config(&cfg);
    setup_source(&source, package_size);

    xz_crc32_init();
    assert(boot_ota_txz_update_from_source(&cfg, &source, NULL) == 0);

    assert(g_erase_calls == 1);
    assert(g_write_calls > 0);
    assert_payload_written(UBOOT_OTA_TXZ_TEST_SMALL_PAYLOAD_PATH, MANIFEST_AP_OFFSET);
    assert_payload_written(UBOOT_OTA_TXZ_TEST_SMALL_CP_PAYLOAD_PATH, MANIFEST_CP_OFFSET);
}

static void test_tar_update_reports_flash_write_failure(void)
{
    uboot_ota_failure_info_t failure = {0};
    int ret;

    g_force_write_failure = 1;
    ret = run_update_case(
        NULL,
        0,
        NULL,
        0,
        UBOOT_OTA_TXZ_TEST_SMALL_TAR_PATH,
        (size_t)-1,
        &failure
    );
    g_force_write_failure = 0;

    assert(ret != 0);
    assert(failure.source == UBOOT_OTA_SOURCE_FLASH);
    assert(failure.reason == UBOOT_OTA_FAILURE_APPLY_FAILED);
    assert(failure.detail == UBOOT_OTA_FAILURE_DETAIL_FLASH_WRITE_FAILED);
}

int main(void)
{
    test_boot_md5_matches_rfc_vectors();
    test_txz_update_chunks_flash_writes_and_preserves_small_payload();
    test_tar_update_preserves_small_payload_without_xz_decode();
    test_txz_update_preserves_large_payload_across_output_refills();
    test_tar_update_preserves_large_payload_across_flash_reads();
    test_txz_update_succeeds_with_single_internal_write_buffer();
    test_txz_update_succeeds_when_inram_cannot_provide_128k_chunk();
    test_tar_update_matches_lowercase_bin_name();
    test_tar_update_allows_unknown_file_before_image();
    test_tar_update_uses_manifest_target_address_instead_of_partab_mapping();
    test_tar_update_fails_when_config_json_is_missing();
    test_tar_update_rejects_oversized_partition_image();
    test_tar_update_accepts_partition_sized_source_after_tar_eof();
    test_tar_update_rejects_package_when_any_image_md5_mismatches();
    test_txz_update_rejects_package_when_any_image_md5_mismatches();
    test_tar_update_reports_manifest_missing_failure();
    test_tar_update_reports_md5_mismatch_failure();
    test_update_reports_package_format_failure_for_unknown_input();
    test_tar_update_skips_erase_and_write_when_flash_already_matches();
    test_tar_update_skips_matching_image_and_writes_the_other();
    test_tar_update_reports_flash_write_failure();
    puts("uboot_ota_txz_tests: PASS");
    return 0;
}
