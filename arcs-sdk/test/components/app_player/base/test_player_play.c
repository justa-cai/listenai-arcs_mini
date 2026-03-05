/*
 * LISA App Player Component - 播放功能测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试播放器基本播放功能
 */

#include "unity.h"
#include "app_player.h"
#include "test_common.h"
#include <stdio.h>

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试播放有效URL
 */
void test_play_valid_url(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    printf("  [Test] Playing URL: %s\n", TEST_URL);
    int ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                              "Play valid URL should return OK");

    /* 等待一段时间让播放启动 */
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 清理 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/**
 * @brief 测试播放NULL URL
 */
void test_play_null_url(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    int ret = app_player_play(player, NULL);
    printf("  [Test] Play NULL URL returns: %s\n", error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_PARAM, ret,
                              "Play NULL URL should return INVALID_PARAM");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试对NULL播放器调用play
 */
void test_play_null_player(void)
{
    int ret = app_player_play(NULL, TEST_URL);
    printf("  [Test] Play on NULL player returns: %s\n", error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_PARAM, ret,
                              "Play on NULL player should return INVALID_PARAM");
}

/**
 * @brief 测试使用play_ex播放
 */
void test_play_ex_with_valid_options(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    app_player_play_opt_t opt = {
        .url = TEST_URL,
        .throw_time_ms = 0
    };

    printf("  [Test] Playing with play_ex: %s\n", TEST_URL);
    int ret = app_player_play_ex(player, &opt);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                              "play_ex with valid options should return OK");

    /* 等待播放启动 */
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 清理 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/**
 * @brief 测试play_ex的throw_time_ms参数
 */
void test_play_ex_with_throw_time(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    app_player_play_opt_t opt = {
        .url = TEST_URL,
        .throw_time_ms = 1000  /* 跳过开始1秒内的低能量段 */
    };

    printf("  [Test] Playing with throw_time_ms=1000\n");
    int ret = app_player_play_ex(player, &opt);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                              "play_ex with throw_time should return OK");

    /* 等待播放启动 */
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 清理 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/**
 * @brief 测试play_ex的throw_time_ms参数（跳过低能量段）
 */
void test_play_ex_with_throw_low_energy(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    app_player_play_opt_t opt = {
        .url = TEST_URL,
        .throw_time_ms = 500  /* 跳过开始500ms内的低能量段 */
    };

    printf("  [Test] Playing with throw_time_ms=500 (skip low energy)\n");
    int ret = app_player_play_ex(player, &opt);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                              "play_ex with throw_time_ms should return OK");

    /* 等待播放启动 */
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 清理 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/**
 * @brief 测试play_ex传NULL选项
 */
void test_play_ex_null_options(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    int ret = app_player_play_ex(player, NULL);
    printf("  [Test] play_ex with NULL options returns: %s\n", error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_PARAM, ret,
                              "play_ex with NULL options should return INVALID_PARAM");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试play_ex传NULL URL
 */
void test_play_ex_null_url(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    app_player_play_opt_t opt = {
        .url = NULL,
        .throw_time_ms = 0
    };

    int ret = app_player_play_ex(player, &opt);
    printf("  [Test] play_ex with NULL URL returns: %s\n", error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_PARAM, ret,
                              "play_ex with NULL URL should return INVALID_PARAM");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试重复调用play
 */
void test_play_twice(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 第一次播放 */
    printf("  [Test] First play\n");
    int ret1 = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret1);

    wait_ms(TEST_WAIT_MEDIUM_MS);
    app_player_stop_sync(player);

    /* 第二次播放 */
    printf("  [Test] Second play\n");
    int ret2 = app_player_play(player, TEST_URL);
    printf("  [Test] Second play returns: %s\n", error_to_string(ret2));

    wait_ms(TEST_WAIT_MEDIUM_MS);
    app_player_stop_sync(player);
    
    /* 清理 */
    app_player_destroy(player);
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_player_play_tests(void)
{
    RUN_TEST(test_play_valid_url);
    RUN_TEST(test_play_null_url);
    RUN_TEST(test_play_null_player);
    RUN_TEST(test_play_ex_with_valid_options);
    RUN_TEST(test_play_ex_with_throw_time);
    RUN_TEST(test_play_ex_with_throw_low_energy);
    RUN_TEST(test_play_ex_null_options);
    RUN_TEST(test_play_ex_null_url);
    RUN_TEST(test_play_twice);
}
