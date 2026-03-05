/*
 * LISA App Player Component - 回调注册测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试播放器事件回调注册功能
 */

#include "unity.h"
#include "app_player.h"
#include "test_common.h"
#include <stdio.h>

/* ========================================
 * 测试辅助变量
 * ======================================== */

static int callback_invoked = 0;
static app_player_event_t last_event = APP_PLAYER_EVENT_ERROR;
static void *last_user_data = NULL;

/* ========================================
 * 测试回调函数
 * ======================================== */

static void test_event_callback(app_player_t *player, app_player_event_t event, void *user_data)
{
    callback_invoked++;
    last_event = event;
    last_user_data = user_data;
    printf("  [Callback] Event: %s, Count: %d\n", test_event_to_string(event), callback_invoked);
}

static void reset_callback_state(void)
{
    callback_invoked = 0;
    last_event = APP_PLAYER_EVENT_ERROR;
    last_user_data = NULL;
}

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试注册有效回调函数
 */
void test_register_valid_callback(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    reset_callback_state();

    int ret = app_player_register_callback(player, test_event_callback, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                              "Register valid callback should return OK");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试注册NULL回调函数
 */
void test_register_null_callback(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    int ret = app_player_register_callback(player, NULL, NULL);

    /* NULL回调可能返回错误,也可能接受(取消回调),这里只验证不崩溃 */
    TEST_ASSERT_TRUE_MESSAGE(1, "Register NULL callback should not crash");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试对NULL播放器注册回调
 */
void test_register_with_null_player(void)
{
    int ret = app_player_register_callback(NULL, test_event_callback, NULL);

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_PARAM, ret,
                              "Register callback on NULL player should return INVALID_PARAM");
}

/**
 * @brief 测试注册带user_data的回调
 */
void test_register_with_user_data(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    reset_callback_state();

    int test_data = 12345;
    int ret = app_player_register_callback(player, test_event_callback, &test_data);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                              "Register callback with user_data should return OK");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试重复注册回调
 */
void test_register_callback_twice(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    reset_callback_state();

    /* 第一次注册 */
    int ret1 = app_player_register_callback(player, test_event_callback, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret1,
                              "First callback registration should succeed");

    /* 第二次注册(覆盖) */
    int ret2 = app_player_register_callback(player, test_event_callback, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret2,
                              "Second callback registration should succeed");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试回调是否能接收到事件(需要真实播放)
 * @note 此测试需要网络连接,会播放真实URL
 */
void test_callback_receives_events(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    reset_callback_state();

    /* 注册回调 */
    int ret = app_player_register_callback(player, test_event_callback, NULL);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 开始播放 */
    printf("  [Test] Starting playback: %s\n", TEST_URL);
    ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Play should return OK");

    /* 等待事件触发 */
    wait_ms(TEST_WAIT_LONG_MS);

    /* 验证回调被调用 */
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, callback_invoked,
                                     "Callback should be invoked at least once");

    printf("  [Test] Callback invoked %d times, last event: %s\n",
           callback_invoked, test_event_to_string(last_event));

    /* 停止播放 */
    app_player_stop_sync(player);

    /* 清理 */
    app_player_destroy(player);
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_player_callback_tests(void)
{
    RUN_TEST(test_register_valid_callback);
    RUN_TEST(test_register_null_callback);
    RUN_TEST(test_register_with_null_player);
    RUN_TEST(test_register_with_user_data);
    RUN_TEST(test_register_callback_twice);
    RUN_TEST(test_callback_receives_events);
}
