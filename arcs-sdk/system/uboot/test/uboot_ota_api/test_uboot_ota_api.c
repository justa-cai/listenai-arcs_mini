/*=======Test Runner Used To Run Each Test Below=====*/
#define RUN_TEST(TestFunc, TestLineNum)                                                                                \
    {                                                                                                                  \
        Unity.CurrentTestName = #TestFunc;                                                                             \
        Unity.CurrentTestLineNumber = TestLineNum;                                                                     \
        Unity.NumberOfTests++;                                                                                         \
        if (TEST_PROTECT()) {                                                                                          \
            setUp();                                                                                                   \
            TestFunc();                                                                                                \
        }                                                                                                              \
        if (TEST_PROTECT()) {                                                                                          \
            tearDown();                                                                                                \
        }                                                                                                              \
        UnityConcludeTest();                                                                                           \
    }

#include "unity.h"
#include <stdint.h>
#include <string.h>

#include "boot_ota_source.h"
#include "boot_ota_lifecycle.h"
#include "boot_ota_request.h"
#include "boot_control_store_test.h"
#include "uboot_ota_api.h"

typedef struct {
    int call_count;
    uboot_ota_request_t last_request;
    uboot_ota_failure_info_t report_failure;
} ota_handler_ctx_t;

static boot_config_t g_partab_config;
static const boot_config_t *g_partab_config_ptr;

const boot_config_t *uboot_ota_partab_get(void)
{
    return g_partab_config_ptr;
}

static int ota_handler_success(const uboot_ota_request_t *req,
                               uboot_ota_failure_info_t *failure,
                               void *ctx)
{
    ota_handler_ctx_t *handler_ctx = ctx;

    (void)failure;
    handler_ctx->call_count++;
    handler_ctx->last_request = *req;
    return 0;
}

static int ota_handler_failure(const uboot_ota_request_t *req,
                               uboot_ota_failure_info_t *failure,
                               void *ctx)
{
    ota_handler_ctx_t *handler_ctx = ctx;

    handler_ctx->call_count++;
    handler_ctx->last_request = *req;
    *failure = handler_ctx->report_failure;
    return -1;
}

static int ota_handler_discard(const uboot_ota_request_t *req,
                               uboot_ota_failure_info_t *failure,
                               void *ctx)
{
    ota_handler_ctx_t *handler_ctx = ctx;

    handler_ctx->call_count++;
    handler_ctx->last_request = *req;
    *failure = handler_ctx->report_failure;
    return BOOT_OTA_LIFECYCLE_FAILED_DISCARD_REQUEST;
}

void setUp(void)
{
    boot_control_store_test_reset();
    boot_ota_request_clear();
    memset(&g_partab_config, 0, sizeof(g_partab_config));
    g_partab_config_ptr = NULL;
}

void tearDown(void)
{
}

void test_last_failure_defaults_to_none(void)
{
    uboot_ota_failure_info_t info;

    memset(&info, 0xFF, sizeof(info));
    TEST_ASSERT_EQUAL_INT(0, uboot_ota_get_last_failure(&info));
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_NONE, info.reason);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_DETAIL_NONE, info.detail);
}

void test_start_does_not_clear_previous_failure(void)
{
    uboot_ota_failure_info_t info = {
        .source = UBOOT_OTA_SOURCE_TF,
        .reason = UBOOT_OTA_FAILURE_SOURCE_UNAVAILABLE,
        .detail = UBOOT_OTA_FAILURE_DETAIL_TF_OPEN_FAILED,
    };

    TEST_ASSERT_EQUAL_INT(0, boot_control_store_set_failure(&info));
    TEST_ASSERT_EQUAL_INT(0, uboot_ota_start_from_flash(0x26000, 0x2800));
    memset(&info, 0, sizeof(info));
    TEST_ASSERT_EQUAL_INT(0, uboot_ota_get_last_failure(&info));
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_SOURCE_TF, info.source);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_SOURCE_UNAVAILABLE, info.reason);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_DETAIL_TF_OPEN_FAILED, info.detail);
}

void test_request_clear_preserves_previous_failure(void)
{
    uboot_ota_failure_info_t info = {
        .source = UBOOT_OTA_SOURCE_TF,
        .reason = UBOOT_OTA_FAILURE_SOURCE_UNAVAILABLE,
        .detail = UBOOT_OTA_FAILURE_DETAIL_TF_OPEN_FAILED,
    };
    uboot_ota_request_t loaded = {0};

    TEST_ASSERT_EQUAL_INT(0, boot_control_store_set_failure(&info));
    TEST_ASSERT_EQUAL_INT(0, uboot_ota_start_from_flash(0x28000, 0x3000));
    TEST_ASSERT_EQUAL_INT(0, boot_ota_request_clear());
    TEST_ASSERT_NOT_EQUAL(0, boot_ota_request_load(&loaded));

    memset(&info, 0, sizeof(info));
    TEST_ASSERT_EQUAL_INT(0, uboot_ota_get_last_failure(&info));
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_SOURCE_TF, info.source);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_SOURCE_UNAVAILABLE, info.reason);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_DETAIL_TF_OPEN_FAILED, info.detail);
}

