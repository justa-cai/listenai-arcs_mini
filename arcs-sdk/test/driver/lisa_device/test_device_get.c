/*
 * LISA Device Framework - 设备获取测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试设备获取和引用计数相关功能
 */

#include "unity.h"
#include "lisa_device.h"
#include "test_common.h"

/* ========================================
 * 测试用设备定义
 * ======================================== */

typedef struct {
    int value;
} test_get_api_t;

static test_get_api_t test_get_api = {.value = 42};

/* 注册测试设备 */
LISA_DEVICE_REGISTER(test_get_device1, &test_get_api, NULL, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_NORMAL);

LISA_DEVICE_REGISTER(test_get_device2, NULL, NULL, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_NORMAL);

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试按名称获取设备
 */
void test_device_get_by_name(void)
{
    /* 获取设备 */
    lisa_device_t *dev = lisa_device_get("test_get_device1");
    
    /* 验证设备存在 */
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_STRING("test_get_device1", dev->name);
    TEST_ASSERT_EQUAL_PTR(&test_get_api, dev->api);
}

/**
 * @brief 测试获取不存在的设备返回NULL
 */
void test_device_get_not_found(void)
{
    lisa_device_t *dev = lisa_device_get("device_does_not_exist");
    
    /* 不存在的设备返回NULL */
    TEST_ASSERT_NULL(dev);
}

/**
 * @brief 测试引用计数自动增加
 */
void test_device_get_ref_count_increment(void)
{
    lisa_device_t *dev = lisa_device_get("test_get_device1");
    TEST_ASSERT_NOT_NULL(dev);
    
    /* 获取初始引用计数 */
    lisa_device_stats_t stats;
    lisa_device_get_stats(dev, &stats);
    uint32_t initial_ref = stats.ref_count;
    
    /* 再次获取设备 */
    lisa_device_t *dev2 = lisa_device_get("test_get_device1");
    TEST_ASSERT_EQUAL_PTR(dev, dev2);
    
    /* 验证引用计数增加 */
    lisa_device_get_stats(dev, &stats);
    TEST_ASSERT_EQUAL_UINT32(initial_ref + 1, stats.ref_count);
}

/**
 * @brief 测试多次获取同一设备
 */
void test_device_get_multiple_times(void)
{
    lisa_device_t *dev1 = lisa_device_get("test_get_device2");
    lisa_device_t *dev2 = lisa_device_get("test_get_device2");
    lisa_device_t *dev3 = lisa_device_get("test_get_device2");
    
    /* 多次获取应返回同一实例 */
    TEST_ASSERT_NOT_NULL(dev1);
    TEST_ASSERT_EQUAL_PTR(dev1, dev2);
    TEST_ASSERT_EQUAL_PTR(dev1, dev3);
    
    /* 验证引用计数 */
    lisa_device_stats_t stats;
    lisa_device_get_stats(dev1, &stats);
    TEST_ASSERT_GREATER_OR_EQUAL(3, stats.ref_count);
}

/**
 * @brief 测试NULL名称处理
 */
void test_device_get_null_name(void)
{
    lisa_device_t *dev = lisa_device_get(NULL);
    TEST_ASSERT_NULL(dev);
}

/**
 * @brief 测试空字符串名称处理
 */
void test_device_get_empty_name(void)
{
    lisa_device_t *dev = lisa_device_get("");
    TEST_ASSERT_NULL(dev);
}

/**
 * @brief 测试大小写敏感性
 */
void test_device_get_case_sensitive(void)
{
    lisa_device_t *dev1 = lisa_device_get("test_get_device1");
    lisa_device_t *dev2 = lisa_device_get("TEST_GET_DEVICE1");
    lisa_device_t *dev3 = lisa_device_get("Test_Get_Device1");
    
    /* 名称大小写敏感 */
    TEST_ASSERT_NOT_NULL(dev1);
    TEST_ASSERT_NULL(dev2);  /* 大写应该找不到 */
    TEST_ASSERT_NULL(dev3);  /* 混合大小写应该找不到 */
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_device_get_tests(void)
{
    RUN_TEST(test_device_get_by_name);
    RUN_TEST(test_device_get_not_found);
    RUN_TEST(test_device_get_ref_count_increment);
    RUN_TEST(test_device_get_multiple_times);
    RUN_TEST(test_device_get_null_name);
    RUN_TEST(test_device_get_empty_name);
    RUN_TEST(test_device_get_case_sensitive);
}
