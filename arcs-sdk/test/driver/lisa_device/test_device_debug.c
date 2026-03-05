/*
 * LISA Device Framework - 调试接口测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试设备调试和维测相关功能
 */

#include "unity.h"
#include "lisa_device.h"
#include "test_common.h"
#include <string.h>

#ifdef CONFIG_LISA_DEVICE_DEBUG
#include "lisa_device_debug.h"

/* ========================================
 * 测试用设备定义
 * ======================================== */

LISA_DEVICE_REGISTER(test_debug_device1, NULL, NULL, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_NORMAL);

LISA_DEVICE_REGISTER(test_debug_device2, NULL, NULL, NULL, 
                     test_mock_init_fail, LISA_DEVICE_PRIORITY_NORMAL);

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试打印设备注册表
 */
void test_device_debug_print_registry(void)
{
    /* 调用打印函数，验证不崩溃 */
    lisa_device_print_registry();
    TEST_PASS();
}

/**
 * @brief 测试打印单个设备信息
 */
void test_device_debug_print_info(void)
{
    lisa_device_t *dev = lisa_device_get("test_debug_device1");
    TEST_ASSERT_NOT_NULL(dev);
    
    /* 调用打印函数，验证不崩溃 */
    lisa_device_print_info(dev);
    TEST_PASS();
}

/**
 * @brief 测试打印设备信息 - NULL设备
 */
void test_device_debug_print_info_null(void)
{
    /* NULL设备不应崩溃 */
    lisa_device_print_info(NULL);
    TEST_PASS();
}

/**
 * @brief 测试打印所有设备信息
 */
void test_device_debug_print_all(void)
{
    /* 调用打印所有设备函数，验证不崩溃 */
    lisa_device_print_all();
    TEST_PASS();
}

/**
 * @brief 测试验证设备注册表
 */
void test_device_debug_verify_registry(void)
{
    int ret = lisa_device_verify_registry();
    
    /* 验证应该返回成功或有效的错误码 */
    TEST_ASSERT_GREATER_OR_EQUAL(LISA_DEVICE_ERR_INVALID, ret);
}

/**
 * @brief 测试获取设备状态名称字符串
 */
void test_device_debug_get_state_name(void)
{
    /* 测试UNINITIALIZED状态 */
    const char *name_uninit = lisa_device_get_state_name(LISA_DEVICE_STATE_UNINITIALIZED);
    TEST_ASSERT_NOT_NULL(name_uninit);
    
    /* 测试INITIALIZED状态 */
    const char *name_init = lisa_device_get_state_name(LISA_DEVICE_STATE_INITIALIZED);
    TEST_ASSERT_NOT_NULL(name_init);
    
    /* 测试ERROR状态 */
    const char *name_error = lisa_device_get_state_name(LISA_DEVICE_STATE_ERROR);
    TEST_ASSERT_NOT_NULL(name_error);
    
    /* 验证不同状态返回不同的字符串 */
    TEST_ASSERT_NOT_EQUAL(name_uninit, name_init);
    TEST_ASSERT_NOT_EQUAL(name_init, name_error);
}

/**
 * @brief 测试打印成功初始化的设备
 */
void test_device_debug_print_success_device(void)
{
    lisa_device_t *dev = lisa_device_get("test_debug_device1");
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_TRUE(lisa_device_is_initialized(dev));
    
    /* 打印成功初始化的设备信息 */
    lisa_device_print_info(dev);
    TEST_PASS();
}

/**
 * @brief 测试打印失败初始化的设备
 */
void test_device_debug_print_fail_device(void)
{
    lisa_device_t *dev = lisa_device_get("test_debug_device2");
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_FALSE(lisa_device_is_initialized(dev));
    
    /* 打印失败初始化的设备信息 */
    lisa_device_print_info(dev);
    TEST_PASS();
}

/**
 * @brief 测试状态名称字符串非空
 */
void test_device_debug_state_name_not_empty(void)
{
    const char *name_uninit = lisa_device_get_state_name(LISA_DEVICE_STATE_UNINITIALIZED);
    const char *name_init = lisa_device_get_state_name(LISA_DEVICE_STATE_INITIALIZED);
    const char *name_error = lisa_device_get_state_name(LISA_DEVICE_STATE_ERROR);
    
    /* 验证字符串不为空 */
    TEST_ASSERT_GREATER_THAN(0, strlen(name_uninit));
    TEST_ASSERT_GREATER_THAN(0, strlen(name_init));
    TEST_ASSERT_GREATER_THAN(0, strlen(name_error));
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_device_debug_tests(void)
{
    RUN_TEST(test_device_debug_print_registry);
    RUN_TEST(test_device_debug_print_info);
    RUN_TEST(test_device_debug_print_info_null);
    RUN_TEST(test_device_debug_print_all);
    RUN_TEST(test_device_debug_verify_registry);
    RUN_TEST(test_device_debug_get_state_name);
    RUN_TEST(test_device_debug_print_success_device);
    RUN_TEST(test_device_debug_print_fail_device);
    RUN_TEST(test_device_debug_state_name_not_empty);
}

#else /* !CONFIG_LISA_DEVICE_DEBUG */

/* 如果未启用DEBUG，提供空实现 */
void run_device_debug_tests(void)
{
    /* DEBUG功能未启用，跳过测试 */
}

#endif /* CONFIG_LISA_DEVICE_DEBUG */
