/*
 * LISA Device Framework - 查询接口测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试设备查询相关功能
 */

#include "unity.h"
#include "lisa_device.h"
#include "test_common.h"

/* ========================================
 * 测试用设备定义
 * ======================================== */

LISA_DEVICE_REGISTER(test_query_device1, NULL, NULL, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_NORMAL);

LISA_DEVICE_REGISTER(test_query_device2, NULL, NULL, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_NORMAL);

LISA_DEVICE_REGISTER(test_query_device3, NULL, NULL, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_NORMAL);

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试获取设备数量
 */
void test_device_get_count_basic(void)
{
    uint32_t count = lisa_device_get_count();
    
    /* 设备数量应该大于0（至少有我们注册的测试设备） */
    TEST_ASSERT_GREATER_THAN(0, count);
    
    /* 至少应该有我们注册的3个测试设备 */
    TEST_ASSERT_GREATER_OR_EQUAL(3, count);
}

/* 回调函数用于计数 */
static int query_count_callback(lisa_device_t *dev, void *user_data)
{
    (void)dev;
    int *counter = (int *)user_data;
    (*counter)++;
    return 0;
}

/**
 * @brief 测试设备数量与遍历结果一致
 */
void test_device_get_count_matches_foreach(void)
{
    /* 获取设备数量 */
    uint32_t count = lisa_device_get_count();
    
    /* 通过遍历计数 */
    int foreach_count = 0;
    int traverse_count = lisa_device_foreach(query_count_callback, &foreach_count);
    
    /* 两种方式获取的数量应该一致 */
    TEST_ASSERT_EQUAL_UINT32(count, traverse_count);
    TEST_ASSERT_EQUAL_UINT32(count, foreach_count);
}

/**
 * @brief 测试多次调用get_count返回相同结果
 */
void test_device_get_count_consistent(void)
{
    uint32_t count1 = lisa_device_get_count();
    uint32_t count2 = lisa_device_get_count();
    uint32_t count3 = lisa_device_get_count();
    
    /* 多次调用应返回相同结果 */
    TEST_ASSERT_EQUAL_UINT32(count1, count2);
    TEST_ASSERT_EQUAL_UINT32(count2, count3);
}

/**
 * @brief 测试设备数量非零
 */
void test_device_get_count_non_zero(void)
{
    uint32_t count = lisa_device_get_count();
    
    /* 应该至少有一些设备被注册 */
    TEST_ASSERT_NOT_EQUAL(0, count);
}

/**
 * @brief 测试能够找到我们注册的查询测试设备
 */
void test_device_get_count_includes_test_devices(void)
{
    /* 验证我们的测试设备存在 */
    lisa_device_t *dev1 = lisa_device_get("test_query_device1");
    lisa_device_t *dev2 = lisa_device_get("test_query_device2");
    lisa_device_t *dev3 = lisa_device_get("test_query_device3");
    
    TEST_ASSERT_NOT_NULL(dev1);
    TEST_ASSERT_NOT_NULL(dev2);
    TEST_ASSERT_NOT_NULL(dev3);
    
    /* 设备总数应该至少包含这3个 */
    uint32_t total_count = lisa_device_get_count();
    TEST_ASSERT_GREATER_OR_EQUAL(3, total_count);
}

/**
 * @brief 测试设备计数的合理范围
 */
void test_device_get_count_reasonable_range(void)
{
    uint32_t count = lisa_device_get_count();
    
    /* 设备数量应该在合理范围内 
     * 最少3个（我们的测试设备），最多不超过1000（合理上限） */
    TEST_ASSERT_GREATER_OR_EQUAL(3, count);
    TEST_ASSERT_LESS_THAN(1000, count);
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_device_query_tests(void)
{
    RUN_TEST(test_device_get_count_basic);
    RUN_TEST(test_device_get_count_matches_foreach);
    RUN_TEST(test_device_get_count_consistent);
    RUN_TEST(test_device_get_count_non_zero);
    RUN_TEST(test_device_get_count_includes_test_devices);
    RUN_TEST(test_device_get_count_reasonable_range);
}