void test_clear_failure_resets_last_failure(void)
{
    uboot_ota_failure_info_t info = {
        .source = UBOOT_OTA_SOURCE_TF,
        .reason = UBOOT_OTA_FAILURE_SOURCE_UNAVAILABLE,
        .detail = UBOOT_OTA_FAILURE_DETAIL_TF_OPEN_FAILED,
    };

    TEST_ASSERT_EQUAL_INT(0, boot_control_store_set_failure(&info));
    TEST_ASSERT_EQUAL_INT(0, boot_control_store_clear_failure());

    memset(&info, 0xFF, sizeof(info));
    TEST_ASSERT_EQUAL_INT(0, uboot_ota_get_last_failure(&info));
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_NONE, info.reason);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_DETAIL_NONE, info.detail);
}

static void set_partab_partition(const char *name, uint32_t base, uint32_t size, uint32_t flags)
{
    memset(&g_partab_config, 0, sizeof(g_partab_config));
    g_partab_config.magic = PARTAB_MAGIC;
    g_partab_config.version = PARTAB_VERSION;
    g_partab_config.size = sizeof(g_partab_config);
    g_partab_config.part_count = 1;
    strncpy(g_partab_config.partitions[0].name, name, PART_NAME_SIZE - 1);
    g_partab_config.partitions[0].base = base;
    g_partab_config.partitions[0].size = size;
    g_partab_config.partitions[0].flags = flags;
    g_partab_config_ptr = &g_partab_config;
}

void test_accepts_valid_flash_request(void)
{
    uboot_ota_request_t req = {
        .source = UBOOT_OTA_SOURCE_FLASH,
        .package_size = 0x4000,
        .flash = {
            .flash_offset = 0x20000,
        },
    };
    uboot_ota_request_t loaded = {0};

    TEST_ASSERT_EQUAL_INT(0, uboot_ota_start(&req));
    TEST_ASSERT_EQUAL_INT(0, boot_ota_request_load(&loaded));
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_SOURCE_FLASH, loaded.source);
    TEST_ASSERT_EQUAL_UINT32(0x4000, loaded.package_size);
    TEST_ASSERT_EQUAL_UINT32(0x20000, loaded.flash.flash_offset);
}

void test_rejects_zero_size_flash_request(void)
{
    TEST_ASSERT_NOT_EQUAL(0, uboot_ota_start_from_flash(0x20000, 0));
}

void test_accepts_tf_request_with_mounted_path(void)
{
    uboot_ota_request_t loaded = {0};

    TEST_ASSERT_EQUAL_INT(0, uboot_ota_start_from_tf("/SD:/ota/update.txz"));
    TEST_ASSERT_EQUAL_INT(0, boot_ota_request_load(&loaded));
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_SOURCE_TF, loaded.source);
    TEST_ASSERT_EQUAL_STRING("/SD:/ota/update.txz", loaded.tf.path);
}

void test_accepts_tf_request_with_root_relative_path(void)
{
    uboot_ota_request_t loaded = {0};

    TEST_ASSERT_EQUAL_INT(0, uboot_ota_start_from_tf("/download/update.txz"));
    TEST_ASSERT_EQUAL_INT(0, boot_ota_request_load(&loaded));
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_SOURCE_TF, loaded.source);
    TEST_ASSERT_EQUAL_STRING("/download/update.txz", loaded.tf.path);
}

void test_accepts_tf_request_without_leading_slash(void)
{
    uboot_ota_request_t loaded = {0};

    TEST_ASSERT_EQUAL_INT(0, uboot_ota_start_from_tf("download/update.txz"));
    TEST_ASSERT_EQUAL_INT(0, boot_ota_request_load(&loaded));
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_SOURCE_TF, loaded.source);
    TEST_ASSERT_EQUAL_STRING("download/update.txz", loaded.tf.path);
}

void test_rejects_empty_tf_path(void)
{
    TEST_ASSERT_NOT_EQUAL(0, uboot_ota_start_from_tf(""));
}

void test_clear_removes_pending_request(void)
{
    uboot_ota_request_t loaded = {0};

    TEST_ASSERT_EQUAL_INT(0, uboot_ota_start_from_flash(0x22000, 0x1800));
    TEST_ASSERT_EQUAL_INT(0, boot_ota_request_clear());
    TEST_ASSERT_NOT_EQUAL(0, boot_ota_request_load(&loaded));
}

