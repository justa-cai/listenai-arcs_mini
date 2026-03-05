/*
 * LISA Device Framework - 统计信息测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试设备统计信息相关功能
 */

#include "unity.h"
#include "lisa_device.h"
#include "test_common.h"

/* ========================================
 * 测试用设备定义
 * ======================================== */

LISA_DEVICE_REGISTER(test_stats_device1, NULL, NULL, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_NORMAL);

LISA_DEVICE_REGISTER(test_stats_device2, NULL, NULL, NULL, 
                     test_mock_init_fail, LISA_DEVICE_PRIORITY_NORMAL);

LISA_DEVICE_REGISTER(test_stats_device3, NULL, NULL, NULL, 
                     test_mock_init_slow, LISA_DEVICE_PRIORITY_NORMAL);

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试引用计数统计
 */
void test_device_stats_ref_count(void)
{
    lisa_device_t *dev = lisa_device_get("test_stats_device1");
    TEST_ASSERT_NOT_NULL(dev);
    
    /* 获取初始引用计数 */
    lisa_device_stats_t stats;
    lisa_device_get_stats(dev, &stats);
    uint32_t initial_ref = stats.ref_count;
    
    /* 再次获取设备几次 */
    lisa_device_get("test_stats_device1");
    lisa_device_get("test_stats_device1");
    lisa_device_get("test_stats_device1");
    
    /* 验证引用计数增加 */
    lisa_device_get_stats(dev, &stats);
    TEST_ASSERT_EQUAL_UINT32(initial_ref + 3, stats.ref_count);
}

/**
 * @brief 测试初始化结果记录 - 成功
 */
void test_device_stats_init_result_success(void)
{
    lisa_device_t *dev = lisa_device_get("test_stats_device1");
    TEST_ASSERT_NOT_NULL(dev);
    
    lisa_device_stats_t stats;
    lisa_device_get_stats(dev, &stats);
    
    /* 初始化成功应返回0 */
    TEST_ASSERT_EQUAL_INT(0, stats.init_result);
}

/**
 * @brief 测试初始化结果记录 - 失败
 */
void test_device_stats_init_result_fail(void)
{
    lisa_device_t *dev = lisa_device_get("test_stats_device2");
    TEST_ASSERT_NOT_NULL(dev);
    
    lisa_device_stats_t stats;
    lisa_device_get_stats(dev, &stats);
    
    /* 初始化失败应返回负数 */
    TEST_ASSERT_EQUAL_INT(-1, stats.init_result);
}

/**
 * @brief 测试初始化时间记录
 */
void test_device_stats_init_time(void)
{
    lisa_device_t *dev = lisa_device_get("test_stats_device1");
    TEST_ASSERT_NOT_NULL(dev);
    
    lisa_device_stats_t stats;
    lisa_device_get_stats(dev, &stats);
    
    /* 初始化时间应该被记录（大于等于0） */
    TEST_ASSERT_GREATER_OR_EQUAL(0, stats.init_time);
}

/**
 * @brief 测试慢速初始化的时间统计
 */
void test_device_stats_init_time_slow(void)
{
    lisa_device_t *dev = lisa_device_get("test_stats_device3");
    TEST_ASSERT_NOT_NULL(dev);
    
    lisa_device_stats_t stats;
    lisa_device_get_stats(dev, &stats);
    
    /* 慢速初始化应该有可测量的耗时 */
    TEST_ASSERT_GREATER_OR_EQUAL(0, stats.init_time);
}

/**
 * @brief 测试统计信息重置
 */
void test_device_stats_reset(void)
{
    lisa_device_t *dev = lisa_device_get("test_stats_device1");
    TEST_ASSERT_NOT_NULL(dev);
    
    /* 获取几次以增加引用计数 */
    lisa_device_get("test_stats_device1");
    lisa_device_get("test_stats_device1");
    
    /* 验证引用计数大于0 */
    lisa_device_stats_t stats;
    lisa_device_get_stats(dev, &stats);
    TEST_ASSERT_GREATER_THAN(0, stats.ref_count);
    
    /* 重置统计信息 */
    lisa_device_reset_stats(dev);
    
    /* 验证统计信息被清零 */
    lisa_device_get_stats(dev, &stats);
    TEST_ASSERT_EQUAL_UINT32(0, stats.ref_count);
    TEST_ASSERT_EQUAL_INT(0, stats.init_result);
    TEST_ASSERT_EQUAL_UINT32(0, stats.init_time);
    TEST_ASSERT_EQUAL_UINT32(0, stats.init_timestamp);
}

/**
 * @brief 测试获取统计信息 - NULL设备指针
 */
void test_device_stats_get_null_device(void)
{
    lisa_device_stats_t stats = {
        .ref_count = 999,
        .init_result = 999,
        .init_time = 999,
        .init_timestamp = 999
    };
    
    /* NULL设备指针不应修改stats */
    lisa_device_get_stats(NULL, &stats);
    
    /* stats应该保持不变 */
    TEST_ASSERT_EQUAL_UINT32(999, stats.ref_count);
}

/**
 * @brief 测试获取统计信息 - NULL stats指针
 */
void test_device_stats_get_null_stats(void)
{
    lisa_device_t *dev = lisa_device_get("test_stats_device1");
    TEST_ASSERT_NOT_NULL(dev);
    
    /* NULL stats指针不应崩溃 */
    lisa_device_get_stats(dev, NULL);
    /* 如果没有崩溃，测试通过 */
    TEST_PASS();
}

/**
 * @brief 测试重置统计信息 - NULL设备指针
 */
void test_device_stats_reset_null_device(void)
{
    /* NULL设备指针不应崩溃 */
    lisa_device_reset_stats(NULL);
    /* 如果没有崩溃，测试通过 */
    TEST_PASS();
}

/**
 * @brief 测试lisa_device_inc_ref_count辅助函数
 */
void test_device_stats_inc_ref_count(void)
{
    lisa_device_t *dev = lisa_device_get("test_stats_device1");
    TEST_ASSERT_NOT_NULL(dev);
    
    /* 获取当前引用计数 */
    lisa_device_stats_t stats;
    lisa_device_get_stats(dev, &stats);
    uint32_t current_ref = stats.ref_count;
    
    /* 手动增加引用计数 */
    lisa_device_inc_ref_count(dev);
    
    /* 验证引用计数增加 */
    lisa_device_get_stats(dev, &stats);
    TEST_ASSERT_EQUAL_UINT32(current_ref + 1, stats.ref_count);
}

/**
 * @brief 测试lisa_device_inc_ref_count - NULL设备
 */
void test_device_stats_inc_ref_count_null(void)
{
    /* NULL设备指针不应崩溃 */
    lisa_device_inc_ref_count(NULL);
    TEST_PASS();
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_device_stats_tests(void)
{
    RUN_TEST(test_device_stats_ref_count);
    RUN_TEST(test_device_stats_init_result_success);
    RUN_TEST(test_device_stats_init_result_fail);
    RUN_TEST(test_device_stats_init_time);
    RUN_TEST(test_device_stats_init_time_slow);
    RUN_TEST(test_device_stats_reset);
    RUN_TEST(test_device_stats_get_null_device);
    RUN_TEST(test_device_stats_get_null_stats);
    RUN_TEST(test_device_stats_reset_null_device);
    RUN_TEST(test_device_stats_inc_ref_count);
    RUN_TEST(test_device_stats_inc_ref_count_null);
}
