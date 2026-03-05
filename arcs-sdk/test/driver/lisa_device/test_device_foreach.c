/*
 * LISA Device Framework - 设备遍历测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试设备遍历相关功能
 */

#include "unity.h"
#include "lisa_device.h"
#include "test_common.h"

/* ========================================
 * 测试用设备定义
 * ======================================== */

LISA_DEVICE_REGISTER(test_foreach_device1, NULL, NULL, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_NORMAL);

LISA_DEVICE_REGISTER(test_foreach_device2, NULL, NULL, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_NORMAL);

LISA_DEVICE_REGISTER(test_foreach_device3, NULL, NULL, NULL, 
                     test_mock_init_success, LISA_DEVICE_PRIORITY_NORMAL);

/* ========================================
 * 测试辅助变量和回调函数
 * ======================================== */

/* 用于统计遍历次数的计数器 */
static int callback_count = 0;

/* 用于存储遍历到的设备名称 */
#define MAX_CALLBACK_DEVICES 20
static const char *callback_device_names[MAX_CALLBACK_DEVICES];

/* 简单计数回调 */
static int count_callback(lisa_device_t *dev, void *user_data)
{
    (void)dev;
    (void)user_data;
    callback_count++;
    return 0;  /* 继续遍历 */
}

/* 记录设备名称的回调 */
static int record_name_callback(lisa_device_t *dev, void *user_data)
{
    (void)user_data;
    if (callback_count < MAX_CALLBACK_DEVICES) {
        callback_device_names[callback_count] = dev->name;
        callback_count++;
    }
    return 0;
}

/* 遇到特定设备就停止的回调 */
static int stop_at_device_callback(lisa_device_t *dev, void *user_data)
{
    const char *stop_name = (const char *)user_data;
    callback_count++;
    
    if (stop_name && dev->name && strcmp(dev->name, stop_name) == 0) {
        return 1;  /* 停止遍历 */
    }
    return 0;  /* 继续遍历 */
}

/* 用户数据传递测试回调 */
static int user_data_callback(lisa_device_t *dev, void *user_data)
{
    (void)dev;
    int *counter = (int *)user_data;
    if (counter) {
        (*counter)++;
    }
    return 0;
}

/* setUp - 在每个测试前执行 */
static void test_foreach_setup(void)
{
    callback_count = 0;
    for (int i = 0; i < MAX_CALLBACK_DEVICES; i++) {
        callback_device_names[i] = NULL;
    }
}

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试遍历所有设备
 */
void test_device_foreach_all(void)
{
    test_foreach_setup();
    
    /* 遍历所有设备 */
    int count = lisa_device_foreach(count_callback, NULL);
    
    /* 验证遍历次数 */
    TEST_ASSERT_GREATER_THAN(0, count);
    TEST_ASSERT_EQUAL(count, callback_count);
}

/**
 * @brief 测试遍历返回正确的设备数量
 */
void test_device_foreach_count(void)
{
    test_foreach_setup();
    
    /* 遍历所有设备 */
    int count = lisa_device_foreach(count_callback, NULL);
    
    /* 验证返回值与实际调用次数一致 */
    TEST_ASSERT_EQUAL(callback_count, count);
    
    /* 验证至少遍历到我们注册的3个测试设备 */
    TEST_ASSERT_GREATER_OR_EQUAL(3, count);
}

/**
 * @brief 测试回调返回非0时停止遍历
 */
void test_device_foreach_early_stop(void)
{
    test_foreach_setup();
    
    /* 在遇到test_foreach_device2时停止 */
    int count = lisa_device_foreach(stop_at_device_callback, 
                                     (void *)"test_foreach_device2");
    
    /* 验证提前停止 */
    TEST_ASSERT_GREATER_THAN(0, count);
    TEST_ASSERT_EQUAL(callback_count, count);
}

/**
 * @brief 测试NULL回调函数
 */
void test_device_foreach_null_callback(void)
{
    /* NULL回调应返回错误 */
    int ret = lisa_device_foreach(NULL, NULL);
    TEST_ASSERT_EQUAL(LISA_DEVICE_ERR_INVALID, ret);
}

/**
 * @brief 测试用户数据传递
 */
void test_device_foreach_user_data(void)
{
    test_foreach_setup();
    
    int user_counter = 0;
    
    /* 使用用户数据 */
    int count = lisa_device_foreach(user_data_callback, &user_counter);
    
    /* 验证用户数据被正确传递和修改 */
    TEST_ASSERT_EQUAL(count, user_counter);
    TEST_ASSERT_GREATER_THAN(0, user_counter);
}

/**
 * @brief 测试遍历能找到特定设备
 */
void test_device_foreach_find_specific(void)
{
    test_foreach_setup();
    
    /* 记录所有设备名称 */
    int count = lisa_device_foreach(record_name_callback, NULL);
    TEST_ASSERT_GREATER_THAN(0, count);
    
    /* 验证能找到我们注册的测试设备 */
    bool found1 = false, found2 = false, found3 = false;
    
    for (int i = 0; i < callback_count; i++) {
        if (callback_device_names[i] != NULL) {
            if (strcmp(callback_device_names[i], "test_foreach_device1") == 0) {
                found1 = true;
            }
            if (strcmp(callback_device_names[i], "test_foreach_device2") == 0) {
                found2 = true;
            }
            if (strcmp(callback_device_names[i], "test_foreach_device3") == 0) {
                found3 = true;
            }
        }
    }
    
    TEST_ASSERT_TRUE(found1);
    TEST_ASSERT_TRUE(found2);
    TEST_ASSERT_TRUE(found3);
}

/**
 * @brief 测试遍历NULL用户数据
 */
void test_device_foreach_null_user_data(void)
{
    test_foreach_setup();
    
    /* NULL用户数据应该正常工作 */
    int count = lisa_device_foreach(count_callback, NULL);
    TEST_ASSERT_GREATER_THAN(0, count);
}

/**
 * @brief 测试多次遍历
 */
void test_device_foreach_multiple_times(void)
{
    test_foreach_setup();
    
    /* 第一次遍历 */
    int count1 = lisa_device_foreach(count_callback, NULL);
    TEST_ASSERT_GREATER_THAN(0, count1);
    
    test_foreach_setup();
    
    /* 第二次遍历 */
    int count2 = lisa_device_foreach(count_callback, NULL);
    TEST_ASSERT_GREATER_THAN(0, count2);
    
    /* 两次遍历应该返回相同数量 */
    TEST_ASSERT_EQUAL(count1, count2);
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_device_foreach_tests(void)
{
    RUN_TEST(test_device_foreach_all);
    RUN_TEST(test_device_foreach_count);
    RUN_TEST(test_device_foreach_early_stop);
    RUN_TEST(test_device_foreach_null_callback);
    RUN_TEST(test_device_foreach_user_data);
    RUN_TEST(test_device_foreach_find_specific);
    RUN_TEST(test_device_foreach_null_user_data);
    RUN_TEST(test_device_foreach_multiple_times);
}
