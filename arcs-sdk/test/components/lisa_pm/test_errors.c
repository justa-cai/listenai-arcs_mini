/*
 * lisa_pm 测试 — case 3: 错误 / 边界路径
 *
 * 集中验证 lisa_pm 公共 API 上有 documented 错误返回 / 隐性契约的入口,
 * 全部同步操作, 不依赖 sleep 周期。每个 case 进入时假设基线干净:
 *   policy = AUTO_LIGHT_SLEEP, wifi_ps = LISTEN,
 *   lock_count = 0, wifi_lock_count = 0
 * (由 setUp + 前置 case 全部 cleanup 保证); 退出时基线不变。
 *
 * 范围:
 *   A. 引用计数 underflow (count=0 release)
 *   B. 越界 enum 拒绝 (invalid policy / invalid ps_mode / invalid interval)
 *   D. 幂等 init
 *
 * 不在范围:
 *   - lisa_pm_get_stats(NULL) → -1 (已在 case 7a)
 *   - LISTEN NULL config 走 default (归到 case 5 wifi ps round-trip)
 *   - 各 getter 无错误路径
 */

#include "test_common.h"

#include <stdio.h>
#include "lisa_pm.h"
#include "unity.h"

/* ---------- A. 引用计数 underflow ---------- */

static void test_lock_release_underflow(void)
{
    /* 基线: lock_count == 0 */
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_lock_get_count());
    TEST_ASSERT_FALSE(lisa_pm_is_sleep_blocked());

    /* count=0 时 release 必须 <0; 且不能让 count 变负 / blocked 变化 */
    int32_t ret = lisa_pm_lock_release();
    printf("[ERR] lock_release@count=0 ret=%ld count=%ld blocked=%d\n",
           (long)ret,
           (long)lisa_pm_lock_get_count(),
           (int)lisa_pm_is_sleep_blocked());

    TEST_ASSERT_TRUE(ret < 0);
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_lock_get_count());
    TEST_ASSERT_FALSE(lisa_pm_is_sleep_blocked());
}

static void test_wifi_lock_release_underflow(void)
{
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_wifi_lock_get_count());
    TEST_ASSERT_FALSE(lisa_pm_wifi_is_power_save_blocked());

    int32_t ret = lisa_pm_wifi_lock_release();
    printf("[ERR] wifi_lock_release@count=0 ret=%ld count=%ld blocked=%d\n",
           (long)ret,
           (long)lisa_pm_wifi_lock_get_count(),
           (int)lisa_pm_wifi_is_power_save_blocked());

    TEST_ASSERT_TRUE(ret < 0);
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_wifi_lock_get_count());
    TEST_ASSERT_FALSE(lisa_pm_wifi_is_power_save_blocked());
}

/* ---------- B. 越界 enum 拒绝 ---------- */

static void test_set_invalid_system_policy(void)
{
    /* 基线: AUTO_LIGHT_SLEEP */
    TEST_ASSERT_EQUAL_INT(LISA_PM_SYSTEM_POLICY_AUTO_LIGHT_SLEEP,
                          lisa_pm_get_system_policy());

    /* 越界值: lisa_pm_porting_arcs::apply_policy 的 switch 走 default → -1
     * lisa_pm_core::set_system_policy 检查到 apply 失败后, 不更新
     * s_system_policy (源码 lisa_pm_core.c:179-186) */
    int32_t ret = lisa_pm_set_system_policy((lisa_pm_system_policy_t)99);
    printf("[ERR] set_system_policy(99) ret=%ld get_after=%d\n",
           (long)ret, (int)lisa_pm_get_system_policy());

    TEST_ASSERT_TRUE(ret < 0);
    /* 状态无副作用泄漏: 仍是基线 */
    TEST_ASSERT_EQUAL_INT(LISA_PM_SYSTEM_POLICY_AUTO_LIGHT_SLEEP,
                          lisa_pm_get_system_policy());
}

static void test_wifi_set_invalid_ps_mode(void)
{
    /* 基线: LISTEN */
    TEST_ASSERT_EQUAL_INT(LISA_PM_WIFI_PS_LISTEN, lisa_pm_wifi_get_ps_mode());

    /* 越界 mode: lisa_pm_wifi.c::switch 走 default → ret = -1
     * 源码保证 ret != 0 时不更新 s_wifi_ps_mode (lisa_pm_wifi.c:137-139) */
    int32_t ret = lisa_pm_wifi_set_ps_mode((lisa_pm_wifi_ps_mode_t)99, NULL);
    printf("[ERR] wifi_set_ps_mode(99) ret=%ld get_after=%d\n",
           (long)ret, (int)lisa_pm_wifi_get_ps_mode());

    TEST_ASSERT_TRUE(ret < 0);
    TEST_ASSERT_EQUAL_INT(LISA_PM_WIFI_PS_LISTEN, lisa_pm_wifi_get_ps_mode());
}

