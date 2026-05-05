#include "unity.h"

#ifdef RUN_TEST
#undef RUN_TEST
#endif
#define RUN_TEST(TestFunc, TestLineNum) UnityDefaultTestRun(TestFunc, #TestFunc, TestLineNum)

#include <stdint.h>
#include <string.h>

#include "boot_config.h"
#include "uboot_recovery_api.h"

static uint32_t g_boot_info_raw;
static int g_hard_enter_call_count;
static int g_hard_enter_result;
static int g_persistent_recovery_armed;

volatile uint32_t *uboot_recovery_api_boot_info_reg(void)
{
    return &g_boot_info_raw;
}

void boot_recovery_software_enter(void)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {
        .raw = g_boot_info_raw,
    };

    bits.info.req = 1u;
    g_boot_info_raw = bits.raw;
}

int boot_recovery_hardware_enter(void)
{
    g_hard_enter_call_count++;
    if (g_hard_enter_result == 0) {
        g_persistent_recovery_armed = 1;
    }

    return g_hard_enter_result;
}

void setUp(void)
{
    g_boot_info_raw = 0;
    g_hard_enter_call_count = 0;
    g_hard_enter_result = 0;
    g_persistent_recovery_armed = 0;
}

void tearDown(void)
{
}

static uint32_t make_boot_info_raw(uint8_t recover_reason, uint8_t req)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};

    bits.info.recover_reason = recover_reason;
    bits.info.req = req;
    return bits.raw;
}

void test_request_soft_recovery_sets_reason_and_request_bit(void)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};

    TEST_ASSERT_EQUAL_INT(0, uboot_recovery_request(UBOOT_RECOVERY_MODE_SOFT));

    bits.raw = g_boot_info_raw;
    TEST_ASSERT_EQUAL_UINT32(RECOVER_REASON_SOFT_REQ, bits.info.recover_reason);
    TEST_ASSERT_EQUAL_UINT32(1u, bits.info.req);
    TEST_ASSERT_EQUAL_INT(0, g_hard_enter_call_count);
    TEST_ASSERT_EQUAL_INT(0, g_persistent_recovery_armed);
}

void test_request_hard_recovery_uses_persistent_helper(void)
{
    TEST_ASSERT_EQUAL_INT(0, uboot_recovery_request(UBOOT_RECOVERY_MODE_HARD));
    TEST_ASSERT_EQUAL_INT(1, g_hard_enter_call_count);
    TEST_ASSERT_EQUAL_INT(1, g_persistent_recovery_armed);
    TEST_ASSERT_EQUAL_UINT32(0u, g_boot_info_raw);
}

void test_request_rejects_invalid_mode_without_touching_state(void)
{
    g_boot_info_raw = make_boot_info_raw(RECOVER_REASON_APP_INVALID, 1u);

    TEST_ASSERT_NOT_EQUAL(0, uboot_recovery_request((uboot_recovery_mode_t)99));
    TEST_ASSERT_EQUAL_UINT32(make_boot_info_raw(RECOVER_REASON_APP_INVALID, 1u), g_boot_info_raw);
    TEST_ASSERT_EQUAL_INT(0, g_hard_enter_call_count);
    TEST_ASSERT_EQUAL_INT(0, g_persistent_recovery_armed);
}

void test_request_hard_recovery_propagates_persist_error_without_arming(void)
{
    g_hard_enter_result = -23;
    g_boot_info_raw = make_boot_info_raw(RECOVER_REASON_SOFT_REQ, 1u);

    TEST_ASSERT_EQUAL_INT(-23, uboot_recovery_request(UBOOT_RECOVERY_MODE_HARD));
    TEST_ASSERT_EQUAL_INT(1, g_hard_enter_call_count);
    TEST_ASSERT_EQUAL_INT(0, g_persistent_recovery_armed);
    TEST_ASSERT_EQUAL_UINT32(make_boot_info_raw(RECOVER_REASON_SOFT_REQ, 1u), g_boot_info_raw);
}

int main(void)
{
    UnityBegin("system/uboot/test/uboot_recovery_api/test_uboot_recovery_api.c");

    RUN_TEST(test_request_soft_recovery_sets_reason_and_request_bit, __LINE__);
    RUN_TEST(test_request_hard_recovery_uses_persistent_helper, __LINE__);
    RUN_TEST(test_request_rejects_invalid_mode_without_touching_state, __LINE__);
    RUN_TEST(test_request_hard_recovery_propagates_persist_error_without_arming, __LINE__);

    return UnityEnd();
}