void test_start_saves_request_before_marking_update_mode(void)
{
    TEST_ASSERT_EQUAL_INT(0, uboot_ota_start_from_flash(0x24000, 0x2400));
    TEST_ASSERT_TRUE(boot_control_store_test_get_save_sequence() > 0);
    TEST_ASSERT_TRUE(boot_control_store_test_get_mode_sequence() > 0);
    TEST_ASSERT_TRUE(boot_control_store_test_get_save_sequence() <
                     boot_control_store_test_get_mode_sequence());
    TEST_ASSERT_EQUAL_INT(BOOT_MODE_UPDATE, boot_control_store_test_get_mode());
}

void test_successful_boot_update_clears_request_and_restores_normal_mode(void)
{
    ota_handler_ctx_t ctx = {0};
    uboot_ota_request_t loaded = {0};
    uboot_ota_failure_info_t info = {
        .source = UBOOT_OTA_SOURCE_TF,
        .reason = UBOOT_OTA_FAILURE_SOURCE_UNAVAILABLE,
        .detail = UBOOT_OTA_FAILURE_DETAIL_TF_OPEN_FAILED,
    };

    TEST_ASSERT_EQUAL_INT(0, boot_control_store_set_failure(&info));
    TEST_ASSERT_EQUAL_INT(0, uboot_ota_start_from_flash(0x26000, 0x2800));
    TEST_ASSERT_EQUAL_INT(BOOT_OTA_LIFECYCLE_UPDATED,
                          boot_ota_lifecycle_run(ota_handler_success, &ctx));
    TEST_ASSERT_EQUAL_INT(1, ctx.call_count);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_SOURCE_FLASH, ctx.last_request.source);
    TEST_ASSERT_EQUAL_UINT32(0x26000, ctx.last_request.flash.flash_offset);
    TEST_ASSERT_EQUAL_UINT32(0x2800, ctx.last_request.package_size);
    TEST_ASSERT_EQUAL_INT(BOOT_MODE_NORMAL, boot_control_store_test_get_mode());
    TEST_ASSERT_NOT_EQUAL(0, boot_ota_request_load(&loaded));

    memset(&info, 0xFF, sizeof(info));
    TEST_ASSERT_EQUAL_INT(0, uboot_ota_get_last_failure(&info));
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_NONE, info.reason);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_DETAIL_NONE, info.detail);
}

void test_failed_boot_update_keeps_request_and_update_mode(void)
{
    ota_handler_ctx_t ctx = {
        .report_failure = {
            .source = UBOOT_OTA_SOURCE_FLASH,
            .reason = UBOOT_OTA_FAILURE_APPLY_FAILED,
            .detail = UBOOT_OTA_FAILURE_DETAIL_FLASH_WRITE_FAILED,
        },
    };
    uboot_ota_request_t loaded = {0};
    uboot_ota_failure_info_t info = {0};

    TEST_ASSERT_EQUAL_INT(0, uboot_ota_start_from_flash(0x2E000, 0x3C00));
    TEST_ASSERT_EQUAL_INT(BOOT_OTA_LIFECYCLE_FAILED,
                          boot_ota_lifecycle_run(ota_handler_failure, &ctx));
    TEST_ASSERT_EQUAL_INT(1, ctx.call_count);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_SOURCE_FLASH, ctx.last_request.source);
    TEST_ASSERT_EQUAL_INT(BOOT_MODE_UPDATE, boot_control_store_test_get_mode());
    TEST_ASSERT_EQUAL_INT(0, boot_ota_request_load(&loaded));
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_SOURCE_FLASH, loaded.source);
    TEST_ASSERT_EQUAL_UINT32(0x2E000, loaded.flash.flash_offset);
    TEST_ASSERT_EQUAL_UINT32(0x3C00, loaded.package_size);

    TEST_ASSERT_EQUAL_INT(0, uboot_ota_get_last_failure(&info));
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_SOURCE_FLASH, info.source);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_APPLY_FAILED, info.reason);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_DETAIL_FLASH_WRITE_FAILED, info.detail);
}

void test_source_unavailable_discards_request_and_restores_normal_mode(void)
{
    ota_handler_ctx_t ctx = {
        .report_failure = {
            .source = UBOOT_OTA_SOURCE_TF,
            .reason = UBOOT_OTA_FAILURE_SOURCE_UNAVAILABLE,
            .detail = UBOOT_OTA_FAILURE_DETAIL_TF_OPEN_FAILED,
        },
    };
    uboot_ota_request_t loaded = {0};
    uboot_ota_failure_info_t info = {0};

    TEST_ASSERT_EQUAL_INT(0, uboot_ota_start_from_tf("download/update.txz"));
    TEST_ASSERT_EQUAL_INT(BOOT_OTA_LIFECYCLE_DISCARDED,
                          boot_ota_lifecycle_run(ota_handler_discard, &ctx));
    TEST_ASSERT_EQUAL_INT(1, ctx.call_count);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_SOURCE_TF, ctx.last_request.source);
    TEST_ASSERT_EQUAL_INT(BOOT_MODE_NORMAL, boot_control_store_test_get_mode());
    TEST_ASSERT_NOT_EQUAL(0, boot_ota_request_load(&loaded));

    TEST_ASSERT_EQUAL_INT(0, uboot_ota_get_last_failure(&info));
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_SOURCE_TF, info.source);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_SOURCE_UNAVAILABLE, info.reason);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_FAILURE_DETAIL_TF_OPEN_FAILED, info.detail);
}

