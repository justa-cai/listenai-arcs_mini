#include "unity.h"

#ifdef RUN_TEST
#undef RUN_TEST
#endif
#define RUN_TEST(TestFunc, TestLineNum) UnityDefaultTestRun(TestFunc, #TestFunc, TestLineNum)

#include <stdint.h>
#include <string.h>

#include "boot_ota_source.h"
#include "lisa_device.h"
#include "lisa_sdmmc.h"
#include "lsfs.h"

typedef struct {
    const char *path;
    const uint8_t *data;
    size_t size;
} test_file_t;

static int g_sdmmc_init_result;
static int g_sdmmc_probe_result;
static int g_disk_init_result;
static int g_lsfs_init_result;
static int g_lsfs_mount_result;
static int g_lsfs_open_result;
static int g_lsfs_close_calls;
static int g_sdmmc_probe_calls;
static int g_require_probe_before_disk_init;
static int g_sdmmc_is_ready;
static test_file_t g_file;
static lisa_device_t g_sdmmc_device;

static uboot_ota_request_t make_tf_request(const char *path)
{
    uboot_ota_request_t request;

    memset(&request, 0, sizeof(request));
    request.source = UBOOT_OTA_SOURCE_TF;
    strncpy(request.tf.path, path, sizeof(request.tf.path) - 1U);
    return request;
}

static uboot_ota_request_t make_flash_request(uint32_t flash_offset, uint32_t package_size)
{
    uboot_ota_request_t request;

    memset(&request, 0, sizeof(request));
    request.source = UBOOT_OTA_SOURCE_FLASH;
    request.package_size = package_size;
    request.flash.flash_offset = flash_offset;
    return request;
}

static void load_stub_file(const char *path, const uint8_t *data, size_t size)
{
    g_file.path = path;
    g_file.data = data;
    g_file.size = size;
}

int sdmmc_hard_init(void)
{
    return g_sdmmc_init_result;
}

int lisa_sdmmc_probe(lisa_device_t *dev)
{
    if (dev != &g_sdmmc_device) {
        return -1;
    }

    g_sdmmc_probe_calls++;
    if (g_sdmmc_probe_result == 0) {
        g_sdmmc_is_ready = 1;
    }

    return g_sdmmc_probe_result;
}

int lisa_sdmmc_status(lisa_device_t *dev)
{
    if (dev != &g_sdmmc_device) {
        return -1;
    }

    return g_sdmmc_is_ready ? LISA_SDMMC_STATUS_OK : -1;
}

lisa_device_t *lisa_device_get(const char *name)
{
    if (name == NULL || strcmp(name, "sdmmc0") != 0) {
        return NULL;
    }

    return &g_sdmmc_device;
}

int disk_init(const void *dev)
{
    (void)dev;

    if (g_require_probe_before_disk_init && !g_sdmmc_is_ready) {
        return -1;
    }

    return g_disk_init_result;
}

int lsfs_init(void)
{
    return g_lsfs_init_result;
}

int lsfs_mount(struct lsfs_mount_t *mnt)
{
    if (mnt == NULL || strcmp(mnt->mnt_point, BOOT_OTA_TF_DEFAULT_MOUNT_POINT) != 0) {
        return -1;
    }

    return g_lsfs_mount_result;
}

int lsfs_open(struct lsfs_file_t *fp, const char *file_name, lsfs_mode_t flags)
{
    (void)flags;

    if (g_lsfs_open_result != 0) {
        return g_lsfs_open_result;
    }
    if (fp == NULL || file_name == NULL || g_file.path == NULL || strcmp(file_name, g_file.path) != 0) {
        return -1;
    }

    fp->data = g_file.data;
    fp->size = g_file.size;
    fp->pos = 0;
    fp->open = 1;
    return 0;
}

int lsfs_close(struct lsfs_file_t *fp)
{
    if (fp == NULL || !fp->open) {
        return -1;
    }

    fp->open = 0;
    g_lsfs_close_calls++;
    return 0;
}

