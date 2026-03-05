/*
 * LISA Device Framework - 测试公共头文件
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================
 * 测试辅助函数声明
 * ======================================== */

/* 声明各个测试模块的运行函数 */
void run_device_register_tests(void);
void run_device_get_tests(void);
void run_device_state_tests(void);
void run_device_stats_tests(void);
void run_device_foreach_tests(void);
void run_device_priority_tests(void);
void run_device_query_tests(void);
#ifdef CONFIG_LISA_DEVICE_DEBUG
void run_device_debug_tests(void);
#endif

/* ========================================
 * 测试辅助宏定义
 * ======================================== */

#define TEST_DEVICE_NAME_MAX 32

/* ========================================
 * 模拟设备初始化函数
 * ======================================== */

/**
 * @brief 模拟设备初始化成功
 * @return 0 表示成功
 */
int test_mock_init_success(void);

/**
 * @brief 模拟设备初始化失败
 * @return -1 表示失败
 */
int test_mock_init_fail(void);

/**
 * @brief 模拟设备初始化慢速(用于测试耗时统计)
 * @return 0 表示成功
 */
int test_mock_init_slow(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_COMMON_H */
