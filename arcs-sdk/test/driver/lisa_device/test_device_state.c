/*
 * LISA Device Framework - 设备状态测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试设备状态管理相关功能
 */

#include "unity.h"
#include "lisa_device.h"
#include "test_common.h"

/* ========================================
 * 测试用设备定义
 * ======================================== */

/* 注册不同初始化状态的测试设备 */
LISA_DEVICE_REGISTER(test_state_success, NULL, NULL, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_NORMAL);

LISA_DEVICE_REGISTER(test_state_fail, NULL, NULL, NULL, 
                     test_mock_init_fail, LISA_DEVICE_PRIORITY_NORMAL);

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试初始化成功后的设备状态
 */
void test_device_state_initialized_success(void)
{
    lisa_device_t *dev = lisa_device_get("test_state_success");
    TEST_ASSERT_NOT_NULL(dev);
    
    /* 初始化成功的设备状态应该是 INITIALIZED */
    TEST_ASSERT_EQUAL(LISA_DEVICE_STATE_INITIALIZED, lisa_device_get_state(dev));
}

/**
 * @brief 测试初始化失败后的设备状态
 */
void test_device_state_error_on_init_fail(void)
{
    lisa_device_t *dev = lisa_device_get("test_state_fail");
    TEST_ASSERT_NOT_NULL(dev);
    
    /* 初始化失败的设备状态应该是 ERROR */
    TEST_ASSERT_EQUAL(LISA_DEVICE_STATE_ERROR, lisa_device_get_state(dev));
}

/**
 * @brief 测试设备就绪检查 - 成功初始化的设备
 */
void test_device_ready_check_success(void)
{
    lisa_device_t *dev = lisa_device_get("test_state_success");
    TEST_ASSERT_NOT_NULL(dev);
    
    /* 初始化成功的设备应该就绪 */
    TEST_ASSERT_TRUE(lisa_device_ready(dev));
}

/**
 * @brief 测试设备就绪检查 - 初始化失败的设备
 */
void test_device_ready_check_fail(void)
{
    lisa_device_t *dev = lisa_device_get("test_state_fail");
    TEST_ASSERT_NOT_NULL(dev);
    
    /* 初始化失败的设备不应就绪 */
    TEST_ASSERT_FALSE(lisa_device_ready(dev));
}

/**
 * @brief 测试设备就绪检查 - NULL设备指针
 */
void test_device_ready_null_device(void)
{
    /* NULL设备指针应返回false */
    TEST_ASSERT_FALSE(lisa_device_ready(NULL));
}

/**
 * @brief 测试lisa_device_is_initialized辅助函数 - 成功设备
 */
void test_device_is_initialized_success(void)
{
    lisa_device_t *dev = lisa_device_get("test_state_success");
    TEST_ASSERT_NOT_NULL(dev);
    
    /* 初始化成功的设备应返回true */
    TEST_ASSERT_TRUE(lisa_device_is_initialized(dev));
}

/**
 * @brief 测试lisa_device_is_initialized辅助函数 - 失败设备
 */
void test_device_is_initialized_fail(void)
{
    lisa_device_t *dev = lisa_device_get("test_state_fail");
    TEST_ASSERT_NOT_NULL(dev);
    
    /* 初始化失败的设备应返回false */
    TEST_ASSERT_FALSE(lisa_device_is_initialized(dev));
}

/**
 * @brief 测试lisa_device_is_initialized辅助函数 - NULL设备
 */
void test_device_is_initialized_null(void)
{
    /* NULL设备应返回false */
    TEST_ASSERT_FALSE(lisa_device_is_initialized(NULL));
}

/**
 * @brief 测试lisa_device_get_state - NULL设备
 */
void test_device_get_state_null(void)
{
    /* NULL设备应返回UNINITIALIZED */
    TEST_ASSERT_EQUAL(LISA_DEVICE_STATE_UNINITIALIZED, lisa_device_get_state(NULL));
}

/**
 * @brief 测试状态的不同值
 */
void test_device_state_values(void)
{
    lisa_device_t *dev_success = lisa_device_get("test_state_success");
    lisa_device_t *dev_fail = lisa_device_get("test_state_fail");
    
    TEST_ASSERT_NOT_NULL(dev_success);
    TEST_ASSERT_NOT_NULL(dev_fail);
    
    /* 验证成功和失败设备的状态不同 */
    TEST_ASSERT_NOT_EQUAL(dev_success->state, dev_fail->state);
    
    /* 验证具体状态值 */
    TEST_ASSERT_EQUAL(LISA_DEVICE_STATE_INITIALIZED, dev_success->state);
    TEST_ASSERT_EQUAL(LISA_DEVICE_STATE_ERROR, dev_fail->state);
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_device_state_tests(void)
{
    RUN_TEST(test_device_state_initialized_success);
    RUN_TEST(test_device_state_error_on_init_fail);
    RUN_TEST(test_device_ready_check_success);
    RUN_TEST(test_device_ready_check_fail);
    RUN_TEST(test_device_ready_null_device);
    RUN_TEST(test_device_is_initialized_success);
    RUN_TEST(test_device_is_initialized_fail);
    RUN_TEST(test_device_is_initialized_null);
    RUN_TEST(test_device_get_state_null);
    RUN_TEST(test_device_state_values);
}