ssize_t lsfs_read(struct lsfs_file_t *fp, void *ptr, size_t size)
{
    size_t left;

    if (fp == NULL || ptr == NULL || !fp->open) {
        return -1;
    }

    left = fp->size - fp->pos;
    if (size > left) {
        size = left;
    }

    memcpy(ptr, fp->data + fp->pos, size);
    fp->pos += size;
    return (ssize_t)size;
}

int lsfs_seek(struct lsfs_file_t *fp, off_t offset, int whence)
{
    off_t base;
    off_t pos;

    if (fp == NULL || !fp->open) {
        return -1;
    }

    switch (whence) {
    case LSFS_SEEK_SET:
        base = 0;
        break;
    case LSFS_SEEK_CUR:
        base = (off_t)fp->pos;
        break;
    case LSFS_SEEK_END:
        base = (off_t)fp->size;
        break;
    default:
        return -1;
    }

    pos = base + offset;
    if (pos < 0 || (size_t)pos > fp->size) {
        return -1;
    }

    fp->pos = (size_t)pos;
    return 0;
}

off_t lsfs_tell(struct lsfs_file_t *fp)
{
    if (fp == NULL || !fp->open) {
        return -1;
    }

    return (off_t)fp->pos;
}

void setUp(void)
{
    g_sdmmc_init_result = 0;
    g_sdmmc_probe_result = 0;
    g_disk_init_result = 0;
    g_lsfs_init_result = 0;
    g_lsfs_mount_result = 0;
    g_lsfs_open_result = 0;
    g_lsfs_close_calls = 0;
    g_sdmmc_probe_calls = 0;
    g_require_probe_before_disk_init = 0;
    g_sdmmc_is_ready = 0;
    g_file.path = NULL;
    g_file.data = NULL;
    g_file.size = 0;
}

void tearDown(void)
{
}

void test_tf_request_normalizes_root_relative_path(void)
{
    uboot_ota_request_t request = make_tf_request("/download/update.txz");
    boot_ota_source_t source = {0};

    TEST_ASSERT_EQUAL_INT(0, boot_ota_source_from_request(&request, &source, NULL));
    TEST_ASSERT_EQUAL_UINT32(BOOT_OTA_SOURCE_KIND_TF, source.kind);
    TEST_ASSERT_EQUAL_STRING("/SD:/download/update.txz", source.tf.path);
}

void test_tf_request_keeps_mounted_path(void)
{
    uboot_ota_request_t request = make_tf_request("/SD:/download/update.txz");
    boot_ota_source_t source = {0};

    TEST_ASSERT_EQUAL_INT(0, boot_ota_source_from_request(&request, &source, NULL));
    TEST_ASSERT_EQUAL_STRING("/SD:/download/update.txz", source.tf.path);
}

void test_tf_request_normalizes_path_without_leading_slash(void)
{
    uboot_ota_request_t request = make_tf_request("download/update.txz");
    boot_ota_source_t source = {0};

    TEST_ASSERT_EQUAL_INT(0, boot_ota_source_from_request(&request, &source, NULL));
    TEST_ASSERT_EQUAL_STRING("/SD:/download/update.txz", source.tf.path);
}

void test_flash_request_conversion_reports_range_failure_on_address_overflow(void)
{
    uboot_ota_request_t request = make_flash_request(0xD0000000U, 0x1000U);
    boot_ota_source_t source = {0};
    uboot_ota_failure_info_t failure = {0};

    TEST_ASSERT_NOT_EQUAL(0, boot_ota_source_from_request(&request, &source, &failure));
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_SOURCE_FLASH, failure.source);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_BAD_REQUEST, failure.reason);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_DETAIL_FLASH_RANGE_INVALID, failure.detail);
}

