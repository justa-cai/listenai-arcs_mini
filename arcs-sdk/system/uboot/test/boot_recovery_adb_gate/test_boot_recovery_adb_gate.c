#include "unity.h"

#ifdef RUN_TEST
#undef RUN_TEST
#endif
#define RUN_TEST(TestFunc, TestLineNum) UnityDefaultTestRun(TestFunc, #TestFunc, TestLineNum)

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "boot_config.h"
#include "boot_recovery_adb_gate.h"

static uint32_t make_boot_info_raw(bool req, bool pending, uint8_t recover_reason, bool handshake_timeout,
                                   bool boot_wdt)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};

    bits.info.req = req ? 1u : 0u;
    bits.info.ota_pending = pending ? 1u : 0u;
    bits.info.recover_reason = recover_reason;
    bits.info.handshake_timeout = handshake_timeout ? 1u : 0u;
    bits.info.boot_wdt = boot_wdt ? 1u : 0u;
    return bits.raw;
}

void setUp(void)
{
}

void tearDown(void)
{
}

void test_allows_explicit_recovery_reasons_only(void)
{
    const uint8_t allow_reasons[] = {
        RECOVER_REASON_SOFT_REQ,
        RECOVER_REASON_HARD_REQ,
        RECOVER_REASON_APP_INVALID,
        RECOVER_REASON_AP_WDT_TIMEOUT,
        RECOVER_REASON_BOOT_WDT_TIMEOUT,
    };

    for (size_t i = 0; i < sizeof(allow_reasons) / sizeof(allow_reasons[0]); ++i) {
        TEST_ASSERT_TRUE(boot_recovery_adb_should_start(
            make_boot_info_raw(false, false, allow_reasons[i], false, false)));
    }
}

void test_denies_zero_boot_info(void)
{
    TEST_ASSERT_FALSE(boot_recovery_adb_should_start(0));
}

void test_denies_pending_only_second_stage_entry(void)
{
    TEST_ASSERT_FALSE(boot_recovery_adb_should_start(
        make_boot_info_raw(false, true, RECOVER_REASON_NONE, false, false)));
}

void test_denies_unknown_recovery_reason(void)
{
    TEST_ASSERT_FALSE(boot_recovery_adb_should_start(
        make_boot_info_raw(false, false, RECOVER_REASON_MAX, false, false)));
}

void test_denies_reason_above_recovery_max(void)
{
    TEST_ASSERT_FALSE(boot_recovery_adb_should_start(
        make_boot_info_raw(false, false, UINT8_MAX, false, false)));
}

void test_denies_request_and_pending_bits_without_explicit_reason(void)
{
    TEST_ASSERT_FALSE(boot_recovery_adb_should_start(
        make_boot_info_raw(true, true, RECOVER_REASON_NONE, true, true)));
}

int main(void)
{
    UnityBegin("system/uboot/test/boot_recovery_adb_gate/test_boot_recovery_adb_gate.c");

    RUN_TEST(test_allows_explicit_recovery_reasons_only, __LINE__);
    RUN_TEST(test_denies_zero_boot_info, __LINE__);
    RUN_TEST(test_denies_pending_only_second_stage_entry, __LINE__);
    RUN_TEST(test_denies_unknown_recovery_reason, __LINE__);
    RUN_TEST(test_denies_reason_above_recovery_max, __LINE__);
    RUN_TEST(test_denies_request_and_pending_bits_without_explicit_reason, __LINE__);

    return UnityEnd();
}
