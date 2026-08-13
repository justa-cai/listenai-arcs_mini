/*
 * lisa_pm 测试 — case 7a: stats 自身正确性
 *
 * 在用 stats 作为"睡眠真发生"的观察者去测其他行为（case 7b lock 抑制
 * sleep 等）之前, 先独立验证 stats 这个测量工具本身可信。所有断言都
 * 是 stats API/计数器自身的不变量, 不涉及锁、不依赖具体唤醒源数量。
 *
 * 三条独立证据 (要求同时成立才认为 stats 可信):
 *   A. stats hook fire: sleep_count / total_sleep_us 在已知应有 sleep
 *      的窗口后大于 0
 *   B. stats 内部一致: sum(wakeup_cause_count[]) == sleep_count
 *      (源码硬保证 — 每次 ++sleep_count 同时 ++wakeup_cause_count[cause])
 *   C. 跨 hook ID 互证: stats 的 wakeup_cause_count[WIFI] > 0 AND
 *      lisa_pm_get_wakeup_cause() == WIFI
 *      (stats 注册 PM_HOOK_ID_0, porting 注册 PM_HOOK_ID_1, 两者
 *      独立 slot, 都依赖 HAL pm_execute_exit_handler fire)
 *
 * 任一不满足都暴露具体故障层 (CONFIG_LISA_PM_STATS 未编入 / hook 未注册 /
 * cause map 错位 / HAL 未 fire 等), 不会被静默吞掉。
 */

#include "test_common.h"

#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "lisa_pm.h"
#include "unity.h"

#define STATS_DELAY_MS 3000U
#define WAKEUP_CAUSE_SLOTS (LISA_PM_WAKEUP_UNKNOWN + 1)

static void dump_stats(const char *tag, const lisa_pm_stats_t *s)
{
    printf("[STATS:%s] sleep_count=%u abort=%u total_sleep_us=%llu "
           "total_active_us=%llu last=%u max=%u ratio=%u/10000\n",
           tag,
           (unsigned)s->sleep_count,
           (unsigned)s->sleep_abort_count,
           (unsigned long long)s->total_sleep_us,
           (unsigned long long)s->total_active_us,
           (unsigned)s->last_sleep_us,
           (unsigned)s->max_sleep_us,
           (unsigned)lisa_pm_get_sleep_ratio());
    printf("[STATS:%s] cause_count[TIMER=%u RTC=%u BT=%u WIFI=%u GPIO=%u UNKNOWN=%u]\n",
           tag,
           (unsigned)s->wakeup_cause_count[LISA_PM_WAKEUP_TIMER],
           (unsigned)s->wakeup_cause_count[LISA_PM_WAKEUP_RTC],
           (unsigned)s->wakeup_cause_count[LISA_PM_WAKEUP_BT],
           (unsigned)s->wakeup_cause_count[LISA_PM_WAKEUP_WIFI],
           (unsigned)s->wakeup_cause_count[LISA_PM_WAKEUP_GPIO],
           (unsigned)s->wakeup_cause_count[LISA_PM_WAKEUP_UNKNOWN]);
}

static void test_stats_get_null(void)
{
    TEST_ASSERT_EQUAL_INT32(-1, lisa_pm_get_stats(NULL));
}

static void test_stats_reset_zeros_all(void)
{
    lisa_pm_stats_t s;

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_reset_stats());
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_get_stats(&s));
    dump_stats("AFTER_RESET", &s);

    TEST_ASSERT_EQUAL_UINT32(0, s.sleep_count);
    TEST_ASSERT_EQUAL_UINT32(0, s.sleep_abort_count);
    /* total_*_us 是 uint64_t; 当前 build (RV32) 没启用 Unity 64-bit 比较宏,
     * 改用 TEST_ASSERT_TRUE + 手工比较, 失败时配合上面的 dump_stats 看上下文 */
    TEST_ASSERT_TRUE(s.total_sleep_us == 0U);
    TEST_ASSERT_TRUE(s.total_active_us == 0U);
    TEST_ASSERT_EQUAL_UINT32(0, s.last_sleep_us);
    TEST_ASSERT_EQUAL_UINT32(0, s.max_sleep_us);

    for (uint32_t i = 0; i < WAKEUP_CAUSE_SLOTS; ++i) {
        TEST_ASSERT_EQUAL_UINT32(0, s.wakeup_cause_count[i]);
    }

    /* sleep_ratio = total_sleep_us / (total_sleep_us + total_active_us);
     * 两个分母都 0 时实现返回 0 (lisa_pm_stats.c:104-106) */
    TEST_ASSERT_EQUAL_UINT32(0, lisa_pm_get_sleep_ratio());
}

