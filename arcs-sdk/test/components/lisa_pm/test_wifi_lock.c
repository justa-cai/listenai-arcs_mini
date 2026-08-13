/*
 * lisa_pm 测试 — case 2: WiFi 省电锁引用计数
 *
 * 覆盖 lisa_pm_wifi_lock_acquire / release / get_count /
 * lisa_pm_wifi_is_power_save_blocked 的引用计数语义。关键不变量:
 *   wifi_lock_get_count() > 0  <=>  wifi_is_power_save_blocked() == true
 *
 * setUp 已将 PS mode 设为 LISTEN; PS mode 与 PS lock 是两个独立维度,
 * initial_state 隐式验证 LISTEN 不会让 wifi_lock 计数非 0。
 *
 * 不在本 case 范围:
 *   - count=0 时多余 release 返回 <0 (归到 case 3 errors)
 */

#include "test_common.h"

#include <stdio.h>
#include "lisa_pm.h"
#include "unity.h"

static void test_wifi_lock_initial_state(void)
{
    int32_t count = lisa_pm_wifi_lock_get_count();
    bool blocked = lisa_pm_wifi_is_power_save_blocked();
    printf("[WIFI_LOCK] initial: count=%ld blocked=%d\n",
           (long)count, (int)blocked);

    TEST_ASSERT_EQUAL_INT32(0, count);
    TEST_ASSERT_FALSE(blocked);
}

static void test_wifi_lock_acquire_release_basic(void)
{
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_wifi_lock_acquire());
    TEST_ASSERT_EQUAL_INT32(1, lisa_pm_wifi_lock_get_count());
    TEST_ASSERT_TRUE(lisa_pm_wifi_is_power_save_blocked());

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_wifi_lock_release());
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_wifi_lock_get_count());
    TEST_ASSERT_FALSE(lisa_pm_wifi_is_power_save_blocked());
}

static void test_wifi_lock_nested_acquire(void)
{
    /* acquire ×3 — count 1→2→3, 全程 blocked */
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_wifi_lock_acquire());
    TEST_ASSERT_EQUAL_INT32(1, lisa_pm_wifi_lock_get_count());
    TEST_ASSERT_TRUE(lisa_pm_wifi_is_power_save_blocked());

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_wifi_lock_acquire());
    TEST_ASSERT_EQUAL_INT32(2, lisa_pm_wifi_lock_get_count());
    TEST_ASSERT_TRUE(lisa_pm_wifi_is_power_save_blocked());

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_wifi_lock_acquire());
    TEST_ASSERT_EQUAL_INT32(3, lisa_pm_wifi_lock_get_count());
    TEST_ASSERT_TRUE(lisa_pm_wifi_is_power_save_blocked());

    /* release ×3 — count 2→1→0, blocked 仅在最后一步落回 false */
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_wifi_lock_release());
    TEST_ASSERT_EQUAL_INT32(2, lisa_pm_wifi_lock_get_count());
    TEST_ASSERT_TRUE(lisa_pm_wifi_is_power_save_blocked());

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_wifi_lock_release());
    TEST_ASSERT_EQUAL_INT32(1, lisa_pm_wifi_lock_get_count());
    TEST_ASSERT_TRUE(lisa_pm_wifi_is_power_save_blocked());

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_wifi_lock_release());
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_wifi_lock_get_count());
    TEST_ASSERT_FALSE(lisa_pm_wifi_is_power_save_blocked());
}

void run_wifi_lock_tests(void)
{
    RUN_TEST(test_wifi_lock_initial_state);
    RUN_TEST(test_wifi_lock_acquire_release_basic);
    RUN_TEST(test_wifi_lock_nested_acquire);
}
