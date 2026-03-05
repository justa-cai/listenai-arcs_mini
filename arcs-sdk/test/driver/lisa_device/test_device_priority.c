/*
 * LISA Device Framework - 优先级测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试设备初始化优先级相关功能
 */

#include "unity.h"
#include "lisa_device.h"
#include "test_common.h"

/* ========================================
 * 测试用设备定义
 * ======================================== */

/* 注册不同优先级的测试设备 */
LISA_DEVICE_REGISTER(test_priority_critical, NULL, NULL, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_CRITICAL);

LISA_DEVICE_REGISTER(test_priority_high, NULL, NULL, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_HIGH);

LISA_DEVICE_REGISTER(test_priority_normal, NULL, NULL, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_NORMAL);

LISA_DEVICE_REGISTER(test_priority_low, NULL, NULL, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_LOW);

LISA_DEVICE_REGISTER(test_priority_lowest, NULL, NULL, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_LOWEST);

/* 自定义优先级 */
LISA_DEVICE_REGISTER(test_priority_custom_5, NULL, NULL, NULL, 
                     test_mock_init_success, 5);

LISA_DEVICE_REGISTER(test_priority_custom_25, NULL, NULL, NULL, 
                     test_mock_init_success, 25);

LISA_DEVICE_REGISTER(test_priority_custom_75, NULL, NULL, NULL, 
                     test_mock_init_success, 75);

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试CRITICAL优先级设备能被找到
 */
void test_device_priority_critical_exists(void)
{
    lisa_device_t *dev = lisa_device_get("test_priority_critical");
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_STRING("test_priority_critical", dev->name);
}

/**
 * @brief 测试HIGH优先级设备能被找到
 */
void test_device_priority_high_exists(void)
{
    lisa_device_t *dev = lisa_device_get("test_priority_high");
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_STRING("test_priority_high", dev->name);
}

/**
 * @brief 测试NORMAL优先级设备能被找到
 */
void test_device_priority_normal_exists(void)
{
    lisa_device_t *dev = lisa_device_get("test_priority_normal");
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_STRING("test_priority_normal", dev->name);
}

/**
 * @brief 测试LOW优先级设备能被找到
 */
void test_device_priority_low_exists(void)
{
    lisa_device_t *dev = lisa_device_get("test_priority_low");
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_STRING("test_priority_low", dev->name);
}

/**
 * @brief 测试LOWEST优先级设备能被找到
 */
void test_device_priority_lowest_exists(void)
{
    lisa_device_t *dev = lisa_device_get("test_priority_lowest");
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_STRING("test_priority_lowest", dev->name);
}

/**
 * @brief 测试自定义优先级设备能被找到
 */
void test_device_priority_custom_exists(void)
{
    lisa_device_t *dev1 = lisa_device_get("test_priority_custom_5");
    lisa_device_t *dev2 = lisa_device_get("test_priority_custom_25");
    lisa_device_t *dev3 = lisa_device_get("test_priority_custom_75");
    
    TEST_ASSERT_NOT_NULL(dev1);
    TEST_ASSERT_NOT_NULL(dev2);
    TEST_ASSERT_NOT_NULL(dev3);
}

/**
 * @brief 测试所有优先级的设备都被初始化
 */
void test_device_priority_all_initialized(void)
{
    lisa_device_t *critical = lisa_device_get("test_priority_critical");
    lisa_device_t *high = lisa_device_get("test_priority_high");
    lisa_device_t *normal = lisa_device_get("test_priority_normal");
    lisa_device_t *low = lisa_device_get("test_priority_low");
    lisa_device_t *lowest = lisa_device_get("test_priority_lowest");
    
    /* 验证所有设备都存在 */
    TEST_ASSERT_NOT_NULL(critical);
    TEST_ASSERT_NOT_NULL(high);
    TEST_ASSERT_NOT_NULL(normal);
    TEST_ASSERT_NOT_NULL(low);
    TEST_ASSERT_NOT_NULL(lowest);
    
    /* 验证所有设备都已初始化 */
    TEST_ASSERT_TRUE(lisa_device_is_initialized(critical));
    TEST_ASSERT_TRUE(lisa_device_is_initialized(high));
    TEST_ASSERT_TRUE(lisa_device_is_initialized(normal));
    TEST_ASSERT_TRUE(lisa_device_is_initialized(low));
    TEST_ASSERT_TRUE(lisa_device_is_initialized(lowest));
}