static void test_wifi_set_listen_invalid_interval(void)
{
    /* 基线: LISTEN; 合法 interval 范围 [1, 19] (lisa_pm_wifi.c:16-17) */
    TEST_ASSERT_EQUAL_INT(LISA_PM_WIFI_PS_LISTEN, lisa_pm_wifi_get_ps_mode());

    /* 下界违例 interval=0 (< MIN=1) */
    lisa_pm_wifi_ps_config_t cfg_low = { .listen_interval = 0U };
    int32_t ret_low = lisa_pm_wifi_set_ps_mode(LISA_PM_WIFI_PS_LISTEN, &cfg_low);
    printf("[ERR] wifi_set_ps_mode(LISTEN, interval=0) ret=%ld get_after=%d\n",
           (long)ret_low, (int)lisa_pm_wifi_get_ps_mode());
    TEST_ASSERT_TRUE(ret_low < 0);
    TEST_ASSERT_EQUAL_INT(LISA_PM_WIFI_PS_LISTEN, lisa_pm_wifi_get_ps_mode());

    /* 上界违例 interval=20 (> MAX=19) */
    lisa_pm_wifi_ps_config_t cfg_high = { .listen_interval = 20U };
    int32_t ret_high = lisa_pm_wifi_set_ps_mode(LISA_PM_WIFI_PS_LISTEN, &cfg_high);
    printf("[ERR] wifi_set_ps_mode(LISTEN, interval=20) ret=%ld get_after=%d\n",
           (long)ret_high, (int)lisa_pm_wifi_get_ps_mode());
    TEST_ASSERT_TRUE(ret_high < 0);
    TEST_ASSERT_EQUAL_INT(LISA_PM_WIFI_PS_LISTEN, lisa_pm_wifi_get_ps_mode());

    /* validate 失败发生在 wifi_sta_set_listen_itv() / wifi_ps_mode_set() 之前
     * (lisa_pm_wifi.c:119-122), 所以 wifi 层 interval 也未被破坏 */
}

/* ---------- D. 幂等性 ---------- */

static void test_lisa_pm_init_idempotent(void)
{
    /* 拍照: 调用前的全部可观察状态 */
    lisa_pm_system_policy_t policy_before = lisa_pm_get_system_policy();
    lisa_pm_wifi_ps_mode_t ps_before = lisa_pm_wifi_get_ps_mode();
    int32_t lock_before = lisa_pm_lock_get_count();
    int32_t wifi_lock_before = lisa_pm_wifi_lock_get_count();
    lisa_pm_wakeup_cause_t cause_before = lisa_pm_get_wakeup_cause();

    /* 二次 init: lisa_pm_core.c:109-112 的 s_initialized 早返回路径,
     * 应当返回 0 (不是 <0), 且不做任何副作用 */
    int32_t ret = lisa_pm_init();
    printf("[ERR] lisa_pm_init() second-call ret=%ld\n", (long)ret);
    TEST_ASSERT_EQUAL_INT32(0, ret);

    /* 拍照: 调用后, 与调用前逐字段相等 */
    TEST_ASSERT_EQUAL_INT(policy_before, lisa_pm_get_system_policy());
    TEST_ASSERT_EQUAL_INT(ps_before, lisa_pm_wifi_get_ps_mode());
    TEST_ASSERT_EQUAL_INT32(lock_before, lisa_pm_lock_get_count());
    TEST_ASSERT_EQUAL_INT32(wifi_lock_before, lisa_pm_wifi_lock_get_count());
    TEST_ASSERT_EQUAL_INT(cause_before, lisa_pm_get_wakeup_cause());

    printf("[ERR] init idempotent: policy=%d ps=%d lock=%ld wifi_lock=%ld cause=%d\n",
           (int)policy_before, (int)ps_before,
           (long)lock_before, (long)wifi_lock_before, (int)cause_before);
}

void run_errors_tests(void)
{
    RUN_TEST(test_lock_release_underflow);
    RUN_TEST(test_wifi_lock_release_underflow);
    RUN_TEST(test_set_invalid_system_policy);
    RUN_TEST(test_wifi_set_invalid_ps_mode);
    RUN_TEST(test_wifi_set_listen_invalid_interval);
    RUN_TEST(test_lisa_pm_init_idempotent);
}