static void test_stats_counters_increment_after_sleep(void)
{
    lisa_pm_stats_t s;

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_reset_stats());
    vTaskDelay(pdMS_TO_TICKS(STATS_DELAY_MS));
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_get_stats(&s));
    dump_stats("AFTER_DELAY", &s);

    /* 证据 A: stats hook 真的 fire 了 */
    TEST_ASSERT_GREATER_THAN_UINT32(0, s.sleep_count);
    TEST_ASSERT_TRUE(s.total_sleep_us > 0U);  /* uint64_t, 见 reset case 注释 */
}

static void test_stats_cause_count_sums_to_sleep_count(void)
{
    lisa_pm_stats_t s;
    uint32_t sum = 0;

    /* 沿用上一个 case 留下的状态 (不 reset); 也可独立 reset+delay,
     * 但本断言关注的是 stats 内部不变量, 任何已经累积的状态都成立 */
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_get_stats(&s));
    dump_stats("SUM_CHECK", &s);

    for (uint32_t i = 0; i < WAKEUP_CAUSE_SLOTS; ++i) {
        sum += s.wakeup_cause_count[i];
    }

    printf("[STATS:SUM_CHECK] sum(cause_count[])=%u sleep_count=%u\n",
           (unsigned)sum, (unsigned)s.sleep_count);

    /* 证据 B: lisa_pm_stats.c::hook_exit 每次 ++sleep_count 都同时
     * ++wakeup_cause_count[cause] 一次, 所以两者必须严格相等。
     * 这条不变量独立于具体哪个 cause 被命中, 也独立于 cause map 是否正确 */
    TEST_ASSERT_EQUAL_UINT32(s.sleep_count, sum);
}

static void test_stats_cross_witness_with_porting_hook(void)
{
    lisa_pm_stats_t s;
    lisa_pm_wakeup_cause_t cause;

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_reset_stats());
    vTaskDelay(pdMS_TO_TICKS(STATS_DELAY_MS));

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_get_stats(&s));
    cause = lisa_pm_get_wakeup_cause();
    dump_stats("CROSS_WITNESS", &s);
    printf("[STATS:CROSS_WITNESS] get_wakeup_cause=%d\n", (int)cause);

    /* 证据 C: stats hook (PM_HOOK_ID_0) 与 porting hook (PM_HOOK_ID_1)
     * 是 HAL 维护的两个独立 slot, 都依赖 pm_execute_exit_handler 遍历
     * 触发。若任一未注册 / 未 fire, 这两条至少有一条不满足: */
    TEST_ASSERT_GREATER_THAN_UINT32(0, s.wakeup_cause_count[LISA_PM_WAKEUP_WIFI]);
    TEST_ASSERT_EQUAL_INT(LISA_PM_WAKEUP_WIFI, cause);
}

static void test_stats_field_self_consistency(void)
{
    lisa_pm_stats_t s;
    uint32_t ratio;

    /* 沿用上一个 case 累积的状态 */
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_get_stats(&s));
    ratio = lisa_pm_get_sleep_ratio();
    dump_stats("SELF_CONSISTENCY", &s);
    printf("[STATS:SELF_CONSISTENCY] ratio=%u/10000\n", (unsigned)ratio);

    /* max_sleep_us 是单次最大睡眠时长, 必然 <= 累计 total_sleep_us
     * (total 是 uint64; 见 reset case 注释) */
    TEST_ASSERT_GREATER_THAN_UINT32(0, s.max_sleep_us);
    TEST_ASSERT_TRUE((uint64_t)s.max_sleep_us <= s.total_sleep_us);

    /* sleep_ratio 是万分比, 上限 10000; AUTO_LIGHT_SLEEP 下应当 > 0 */
    TEST_ASSERT_GREATER_THAN_UINT32(0, ratio);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(10000U, ratio);

    /* last_sleep_us 是最近一次睡眠时长, 应 <= max_sleep_us */
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(s.max_sleep_us, s.last_sleep_us);
}

void run_stats_tests(void)
{
    RUN_TEST(test_stats_get_null);
    RUN_TEST(test_stats_reset_zeros_all);
    RUN_TEST(test_stats_counters_increment_after_sleep);
    RUN_TEST(test_stats_cause_count_sums_to_sleep_count);
    RUN_TEST(test_stats_cross_witness_with_porting_hook);
    RUN_TEST(test_stats_field_self_consistency);
}
