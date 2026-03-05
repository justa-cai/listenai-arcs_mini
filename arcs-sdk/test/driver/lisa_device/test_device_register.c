/*
 * LISA Device Framework - 设备注册测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试设备注册相关功能
 */

#include "unity.h"
#include "lisa_device.h"
#include "test_common.h"
#include <string.h>

/* ========================================
 * 测试用设备定义
 * ======================================== */

/* 测试设备API结构 */
typedef struct {
    int dummy;
} test_device_api_t;

static test_device_api_t test_api_1 = {.dummy = 1};
static test_device_api_t test_api_2 = {.dummy = 2};

/* 私有数据 */
static int test_priv_data_1 = 100;
static int test_priv_data_2 = 200;

/* 注册测试设备 */
LISA_DEVICE_REGISTER(test_reg_device1, &test_api_1, &test_priv_data_1, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_NORMAL);

LISA_DEVICE_REGISTER(test_reg_device2, &test_api_2, &test_priv_data_2, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_HIGH);

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试静态设备注册是否成功
 */
void test_device_register_static(void)
{
    /* 获取注册的测试设备 */
    lisa_device_t *dev1 = lisa_device_get("test_reg_device1");
    lisa_device_t *dev2 = lisa_device_get("test_reg_device2");

    /* 验证设备存在 */
    TEST_ASSERT_NOT_NULL(dev1);
    TEST_ASSERT_NOT_NULL(dev2);

    /* 验证设备名称 */
    TEST_ASSERT_EQUAL_STRING("test_reg_device1", dev1->name);
    TEST_ASSERT_EQUAL_STRING("test_reg_device2", dev2->name);

    /* 验证API指针 */
    TEST_ASSERT_EQUAL_PTR(&test_api_1, dev1->api);
    TEST_ASSERT_EQUAL_PTR(&test_api_2, dev2->api);

    /* 验证私有数据 */
    TEST_ASSERT_EQUAL_PTR(&test_priv_data_1, dev1->priv_data);
    TEST_ASSERT_EQUAL_PTR(&test_priv_data_2, dev2->priv_data);
}

/**
 * @brief 测试设备名称的唯一性
 */
void test_device_register_unique_name(void)
{
    lisa_device_t *dev1 = lisa_device_get("test_reg_device1");
    lisa_device_t *dev2 = lisa_device_get("test_reg_device1");

    /* 同名设备应该返回同一个实例 */
    TEST_ASSERT_EQUAL_PTR(dev1, dev2);
}

/**
 * @brief 测试获取不存在的设备
 */
void test_device_register_not_found(void)
{
    lisa_device_t *dev = lisa_device_get("non_existent_device");
    
    /* 不存在的设备应该返回NULL */
    TEST_ASSERT_NULL(dev);
}

/**
 * @brief 测试空名称处理
 */
void test_device_register_null_name(void)
{
    lisa_device_t *dev = lisa_device_get(NULL);
    
    /* 空名称应该返回NULL */
    TEST_ASSERT_NULL(dev);
}

/**
 * @brief 测试空字符串名称处理
 */
void test_device_register_empty_name(void)
{
    lisa_device_t *dev = lisa_device_get("");
    
    /* 空字符串名称应该返回NULL */
    TEST_ASSERT_NULL(dev);
}

/**
 * @brief 测试设备的user_data字段
 */
void test_device_register_user_data(void)
{
    lisa_device_t *dev1 = lisa_device_get("test_reg_device1");
    
    TEST_ASSERT_NOT_NULL(dev1);
    
    /* 初始user_data应为NULL */
    TEST_ASSERT_NULL(dev1->user_data);
    
    /* 可以设置user_data */
    int user_value = 999;
    dev1->user_data = &user_value;
    TEST_ASSERT_EQUAL_PTR(&user_value, dev1->user_data);
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_device_register_tests(void)
{
    RUN_TEST(test_device_register_static);
    RUN_TEST(test_device_register_unique_name);
    RUN_TEST(test_device_register_not_found);
    RUN_TEST(test_device_register_null_name);
    RUN_TEST(test_device_register_empty_name);
    RUN_TEST(test_device_register_user_data);
}