void test_flash_request_converts_to_flash_source_descriptor(void)
{
    uboot_ota_request_t request = {
        .source = UBOOT_OTA_SOURCE_FLASH,
        .package_size = 0x4800,
        .flash = {
            .flash_offset = 0x36000,
        },
    };
    boot_ota_source_t source = {0};

    TEST_ASSERT_EQUAL_INT(0, boot_ota_source_from_request(&request, &source, NULL));
    TEST_ASSERT_EQUAL_UINT32(BOOT_OTA_SOURCE_KIND_FLASH, source.kind);
    TEST_ASSERT_EQUAL_UINT32(0x30036000, source.flash.addr);
    TEST_ASSERT_EQUAL_UINT32(0x4800, source.size);
}

void test_partab_page_is_reserved_at_end_of_boot_region(void)
{
    TEST_ASSERT_EQUAL_UINT32(CONFIG_MEM_FLASH_BASE + CONFIG_BOOT_FLASH_SIZE - BOOT_CONFIG_SIZE,
                             BOOT_CONFIG_BASE);
    TEST_ASSERT_EQUAL_UINT32(CONFIG_MEM_FLASH_BASE + CONFIG_BOOT_FLASH_SIZE,
                             BOOT_CONFIG_BASE + BOOT_CONFIG_SIZE);
}

void test_start_from_ota_partition_uses_fixed_ota_partition_range(void)
{
    uboot_ota_request_t loaded = {0};

    set_partab_partition("OTA_TXZ", 0x30060000, 0x12000, PART_FLAG_VALID);

    TEST_ASSERT_EQUAL_INT(0, uboot_ota_start_from_ota_partition());
    TEST_ASSERT_EQUAL_INT(0, boot_ota_request_load(&loaded));
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_SOURCE_FLASH, loaded.source);
    TEST_ASSERT_EQUAL_UINT32(0x60000, loaded.flash.flash_offset);
    TEST_ASSERT_EQUAL_UINT32(0x12000, loaded.package_size);
}

void test_start_from_ota_partition_rejects_missing_valid_ota_partition(void)
{
    set_partab_partition("AP", 0x3001C000, 0x200000, PART_FLAG_VALID | PART_FLAG_BOOTABLE);

    TEST_ASSERT_NOT_EQUAL(0, uboot_ota_start_from_ota_partition());
}

int main(void)
{
    UnityBegin("system/uboot/test/uboot_ota_api/test_uboot_ota_api.c");

    RUN_TEST(test_last_failure_defaults_to_none, __LINE__);
    RUN_TEST(test_start_does_not_clear_previous_failure, __LINE__);
    RUN_TEST(test_request_clear_preserves_previous_failure, __LINE__);
    RUN_TEST(test_clear_failure_resets_last_failure, __LINE__);
    RUN_TEST(test_accepts_valid_flash_request, __LINE__);
    RUN_TEST(test_rejects_zero_size_flash_request, __LINE__);
    RUN_TEST(test_accepts_tf_request_with_mounted_path, __LINE__);
    RUN_TEST(test_accepts_tf_request_with_root_relative_path, __LINE__);
    RUN_TEST(test_accepts_tf_request_without_leading_slash, __LINE__);
    RUN_TEST(test_rejects_empty_tf_path, __LINE__);
    RUN_TEST(test_clear_removes_pending_request, __LINE__);
    RUN_TEST(test_start_saves_request_before_marking_update_mode, __LINE__);
    RUN_TEST(test_successful_boot_update_clears_request_and_restores_normal_mode, __LINE__);
    RUN_TEST(test_failed_boot_update_keeps_request_and_update_mode, __LINE__);
    RUN_TEST(test_source_unavailable_discards_request_and_restores_normal_mode, __LINE__);
    RUN_TEST(test_flash_request_converts_to_flash_source_descriptor, __LINE__);
    RUN_TEST(test_partab_page_is_reserved_at_end_of_boot_region, __LINE__);
    RUN_TEST(test_start_from_ota_partition_uses_fixed_ota_partition_range, __LINE__);
    RUN_TEST(test_start_from_ota_partition_rejects_missing_valid_ota_partition, __LINE__);

    return UnityEnd();
}
