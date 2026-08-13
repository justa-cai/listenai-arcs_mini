/*
 * lisa_pm 测试 — case 6: 唤醒原因归一化
 *
 * 进入本 case 时, setUp + 前置 case 已让系统在 AUTO_LIGHT_SLEEP + LISTEN
 * 主循环中跑过大量深睡周期, lisa_pm_get_wakeup_cause() 应反映最近一次
 * 唤醒。当前测试环境唯一稳定可控的唤醒源为 WiFi (LISTEN beacon /
 * keep-alive), 强断言归一化结果为 LISA_PM_WAKEUP_WIFI。
 *
 * 若实板首次跑发现稳定返回非 WIFI 值, 应先查 components/lisa_pm/src/ 中
 * wakeup_cause 设置点确认归一化优先级, 再修正本断言, 不退化为弱断言。
 */

#include "test_common.h"

#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "lisa_pm.h"
#include "unity.h"

static void test_wakeup_cause_after_setup(void)
{
    lisa_pm_wakeup_cause_t cause = lisa_pm_get_wakeup_cause();
    printf("[WAKEUP] after setup+prior cases: cause=%d\n", (int)cause);

    TEST_ASSERT_EQUAL_INT(LISA_PM_WAKEUP_WIFI, cause);
}

static void test_wakeup_cause_after_explicit_delay(void)
{
    vTaskDelay(pdMS_TO_TICKS(2000));

    lisa_pm_wakeup_cause_t cause = lisa_pm_get_wakeup_cause();
    printf("[WAKEUP] after 2s delay: cause=%d\n", (int)cause);

    TEST_ASSERT_EQUAL_INT(LISA_PM_WAKEUP_WIFI, cause);
}

void run_wakeup_cause_tests(void)
{
    RUN_TEST(test_wakeup_cause_after_setup);
    RUN_TEST(test_wakeup_cause_after_explicit_delay);
}
