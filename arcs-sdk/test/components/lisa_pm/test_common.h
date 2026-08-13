/*
 * lisa_pm 测试工程 — 通用头文件
 *
 * 声明：
 *   - 公共环境 setup（WiFi + lisa_pm + 进入 AUTO_LIGHT_SLEEP）
 *   - 各 case 入口 run_xxx_tests()
 *
 * 添加新 case 时：
 *   1. 新建 test_<name>.c 实现 run_<name>_tests()
 *   2. 在本文件追加 run_<name>_tests() 声明
 *   3. 在 test_lisa_pm.c::main 里调用 run_<name>_tests()
 *   4. 在 CMakeLists.txt 把 test_<name>.c 加进 target_sources
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 完成 lisa_pm + WiFi 连接 + 进入 AUTO_LIGHT_SLEEP 的公共环境。
 *
 * 同步等待 WiFi 连上并拿到 IP，然后启用 WiFi LISTEN 模式 + lisa_pm
 * AUTO_LIGHT_SLEEP 系统策略。返回后系统已进入低功耗循环，case 即可
 * 在 vTaskDelay 期间观察深度睡眠/唤醒行为。
 *
 * @return 0 成功，<0 失败
 */
int test_common_setup(void);

/* 各 case 入口（添加新 case 时在此声明） */
void run_snapshot_tests(void);
void run_lock_tests(void);
void run_wifi_lock_tests(void);
void run_wakeup_cause_tests(void);
void run_stats_tests(void);
void run_lock_blocks_sleep_tests(void);
void run_errors_tests(void);
void run_sleep_callback_tests(void);
void run_device_pm_attach_tests(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_COMMON_H */