void test_tf_request_conversion_reports_path_failure_when_normalized_path_overflows(void)
{
    uboot_ota_request_t request = make_tf_request("");
    boot_ota_source_t source = {0};
    uboot_ota_failure_info_t failure = {0};
    size_t i;

    for (i = 0; i < sizeof(request.tf.path) - 1U; ++i) {
        request.tf.path[i] = 'a';
    }
    request.tf.path[sizeof(request.tf.path) - 1U] = '\0';

    TEST_ASSERT_NOT_EQUAL(0, boot_ota_source_from_request(&request, &source, &failure));
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_SOURCE_TF, failure.source);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_BAD_REQUEST, failure.reason);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_DETAIL_TF_PATH_INVALID, failure.detail);
}

void test_open_tf_source_reports_unavailable_when_mount_fails(void)
{
    uboot_ota_request_t request = make_tf_request("download/update.txz");
    boot_ota_source_t source = {0};

    TEST_ASSERT_EQUAL_INT(0, boot_ota_source_from_request(&request, &source, NULL));
    g_lsfs_mount_result = -1;
    TEST_ASSERT_EQUAL_INT(BOOT_OTA_SOURCE_ERR_UNAVAILABLE, boot_ota_source_open(&source));
    TEST_ASSERT_EQUAL_INT(BOOT_OTA_SOURCE_ERR_UNAVAILABLE, boot_ota_source_last_error(&source));
}

void test_open_tf_source_reads_file_size_and_payload(void)
{
    static const uint8_t payload[] = "payload";
    uint8_t buffer[sizeof(payload)] = {0};
    uboot_ota_request_t request = make_tf_request("download/update.txz");
    boot_ota_source_t source = {0};

    TEST_ASSERT_EQUAL_INT(0, boot_ota_source_from_request(&request, &source, NULL));
    load_stub_file("/SD:/download/update.txz", payload, sizeof(payload) - 1U);

    TEST_ASSERT_EQUAL_INT(0, boot_ota_source_open(&source));
    TEST_ASSERT_EQUAL_UINT32(sizeof(payload) - 1U, source.size);
    TEST_ASSERT_EQUAL_INT(0, boot_ota_source_read(&source, 0, buffer, sizeof(payload) - 1U));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, buffer, sizeof(payload) - 1U);
    TEST_ASSERT_EQUAL_INT(0, boot_ota_source_close(&source));
    TEST_ASSERT_EQUAL_INT(1, g_lsfs_close_calls);
}

void test_open_tf_source_probes_sdmmc_before_disk_init(void)
{
    static const uint8_t payload[] = "payload";
    uboot_ota_request_t request = make_tf_request("download/update.txz");
    boot_ota_source_t source = {0};

    g_require_probe_before_disk_init = 1;

    TEST_ASSERT_EQUAL_INT(0, boot_ota_source_from_request(&request, &source, NULL));
    load_stub_file("/SD:/download/update.txz", payload, sizeof(payload) - 1U);

    TEST_ASSERT_EQUAL_INT(0, boot_ota_source_open(&source));
    TEST_ASSERT_EQUAL_INT(1, g_sdmmc_probe_calls);
}

int main(void)
{
    UnityBegin("system/uboot/test/uboot_ota_source/test_boot_ota_source.c");

    RUN_TEST(test_tf_request_normalizes_root_relative_path, __LINE__);
    RUN_TEST(test_tf_request_keeps_mounted_path, __LINE__);
    RUN_TEST(test_tf_request_normalizes_path_without_leading_slash, __LINE__);
    RUN_TEST(test_flash_request_conversion_reports_range_failure_on_address_overflow, __LINE__);
    RUN_TEST(test_tf_request_conversion_reports_path_failure_when_normalized_path_overflows, __LINE__);
    RUN_TEST(test_open_tf_source_reports_unavailable_when_mount_fails, __LINE__);
    RUN_TEST(test_open_tf_source_reads_file_size_and_payload, __LINE__);
    RUN_TEST(test_open_tf_source_probes_sdmmc_before_disk_init, __LINE__);

    return UnityEnd();
}
