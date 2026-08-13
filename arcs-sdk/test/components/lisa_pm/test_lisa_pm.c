/*
 * lisa_pm 测试 — 主运行器
 *
 * 调用 test_common_setup() 完成 WiFi + lisa_pm + AUTO_LIGHT_SLEEP
 * 的标准环境，然后用 Unity 顺序执行各 case 入口 run_xxx_tests()。
 *
 * 添加新 case：在 test_common.h 加 run_<name>_tests() 声明，新建
 * test_<name>.c 实现，并在下方 main() 里追加调用。
 */

#include "test_common.h"

#include <stdio.h>
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (test_common_setup() != 0) {
        printf("[TEST] common setup failed\n");
        return -1;
    }

    UNITY_BEGIN();

    printf("\n>>> Running snapshot tests...\n");
    run_snapshot_tests();

    printf("\n>>> Running lock tests...\n");
    run_lock_tests();

    printf("\n>>> Running wifi_lock tests...\n");
    run_wifi_lock_tests();

    printf("\n>>> Running wakeup_cause tests...\n");
    run_wakeup_cause_tests();

    printf("\n>>> Running stats tests...\n");
    run_stats_tests();

    printf("\n>>> Running lock_blocks_sleep tests...\n");
    run_lock_blocks_sleep_tests();

    printf("\n>>> Running errors tests...\n");
    run_errors_tests();

    printf("\n>>> Running sleep_callback tests...\n");
    run_sleep_callback_tests();

    printf("\n>>> Running device_pm_attach tests...\n");
    run_device_pm_attach_tests();

    return UNITY_END();
}
