/*
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"

#include <stdint.h>

#include "PowerManager.h"
#include "sys/reset_reason.h"

uint32_t sys_arch_reset_reason_get(void);

static uint32_t g_fake_raw;
static int g_fake_snapshot_calls;

uint32_t HAL_PMU_GetSysResetCauseRaw(void)
{
    return g_fake_raw;
}

void HAL_PMU_SnapshotResetCause(void)
{
    g_fake_snapshot_calls++;
}

void setUp(void)
{
    g_fake_raw = 0;
}

void tearDown(void)
{
}

static void run_case(uint32_t raw, sys_reset_reason_t expected)
{
    g_fake_raw = raw;
    TEST_ASSERT_EQUAL_HEX32(expected, sys_arch_reset_reason_get());
}

void test_empty_raw_returns_zero(void)
{
    run_case(0u, 0u);
}

void test_POR_maps_to_POR_and_PIN(void)
{
    run_case(1u << PMU_RST_POR,
             SYS_RESET_REASON_POR | SYS_RESET_REASON_PIN);
}

void test_AON_maps_to_WATCHDOG_and_SOFTWARE(void)
{
    run_case(1u << PMU_RST_AON,
             SYS_RESET_REASON_WATCHDOG | SYS_RESET_REASON_SOFTWARE);
}

void test_CP_WDT_maps_to_WATCHDOG(void)
{
    run_case(1u << PMU_RST_CP_WDT, SYS_RESET_REASON_WATCHDOG);
}

void test_CMN_maps_to_SOFTWARE(void)
{
    run_case(1u << PMU_RST_CMN, SYS_RESET_REASON_SOFTWARE);
}

void test_CP_SW_maps_to_SOFTWARE(void)
{
    run_case(1u << PMU_RST_CP_SW, SYS_RESET_REASON_SOFTWARE);
}

void test_AP_SW_WDT_maps_to_WATCHDOG_and_SOFTWARE(void)
{
    run_case(1u << PMU_RST_AP_SW_WDT,
             SYS_RESET_REASON_WATCHDOG | SYS_RESET_REASON_SOFTWARE);
}

void test_POR_plus_CP_WDT_coexist(void)
{
    run_case((1u << PMU_RST_POR) | (1u << PMU_RST_CP_WDT),
             SYS_RESET_REASON_POR
             | SYS_RESET_REASON_PIN
             | SYS_RESET_REASON_WATCHDOG);
}

void test_CP_WDT_plus_CP_SW_coexist(void)
{
    run_case((1u << PMU_RST_CP_WDT) | (1u << PMU_RST_CP_SW),
             SYS_RESET_REASON_WATCHDOG | SYS_RESET_REASON_SOFTWARE);
}

void test_reserved_bit_falls_back_to_UNKNOWN(void)
{
    /* bit 5 is reserved per LS26 manual; never mapped. */
    run_case(1u << 5, SYS_RESET_REASON_UNKNOWN);
}

void test_UNKNOWN_coexists_with_known_when_unmapped_bits_present(void)
{
    /*
     * 已知位 + 未知位共存：必须同时置 UNKNOWN，提示调用方"raw 含有
     * 未映射信息"。否则未识别位会被静默丢弃，调用方无法区分"硬件
     * 真的只是 POR" vs "POR + 未知原因"。
     */
    run_case((1u << PMU_RST_POR) | (1u << 5),
             SYS_RESET_REASON_POR
             | SYS_RESET_REASON_PIN
             | SYS_RESET_REASON_UNKNOWN);
}

int main(void)
{
    UnityBegin("test/host/sys_reset_reason/test_sys_reset_reason.c");
    RUN_TEST(test_empty_raw_returns_zero, __LINE__);
    RUN_TEST(test_POR_maps_to_POR_and_PIN, __LINE__);
    RUN_TEST(test_AON_maps_to_WATCHDOG_and_SOFTWARE, __LINE__);
    RUN_TEST(test_CP_WDT_maps_to_WATCHDOG, __LINE__);
    RUN_TEST(test_CMN_maps_to_SOFTWARE, __LINE__);
    RUN_TEST(test_CP_SW_maps_to_SOFTWARE, __LINE__);
    RUN_TEST(test_AP_SW_WDT_maps_to_WATCHDOG_and_SOFTWARE, __LINE__);
    RUN_TEST(test_POR_plus_CP_WDT_coexist, __LINE__);
    RUN_TEST(test_CP_WDT_plus_CP_SW_coexist, __LINE__);
    RUN_TEST(test_reserved_bit_falls_back_to_UNKNOWN, __LINE__);
    RUN_TEST(test_UNKNOWN_coexists_with_known_when_unmapped_bits_present, __LINE__);
    return UnityEnd();
}
