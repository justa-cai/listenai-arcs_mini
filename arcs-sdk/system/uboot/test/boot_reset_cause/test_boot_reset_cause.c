#include <unity.h>

#ifdef RUN_TEST
#undef RUN_TEST
#endif
#define RUN_TEST(TestFunc, TestLineNum) UnityDefaultTestRun(TestFunc, #TestFunc, TestLineNum)

#include <stdbool.h>
#include <stdint.h>

#include "boot_config.h"
#include "boot_reset_cause.h"

static uint32_t make_boot_info_raw(uint8_t reboot_cnt, uint8_t recover_reason, bool req, bool boot_wdt)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};

    bits.info.reboot_cnt = reboot_cnt;
    bits.info.recover_reason = recover_reason;
    bits.info.req = req ? 1u : 0u;
    bits.info.boot_wdt = boot_wdt ? 1u : 0u;
    return bits.raw;
}

void setUp(void)
{
}

void tearDown(void)
{
}

void test_non_watchdog_reset_keeps_boot_info_unchanged(void)
{
    uint32_t raw = make_boot_info_raw(3u, RECOVER_REASON_NONE, false, false);

    boot_reset_cause_apply(&raw, 0u);

    TEST_ASSERT_EQUAL_UINT32(make_boot_info_raw(3u, RECOVER_REASON_NONE, false, false), raw);
}

void test_ap_watchdog_below_threshold_only_increments_reboot_count(void)
{
    uint32_t raw = make_boot_info_raw(3u, RECOVER_REASON_NONE, false, false);

    boot_reset_cause_apply(&raw, 1u << PMU_RST_AP_SW_WDT);

    TEST_ASSERT_EQUAL_UINT32(make_boot_info_raw(4u, RECOVER_REASON_NONE, false, false), raw);
}

void test_ap_watchdog_reaching_threshold_requests_recovery(void)
{
    uint32_t raw = make_boot_info_raw(4u, RECOVER_REASON_NONE, false, false);

    boot_reset_cause_apply(&raw, 1u << PMU_RST_AP_SW_WDT);

    TEST_ASSERT_EQUAL_UINT32(make_boot_info_raw(0u, RECOVER_REASON_AP_WDT_TIMEOUT, true, false), raw);
}

void test_ap_watchdog_threshold_overrides_previous_reason(void)
{
    uint32_t raw = make_boot_info_raw(4u, RECOVER_REASON_APP_INVALID, false, false);

    boot_reset_cause_apply(&raw, 1u << PMU_RST_AP_SW_WDT);

    TEST_ASSERT_EQUAL_UINT32(make_boot_info_raw(0u, RECOVER_REASON_AP_WDT_TIMEOUT, true, false), raw);
}

void test_boot_watchdog_sets_transient_request_and_flag(void)
{
    uint32_t raw = make_boot_info_raw(2u, RECOVER_REASON_NONE, false, false);

    boot_reset_cause_apply(&raw, 1u << PMU_RST_CP_WDT);

    TEST_ASSERT_EQUAL_UINT32(make_boot_info_raw(2u, RECOVER_REASON_NONE, true, true), raw);
}

int main(void)
{
    UnityBegin("system/uboot/test/boot_reset_cause/test_boot_reset_cause.c");

    RUN_TEST(test_non_watchdog_reset_keeps_boot_info_unchanged, __LINE__);
    RUN_TEST(test_ap_watchdog_below_threshold_only_increments_reboot_count, __LINE__);
    RUN_TEST(test_ap_watchdog_reaching_threshold_requests_recovery, __LINE__);
    RUN_TEST(test_ap_watchdog_threshold_overrides_previous_reason, __LINE__);
    RUN_TEST(test_boot_watchdog_sets_transient_request_and_flag, __LINE__);

    return UnityEnd();
}