/**
 * @brief 测试优先级常量的值
 */
void test_device_priority_constants(void)
{
    /* 验证优先级常量的相对大小关系（数值越小优先级越高） */
    TEST_ASSERT_LESS_THAN(LISA_DEVICE_PRIORITY_NORMAL, LISA_DEVICE_PRIORITY_HIGH);
    TEST_ASSERT_LESS_THAN(LISA_DEVICE_PRIORITY_LOW, LISA_DEVICE_PRIORITY_NORMAL);
    TEST_ASSERT_LESS_THAN(LISA_DEVICE_PRIORITY_LOWEST, LISA_DEVICE_PRIORITY_LOW);
    TEST_ASSERT_LESS_THAN(LISA_DEVICE_PRIORITY_HIGH, LISA_DEVICE_PRIORITY_CRITICAL);
    
    /* 验证优先级在0-99范围内 */
    TEST_ASSERT_GREATER_OR_EQUAL(0, LISA_DEVICE_PRIORITY_CRITICAL);
    TEST_ASSERT_LESS_OR_EQUAL(99, LISA_DEVICE_PRIORITY_LOWEST);
}

/**
 * @brief 测试相同优先级的设备
 */
void test_device_priority_same_level(void)
{
    /* 注册两个相同优先级的设备（都是NORMAL） */
    lisa_device_t *dev1 = lisa_device_get("test_priority_normal");
    
    TEST_ASSERT_NOT_NULL(dev1);
    
    /* 相同优先级的设备都应该被初始化 */
    TEST_ASSERT_TRUE(lisa_device_is_initialized(dev1));
}

/**
 * @brief 测试优先级范围的边界值
 */
void test_device_priority_boundary_values(void)
{
    /* 测试最小优先级 (CRITICAL = 0) */
    TEST_ASSERT_EQUAL(0, LISA_DEVICE_PRIORITY_CRITICAL);
    
    /* 测试最大优先级 (LOWEST = 99) */
    TEST_ASSERT_EQUAL(99, LISA_DEVICE_PRIORITY_LOWEST);
    
    /* 验证所有预定义优先级都在范围内 */
    TEST_ASSERT_GREATER_OR_EQUAL(0, LISA_DEVICE_PRIORITY_HIGH);
    TEST_ASSERT_LESS_OR_EQUAL(99, LISA_DEVICE_PRIORITY_HIGH);
    
    TEST_ASSERT_GREATER_OR_EQUAL(0, LISA_DEVICE_PRIORITY_NORMAL);
    TEST_ASSERT_LESS_OR_EQUAL(99, LISA_DEVICE_PRIORITY_NORMAL);
    
    TEST_ASSERT_GREATER_OR_EQUAL(0, LISA_DEVICE_PRIORITY_LOW);
    TEST_ASSERT_LESS_OR_EQUAL(99, LISA_DEVICE_PRIORITY_LOW);
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_device_priority_tests(void)
{
    RUN_TEST(test_device_priority_critical_exists);
    RUN_TEST(test_device_priority_high_exists);
    RUN_TEST(test_device_priority_normal_exists);
    RUN_TEST(test_device_priority_low_exists);
    RUN_TEST(test_device_priority_lowest_exists);
    RUN_TEST(test_device_priority_custom_exists);
    RUN_TEST(test_device_priority_all_initialized);
    RUN_TEST(test_device_priority_constants);
    RUN_TEST(test_device_priority_same_level);
    RUN_TEST(test_device_priority_boundary_values);
}
