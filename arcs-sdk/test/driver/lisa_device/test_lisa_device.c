/*
 * LISA Device Framework - 测试主程序
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 本文件作为测试运行器，负责初始化设备管理器并运行所有测试用例
 */

#include "unity.h"
#include "lisa_device.h"
#include "test_common.h"
#include <stdio.h>

/* ========================================
 * Unity 测试框架钩子函数
 * ======================================== */

/**
 * @brief 在每个测试用例之前执行
 */
void setUp(void)
{
    /* 可以在此处添加通用的测试前置操作 */
}

/**
 * @brief 在每个测试用例之后执行
 */
void tearDown(void)
{
    /* 可以在此处添加通用的测试清理操作 */
}

/* ========================================
 * 主函数
 * ======================================== */

int main(void)
{
    /* 初始化设备管理器 */
    int init_count = lisa_device_init();
    printf("\n========================================\n");
    printf("LISA Device Manager Initialized\n");
    printf("Registered devices: %d\n", init_count);
    printf("========================================\n\n");
    
    /* 开始运行测试套件 */
    UNITY_BEGIN();
    
    /* 运行各个测试模块 */
    printf("\n>>> Running Device Register Tests...\n");
    run_device_register_tests();
    
    printf("\n>>> Running Device Get Tests...\n");
    run_device_get_tests();
    
    printf("\n>>> Running Device State Tests...\n");
    run_device_state_tests();
    
    printf("\n>>> Running Device Stats Tests...\n");
    run_device_stats_tests();
    
    printf("\n>>> Running Device Foreach Tests...\n");
    run_device_foreach_tests();
    
    printf("\n>>> Running Device Priority Tests...\n");
    run_device_priority_tests();
    
    printf("\n>>> Running Device Query Tests...\n");
    run_device_query_tests();
    
#ifdef CONFIG_LISA_DEVICE_DEBUG
    printf("\n>>> Running Device Debug Tests...\n");
    run_device_debug_tests();
#endif
    
    printf("\n========================================\n");
    printf("All Tests Completed\n");
    printf("========================================\n");
    
    return UNITY_END();
}
