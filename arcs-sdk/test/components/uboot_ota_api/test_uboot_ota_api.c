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
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "arcs_ap.h"
#include "boot_config.h"
#include "uboot_ota_api.h"
#include "boot_ota_lifecycle.h"
#include "boot_ota_request.h"
#include "boot_control_store_test.h"

extern bool boot_ota_handoff_consume_upgrade_request(void);
extern bool boot_ota_handoff_has_pending_update(void);
extern void boot_ota_handoff_clear_pending_update(void);
extern bool boot_ota_handoff_should_enter_second_stage(void);

typedef struct {
    int call_count;
    uboot_ota_request_t last_request;
} ota_handler_ctx_t;

static uint32_t saved_boot_info_reg;

static uint32_t boot_info_request_mask(void)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};

    bits.info.req = 1;
    return bits.raw;
}

static uint32_t boot_info_pending_mask(void)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};

    bits.info.reserved = 1;
    return bits.raw;
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

    (void)failure;
    handler_ctx->call_count++;
    handler_ctx->last_request = *req;
    return -1;
}

static int ota_handler_discard(const uboot_ota_request_t *req,
                               uboot_ota_failure_info_t *failure,
                               void *ctx)
{
    ota_handler_ctx_t *handler_ctx = ctx;

    (void)failure;
    handler_ctx->call_count++;
    handler_ctx->last_request = *req;
    return BOOT_OTA_LIFECYCLE_FAILED_DISCARD_REQUEST;
}

void setUp(void)
{
    saved_boot_info_reg = IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    IP_AON_CTRL->REG_AON_DIG_RSVD4.all = 0;
    boot_control_store_test_reset();
    boot_ota_request_clear();
    boot_ota_handoff_clear_pending_update();
}

void tearDown(void)
{
    IP_AON_CTRL->REG_AON_DIG_RSVD4.all = saved_boot_info_reg;
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

    TEST_ASSERT_EQUAL_INT(0, uboot_ota_start_from_flash(0x26000, 0x2800));
    TEST_ASSERT_EQUAL_INT(BOOT_OTA_LIFECYCLE_UPDATED,
                          boot_ota_lifecycle_run(ota_handler_success, &ctx));
    TEST_ASSERT_EQUAL_INT(1, ctx.call_count);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_SOURCE_FLASH, ctx.last_request.source);
    TEST_ASSERT_EQUAL_UINT32(0x26000, ctx.last_request.flash.flash_offset);
    TEST_ASSERT_EQUAL_UINT32(0x2800, ctx.last_request.package_size);
    TEST_ASSERT_EQUAL_INT(BOOT_MODE_NORMAL, boot_control_store_test_get_mode());
    TEST_ASSERT_NOT_EQUAL(0, boot_ota_request_load(&loaded));
}

void test_failed_boot_update_keeps_request_and_update_mode(void)
{
    ota_handler_ctx_t ctx = {0};
    uboot_ota_request_t loaded = {0};

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
}

void test_source_unavailable_discards_request_and_restores_normal_mode(void)
{
    ota_handler_ctx_t ctx = {0};
    uboot_ota_request_t loaded = {0};

    TEST_ASSERT_EQUAL_INT(0, uboot_ota_start_from_tf("download/update.txz"));
    TEST_ASSERT_EQUAL_INT(BOOT_OTA_LIFECYCLE_DISCARDED,
                          boot_ota_lifecycle_run(ota_handler_discard, &ctx));
    TEST_ASSERT_EQUAL_INT(1, ctx.call_count);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_OTA_SOURCE_TF, ctx.last_request.source);
    TEST_ASSERT_EQUAL_INT(BOOT_MODE_NORMAL, boot_control_store_test_get_mode());
    TEST_ASSERT_NOT_EQUAL(0, boot_ota_request_load(&loaded));
}

void test_start_marks_boot_second_stage_request_for_reboot(void)
{
    struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    uint32_t expected_mask = boot_info_request_mask() | boot_info_pending_mask();

    info->req = 0;

    TEST_ASSERT_EQUAL_INT(0, uboot_ota_start_from_flash(0x28000, 0x3000));
    TEST_ASSERT_EQUAL_HEX32(expected_mask, IP_AON_CTRL->REG_AON_DIG_RSVD4.all);
    TEST_ASSERT_EQUAL_UINT32(1, info->req);
    TEST_ASSERT_TRUE(boot_ota_handoff_has_pending_update());
}

void test_boot_can_consume_pending_upgrade_request(void)
{
    struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;

    info->req = 0;

    TEST_ASSERT_EQUAL_INT(0, uboot_ota_start_from_flash(0x2A000, 0x3400));
    TEST_ASSERT_TRUE(boot_ota_handoff_consume_upgrade_request());
    TEST_ASSERT_EQUAL_UINT32(0, info->req);
    TEST_ASSERT_FALSE(boot_ota_handoff_consume_upgrade_request());
}

void test_boot_retries_second_stage_while_pending_update_survives(void)
{
    struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;

    info->req = 0;

    TEST_ASSERT_EQUAL_INT(0, uboot_ota_start_from_flash(0x2C000, 0x3800));
    TEST_ASSERT_TRUE(boot_ota_handoff_has_pending_update());
    TEST_ASSERT_TRUE(boot_ota_handoff_should_enter_second_stage());
    TEST_ASSERT_EQUAL_UINT32(0, info->req);
    TEST_ASSERT_TRUE(boot_ota_handoff_has_pending_update());
    TEST_ASSERT_TRUE(boot_ota_handoff_should_enter_second_stage());
}

int main(void)
{
    UnityBegin("test/components/uboot_ota_api/test_uboot_ota_api.c");

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
    RUN_TEST(test_start_marks_boot_second_stage_request_for_reboot, __LINE__);
    RUN_TEST(test_boot_can_consume_pending_upgrade_request, __LINE__);
    RUN_TEST(test_boot_retries_second_stage_while_pending_update_survives, __LINE__);

    return UnityEnd();
}
