#include <unity.h>

#ifdef RUN_TEST
#undef RUN_TEST
#endif
#define RUN_TEST(TestFunc, TestLineNum) UnityDefaultTestRun(TestFunc, #TestFunc, TestLineNum)

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "boot_config.h"
#include "boot_stage_gate.h"

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

static struct boot_config make_valid_config(uint8_t recover)
{
    struct boot_config cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.hr.tag = BOOT_CONFIG_TAG;
    cfg.hr.version = BOOT_CONFIG_VERSION;
    cfg.hr.size = sizeof(struct boot_config) - sizeof(struct boot_cfg_hr);
    cfg.recover = recover;
    return cfg;
}

void setUp(void)
{
}

void tearDown(void)
{
}

void test_consumes_ota_request_but_preserves_pending_retry(void)
{
    uint32_t raw = make_boot_info_raw(true, true, RECOVER_REASON_NONE, false, false);

    TEST_ASSERT_TRUE(boot_stage_gate_should_enter_second_stage(&raw, NULL));
    TEST_ASSERT_EQUAL_UINT32(make_boot_info_raw(false, true, RECOVER_REASON_NONE, false, false), raw);
}

void test_enters_second_stage_for_pending_retry_without_request(void)
{
    uint32_t raw = make_boot_info_raw(false, true, RECOVER_REASON_NONE, false, false);

    TEST_ASSERT_TRUE(boot_stage_gate_should_enter_second_stage(&raw, NULL));
    TEST_ASSERT_EQUAL_UINT32(make_boot_info_raw(false, true, RECOVER_REASON_NONE, false, false), raw);
}

void test_enters_second_stage_for_recovery_request_without_pending_retry(void)
{
    uint32_t raw = make_boot_info_raw(true, false, RECOVER_REASON_SOFT_REQ, false, false);

    TEST_ASSERT_TRUE(boot_stage_gate_should_enter_second_stage(&raw, NULL));
}

void test_enters_second_stage_for_persistent_hard_recovery(void)
{
    uint32_t raw = make_boot_info_raw(false, false, RECOVER_REASON_NONE, false, false);
    struct boot_config cfg = make_valid_config(true);

    TEST_ASSERT_TRUE(boot_stage_gate_should_enter_second_stage(&raw, &cfg));
    TEST_ASSERT_EQUAL_UINT32(make_boot_info_raw(false, false, RECOVER_REASON_HARD_REQ, false, false), raw);
}

void test_persistent_hard_recovery_preserves_existing_reason_metadata(void)
{
    uint32_t raw = make_boot_info_raw(false, false, RECOVER_REASON_APP_INVALID, false, false);
    struct boot_config cfg = make_valid_config(true);

    TEST_ASSERT_TRUE(boot_stage_gate_should_enter_second_stage(&raw, &cfg));
    TEST_ASSERT_EQUAL_UINT32(make_boot_info_raw(false, false, RECOVER_REASON_APP_INVALID, false, false), raw);
}

void test_watchdog_request_synthesizes_boot_watchdog_reason(void)
{
    uint32_t raw = make_boot_info_raw(true, false, RECOVER_REASON_NONE, false, true);

    TEST_ASSERT_TRUE(boot_stage_gate_should_enter_second_stage(&raw, NULL));
    TEST_ASSERT_EQUAL_UINT32(make_boot_info_raw(false, false, RECOVER_REASON_BOOT_WDT_TIMEOUT, false, true), raw);
}

void test_handshake_timeout_request_synthesizes_ap_watchdog_reason(void)
{
    uint32_t raw = make_boot_info_raw(true, false, RECOVER_REASON_NONE, true, false);

    TEST_ASSERT_TRUE(boot_stage_gate_should_enter_second_stage(&raw, NULL));
    TEST_ASSERT_EQUAL_UINT32(make_boot_info_raw(false, false, RECOVER_REASON_AP_WDT_TIMEOUT, true, false), raw);
}

void test_invalid_or_missing_config_does_not_trigger_second_stage(void)
{
    uint32_t raw = make_boot_info_raw(false, false, RECOVER_REASON_NONE, false, false);
    struct boot_config cfg = make_valid_config(true);

    TEST_ASSERT_FALSE(boot_stage_gate_should_enter_second_stage(&raw, NULL));

    memset(&cfg, 0, sizeof(cfg));
    TEST_ASSERT_FALSE(boot_stage_gate_should_enter_second_stage(&raw, &cfg));

    cfg = make_valid_config(true);
    cfg.hr.version = BOOT_CONFIG_VERSION + 1;
    TEST_ASSERT_FALSE(boot_stage_gate_should_enter_second_stage(&raw, &cfg));

    cfg = make_valid_config(true);
    cfg.hr.size = 0;
    TEST_ASSERT_FALSE(boot_stage_gate_should_enter_second_stage(&raw, &cfg));
}

void test_reason_only_metadata_does_not_trigger_second_stage(void)
{
    uint32_t raw = make_boot_info_raw(false, false, RECOVER_REASON_APP_INVALID, false, false);

    TEST_ASSERT_FALSE(boot_stage_gate_should_enter_second_stage(&raw, NULL));
    TEST_ASSERT_EQUAL_UINT32(make_boot_info_raw(false, false, RECOVER_REASON_APP_INVALID, false, false), raw);
}

void test_persistent_recovery_and_pending_retry_both_request_second_stage(void)
{
    uint32_t raw = make_boot_info_raw(false, true, RECOVER_REASON_HARD_REQ, false, false);
    struct boot_config cfg = make_valid_config(true);

    TEST_ASSERT_TRUE(boot_stage_gate_should_enter_second_stage(&raw, &cfg));
    TEST_ASSERT_EQUAL_UINT32(make_boot_info_raw(false, true, RECOVER_REASON_HARD_REQ, false, false), raw);
}

int main(void)
{
    UnityBegin("system/uboot/test/boot_stage_gate/test_boot_stage_gate.c");

    RUN_TEST(test_consumes_ota_request_but_preserves_pending_retry, __LINE__);
    RUN_TEST(test_enters_second_stage_for_pending_retry_without_request, __LINE__);
    RUN_TEST(test_enters_second_stage_for_recovery_request_without_pending_retry, __LINE__);
    RUN_TEST(test_enters_second_stage_for_persistent_hard_recovery, __LINE__);
    RUN_TEST(test_persistent_hard_recovery_preserves_existing_reason_metadata, __LINE__);
    RUN_TEST(test_watchdog_request_synthesizes_boot_watchdog_reason, __LINE__);
    RUN_TEST(test_handshake_timeout_request_synthesizes_ap_watchdog_reason, __LINE__);
    RUN_TEST(test_invalid_or_missing_config_does_not_trigger_second_stage, __LINE__);
    RUN_TEST(test_reason_only_metadata_does_not_trigger_second_stage, __LINE__);
    RUN_TEST(test_persistent_recovery_and_pending_retry_both_request_second_stage, __LINE__);

    return UnityEnd();
}
