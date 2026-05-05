#include <unity.h>

#ifdef RUN_TEST
#undef RUN_TEST
#endif
#define RUN_TEST(TestFunc, TestLineNum) UnityDefaultTestRun(TestFunc, #TestFunc, TestLineNum)

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "boot_config.h"
#include "boot_default_app.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_blank_image_is_not_a_valid_app(void)
{
    uint8_t image[0x180];

    memset(image, 0xFF, sizeof(image));
    TEST_ASSERT_FALSE(boot_default_app_image_is_valid(image, 0x30040000));
}

void test_header_with_hr_magic_and_matching_base_is_valid(void)
{
    uint8_t image[0x180];
    uint32_t load_addr = 0x30040000;

    memset(image, 0xFF, sizeof(image));
    image[0x140] = 'H';
    image[0x141] = 'r';
    memcpy(&image[0x148], &load_addr, sizeof(load_addr));

    TEST_ASSERT_TRUE(boot_default_app_image_is_valid(image, load_addr));
}

void test_invalid_app_sets_recovery_request_and_reason(void)
{
    uint32_t raw = 0;
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};

    boot_default_app_prepare_recovery(&raw, false);

    bits.raw = raw;
    TEST_ASSERT_EQUAL_UINT32(1u, bits.info.req);
    TEST_ASSERT_EQUAL_UINT32(RECOVER_REASON_APP_INVALID, bits.info.recover_reason);
}

void test_invalid_app_preserves_existing_reason(void)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};

    bits.info.recover_reason = RECOVER_REASON_BOOT_WDT_TIMEOUT;
    boot_default_app_prepare_recovery(&bits.raw, false);

    TEST_ASSERT_EQUAL_UINT32(1u, bits.info.req);
    TEST_ASSERT_EQUAL_UINT32(RECOVER_REASON_BOOT_WDT_TIMEOUT, bits.info.recover_reason);
}

void test_valid_app_does_not_change_boot_info(void)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};
    uint32_t before;

    bits.info.req = 0u;
    bits.info.recover_reason = RECOVER_REASON_SOFT_REQ;
    before = bits.raw;

    boot_default_app_prepare_recovery(&bits.raw, true);

    TEST_ASSERT_EQUAL_UINT32(before, bits.raw);
}

int main(void)
{
    UnityBegin("system/uboot/test/boot_default_app/test_boot_default_app.c");

    RUN_TEST(test_blank_image_is_not_a_valid_app, __LINE__);
    RUN_TEST(test_header_with_hr_magic_and_matching_base_is_valid, __LINE__);
    RUN_TEST(test_invalid_app_sets_recovery_request_and_reason, __LINE__);
    RUN_TEST(test_invalid_app_preserves_existing_reason, __LINE__);
    RUN_TEST(test_valid_app_does_not_change_boot_info, __LINE__);

    return UnityEnd();
}
