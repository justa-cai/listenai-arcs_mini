/*
 * lisa_pm 测试 — case 7b: 系统睡眠锁端到端抑制 sleep
 *
 * 用 case 7a (test_stats.c) 已经独立验证过的 stats 作为"实际是否进了
 * sleep"的观察者, 测 lisa_pm_lock_acquire() 持锁期间芯片确实不进
 * light sleep。
 *
 * 三步链 (顺序敏感):
 *   1. baseline    不持锁 + delay → sleep_count > 0  (证明 baseline 在睡)
 *   2. blocked     持锁 + delay   → sleep_count == 0 (核心: 持锁不睡)
 *   3. resumed     释放 + delay   → sleep_count > 0  (释放后恢复)
 *
 * 顺序的意义: 单独看 (2) 的 sleep_count==0 可能是别的原因 (setUp 没起
 * 来 / stats 坏 / sleep 频率本来就低); 由 (1) 和 (3) 夹住 (2), 才能把
 * "锁的因果效应"和"环境噪声"分开。
 *
 * 关键时序细节:
 *   - acquire 必须在 reset_stats 之前, 才能保证 reset 之后 delay 期间
 *     里没有"reset 后 / acquire 前"的 sleep 漏进来
 *   - delay 用 3000ms, 在 case 7a 的实测里 baseline 稳定 sleep_count=3
 *     (~1Hz beacon-driven wake), 给 ≥1 留足 margin
 */

#include "test_common.h"

#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "lisa_pm.h"
#include "unity.h"

#define LBS_DELAY_MS 3000U

static void dump(const char *tag, const lisa_pm_stats_t *s)
{
    printf("[LBS:%s] lock_count=%ld sleep_count=%u total_sleep_us=%llu "
           "ratio=%u/10000\n",
           tag,
           (long)lisa_pm_lock_get_count(),
           (unsigned)s->sleep_count,
           (unsigned long long)s->total_sleep_us,
           (unsigned)lisa_pm_get_sleep_ratio());
}

static void test_lbs_baseline_sleep_accumulates(void)
{
    lisa_pm_stats_t s;

    /* 基线: 必须不持锁, 由 setUp 后状态保证。这里防御性 assert: */
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_lock_get_count());
    TEST_ASSERT_FALSE(lisa_pm_is_sleep_blocked());

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_reset_stats());
    vTaskDelay(pdMS_TO_TICKS(LBS_DELAY_MS));

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_get_stats(&s));
    dump("BASELINE", &s);

    /* 证明 baseline 真的在睡。前置 case (case 7a) 已证 sleep_count > 0
     * 在 3s 窗口下稳定; 这里要求一致行为, 不重新验证 stats 自身 */
    TEST_ASSERT_GREATER_THAN_UINT32(0, s.sleep_count);
    TEST_ASSERT_TRUE(s.total_sleep_us > 0U);
}

static void test_lbs_system_lock_blocks_sleep(void)
{
    lisa_pm_stats_t s;

    /* 关键时序: acquire 先于 reset_stats, 避免 reset 与 acquire 之间
     * 漏进一次 sleep 把 sleep_count 染脏 */
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_lock_acquire());
    TEST_ASSERT_EQUAL_INT32(1, lisa_pm_lock_get_count());
    TEST_ASSERT_TRUE(lisa_pm_is_sleep_blocked());

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_reset_stats());
    vTaskDelay(pdMS_TO_TICKS(LBS_DELAY_MS));

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_get_stats(&s));
    dump("LOCKED", &s);

    /* 核心断言: 持锁期间严格 0 次 sleep。pm_lock_acquire(PM_LOCK_APP)
     * 在 HAL 侧直接阻断 light_sleep 进入 → hook_exit 永不 fire →
     * sleep_count 不增。若实板观察到非 0, 说明 lock-vs-HAL 接缝有漏洞,
     * 不要放宽断言, 先排查 */
    TEST_ASSERT_EQUAL_UINT32(0, s.sleep_count);
    TEST_ASSERT_TRUE(s.total_sleep_us == 0U);

    /* cleanup, 给下一个 case 干净基线 */
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_lock_release());
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_lock_get_count());
    TEST_ASSERT_FALSE(lisa_pm_is_sleep_blocked());
}

static void test_lbs_release_resumes_sleep(void)
{
    lisa_pm_stats_t s;

    /* 由上一个 case 的 cleanup 保证。重新防御性 assert: */
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_lock_get_count());
    TEST_ASSERT_FALSE(lisa_pm_is_sleep_blocked());

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_reset_stats());
    vTaskDelay(pdMS_TO_TICKS(LBS_DELAY_MS));

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_get_stats(&s));
    dump("RESUMED", &s);

    /* 释放后 sleep 行为恢复, 与 baseline 对称。这是反证:
     * (1) sleep 在 baseline 在睡, (2) 持锁不睡, (3) 释放再睡
     * 三个一致才说明确实是 lock 在因果影响 sleep */
    TEST_ASSERT_GREATER_THAN_UINT32(0, s.sleep_count);
    TEST_ASSERT_TRUE(s.total_sleep_us > 0U);
}

void run_lock_blocks_sleep_tests(void)
{
    RUN_TEST(test_lbs_baseline_sleep_accumulates);
    RUN_TEST(test_lbs_system_lock_blocks_sleep);
    RUN_TEST(test_lbs_release_resumes_sleep);
}
