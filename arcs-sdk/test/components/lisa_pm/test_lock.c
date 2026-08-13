/*
 * lisa_pm 测试 — case 1: 系统睡眠锁引用计数
 *
 * 覆盖 lisa_pm_lock_acquire / lisa_pm_lock_release / lisa_pm_lock_get_count /
 * lisa_pm_is_sleep_blocked 的引用计数语义。关键不变量:
 *   count > 0  <=>  is_sleep_blocked() == true
 *
 * 不在本 case 范围:
 *   - count=0 时多余 release 返回 <0 (归到 case 3 errors)
 */

#include "test_common.h"

#include <stdio.h>
#include "lisa_pm.h"
#include "unity.h"

static void test_lock_initial_state(void)
{
    int32_t count = lisa_pm_lock_get_count();
    bool blocked = lisa_pm_is_sleep_blocked();
    printf("[LOCK] initial: count=%ld blocked=%d\n",
           (long)count, (int)blocked);

    TEST_ASSERT_EQUAL_INT32(0, count);
    TEST_ASSERT_FALSE(blocked);
}

static void test_lock_acquire_release_basic(void)
{
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_lock_acquire());
    TEST_ASSERT_EQUAL_INT32(1, lisa_pm_lock_get_count());
    TEST_ASSERT_TRUE(lisa_pm_is_sleep_blocked());

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_lock_release());
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_lock_get_count());
    TEST_ASSERT_FALSE(lisa_pm_is_sleep_blocked());
}

static void test_lock_nested_acquire(void)
{
    /* acquire ×3 — count 1→2→3, 全程 blocked */
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_lock_acquire());
    TEST_ASSERT_EQUAL_INT32(1, lisa_pm_lock_get_count());
    TEST_ASSERT_TRUE(lisa_pm_is_sleep_blocked());

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_lock_acquire());
    TEST_ASSERT_EQUAL_INT32(2, lisa_pm_lock_get_count());
    TEST_ASSERT_TRUE(lisa_pm_is_sleep_blocked());

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_lock_acquire());
    TEST_ASSERT_EQUAL_INT32(3, lisa_pm_lock_get_count());
    TEST_ASSERT_TRUE(lisa_pm_is_sleep_blocked());

    /* release ×3 — count 2→1→0, blocked 仅在最后一步落回 false */
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_lock_release());
    TEST_ASSERT_EQUAL_INT32(2, lisa_pm_lock_get_count());
    TEST_ASSERT_TRUE(lisa_pm_is_sleep_blocked());

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_lock_release());
    TEST_ASSERT_EQUAL_INT32(1, lisa_pm_lock_get_count());
    TEST_ASSERT_TRUE(lisa_pm_is_sleep_blocked());

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_lock_release());
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_lock_get_count());
    TEST_ASSERT_FALSE(lisa_pm_is_sleep_blocked());
}

void run_lock_tests(void)
{
    RUN_TEST(test_lock_initial_state);
    RUN_TEST(test_lock_acquire_release_basic);
    RUN_TEST(test_lock_nested_acquire);
}
