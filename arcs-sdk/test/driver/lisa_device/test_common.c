/*
 * LISA Device Framework - 测试公共函数实现
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_common.h"
#include <lisa_time.h>

/* ========================================
 * 模拟设备初始化函数实现
 * ======================================== */

/**
 * @brief 模拟设备初始化成功
 * @return 0 表示成功
 */
int test_mock_init_success(void)
{
    /* 模拟成功初始化，立即返回0 */
    return 0;
}

/**
 * @brief 模拟设备初始化失败
 * @return -1 表示失败
 */
int test_mock_init_fail(void)
{
    /* 模拟失败初始化，返回负数错误码 */
    return -1;
}

/**
 * @brief 模拟设备初始化慢速(用于测试耗时统计)
 * @return 0 表示成功
 */
int test_mock_init_slow(void)
{
    /* 模拟慢速初始化，延迟一段时间 */
    for(int i = 0; i < 1000000; i++) {
        asm("nop");
    }
    return 0;
}
