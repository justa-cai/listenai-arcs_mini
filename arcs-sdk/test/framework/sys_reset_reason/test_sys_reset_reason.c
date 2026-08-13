/*
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>

#include "sys/reset_reason.h"

#define ALL_KNOWN_BITS (SYS_RESET_REASON_POR        \
                        | SYS_RESET_REASON_PIN      \
                        | SYS_RESET_REASON_WATCHDOG \
                        | SYS_RESET_REASON_SOFTWARE \
                        | SYS_RESET_REASON_UNKNOWN)

void setUp(void)
{
}

void tearDown(void)
{
}

void test_reset_reason_not_zero_after_boot(void)
{
    /* 启动后 HAL 已完成 snapshot；至少包含一个 cause（cold boot 必有 POR） */
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, sys_reset_reason_get());
}

void test_reset_reason_no_undefined_bits(void)
{
    /* 返回值不应包含任何抽象集之外的位（防止映射函数泄漏） */
    uint32_t r = sys_reset_reason_get();
    TEST_ASSERT_EQUAL_HEX32(0u, r & ~ALL_KNOWN_BITS);
}

void test_reset_reason_no_UNKNOWN_on_arcs(void)
{
    /* ARCS 6 个硬件位都已映射，启动时不应落 UNKNOWN 兜底 */
    uint32_t r = sys_reset_reason_get();
    TEST_ASSERT_EQUAL_HEX32(0u, r & SYS_RESET_REASON_UNKNOWN);
}

void test_cold_boot_includes_POR_and_PIN(void)
{
    /*
     * cskburn 烧录后通过电源/复位脚启动板子，ARCS POR_STATUS (bit 0)
     * 复位默认值就是 0x1，且硬件合并 POR + PAD reset 共用此位。
     * 按抽象映射规则，启动后应同时置 POR 和 PIN。
     */
    uint32_t r = sys_reset_reason_get();
    TEST_ASSERT_TRUE_MESSAGE(r & SYS_RESET_REASON_POR,
                             "cold boot expected to flag POR");
    TEST_ASSERT_TRUE_MESSAGE(r & SYS_RESET_REASON_PIN,
                             "POR must co-flag PIN per ARCS mapping");
}

void test_get_returns_stable_value(void)
{
    /* 快照一次性建立，多次读返回值相同 */
    uint32_t a = sys_reset_reason_get();
    uint32_t b = sys_reset_reason_get();
    TEST_ASSERT_EQUAL_HEX32(a, b);
}

int main(void)
{
    UnityBegin("test/framework/sys_reset_reason/test_sys_reset_reason.c");
    RUN_TEST(test_reset_reason_not_zero_after_boot, __LINE__);
    RUN_TEST(test_reset_reason_no_undefined_bits, __LINE__);
    RUN_TEST(test_reset_reason_no_UNKNOWN_on_arcs, __LINE__);
    RUN_TEST(test_cold_boot_includes_POR_and_PIN, __LINE__);
    RUN_TEST(test_get_returns_stable_value, __LINE__);
    return UnityEnd();
}
