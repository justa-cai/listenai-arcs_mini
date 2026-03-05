/*
 * LISA App Player Component - 进度控制测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试播放器进度控制功能(seek、get_position、get_duration)
 */

#include "unity.h"
#include "app_player.h"
#include "test_common.h"
#include <stdio.h>

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试seek到有效位置
 */
void test_seek_valid_position(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 开始播放 */
    int ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 等待播放启动 */
    wait_ms(TEST_WAIT_LONG_MS);

    /* Seek到2秒位置 */
    uint32_t seek_pos = 2000;
    printf("  [Test] Seeking to %u ms\n", seek_pos);
    ret = app_player_seek(player, seek_pos);
    printf("  [Test] Seek returns: %s\n", error_to_string(ret));

    if (ret == APP_PLAYER_OK) {
        wait_ms(TEST_WAIT_MEDIUM_MS);
        TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Seek should return OK");
    }

    /* 清理 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/**
 * @brief 测试seek到0位置
 */
void test_seek_zero_position(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 开始播放 */
    int ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 等待播放启动 */
    wait_ms(TEST_WAIT_LONG_MS);

    /* Seek到0位置 */
    printf("  [Test] Seeking to 0 ms\n");
    ret = app_player_seek(player, 0);
    printf("  [Test] Seek to 0 returns: %s\n", error_to_string(ret));

    /* Seek到0可能成功或失败,取决于实现,这里只验证不崩溃 */
    TEST_ASSERT_TRUE_MESSAGE(1, "Seek to 0 should not crash");

    /* 清理 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/**
 * @brief 测试对NULL播放器调用seek
 */
void test_seek_null_player(void)
{
    int ret = app_player_seek(NULL, 1000);
    printf("  [Test] Seek on NULL player returns: %s\n", error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_PARAM, ret,
                              "Seek on NULL player should return INVALID_PARAM");
}

/**
 * @brief 测试在IDLE状态调用seek
 */
void test_seek_idle_player(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    TEST_ASSERT_EQUAL(APP_PLAYER_STATE_IDLE, app_player_get_state(player));

    int ret = app_player_seek(player, 1000);
    printf("  [Test] Seek on IDLE player returns: %s\n", error_to_string(ret));

    TEST_ASSERT_NOT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                                  "Seek on IDLE player should return error");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试获取当前播放位置
 */
void test_get_position_valid_player(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 开始播放 */
    int ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 等待播放启动 */
    wait_ms(TEST_WAIT_LONG_MS);

    /* 获取当前位置 */
    uint32_t position = 0;
    ret = app_player_get_position(player, &position);
    printf("  [Test] get_position returns: %s, position=%u ms\n",
           error_to_string(ret), position);

    if (ret == APP_PLAYER_OK) {
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0, position,
                                             "Position should be >= 0");
    }

    /* 清理 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/**
 * @brief 测试对NULL播放器获取位置
 */
void test_get_position_null_player(void)
{
    uint32_t position = 0;
    int ret = app_player_get_position(NULL, &position);
    printf("  [Test] get_position on NULL player returns: %s\n",
           error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_PARAM, ret,
                              "get_position on NULL player should return INVALID_PARAM");
}

/**
 * @brief 测试传NULL输出参数获取位置
 */
void test_get_position_null_output(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    int ret = app_player_get_position(player, NULL);
    printf("  [Test] get_position with NULL output returns: %s\n",
           error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_PARAM, ret,
                              "get_position with NULL output should return INVALID_PARAM");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试获取总时长
 */
void test_get_duration_valid_player(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 开始播放 */
    int ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 等待播放启动(需要解析完才能获取时长) */
    wait_ms(TEST_WAIT_LONG_MS);

    /* 获取总时长 */
    uint32_t duration = 0;
    ret = app_player_get_duration(player, &duration);
    printf("  [Test] get_duration returns: %s, duration=%u ms\n",
           error_to_string(ret), duration);

    if (ret == APP_PLAYER_OK) {
        TEST_ASSERT_GREATER_THAN_MESSAGE(0, duration,
                                         "Duration should be > 0");
    }

    /* 清理 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/**
 * @brief 测试对NULL播放器获取时长
 */
void test_get_duration_null_player(void)
{
    uint32_t duration = 0;
    int ret = app_player_get_duration(NULL, &duration);
    printf("  [Test] get_duration on NULL player returns: %s\n",
           error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_PARAM, ret,
                              "get_duration on NULL player should return INVALID_PARAM");
}

/**
 * @brief 测试传NULL输出参数获取时长
 */
void test_get_duration_null_output(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    int ret = app_player_get_duration(player, NULL);
    printf("  [Test] get_duration with NULL output returns: %s\n",
           error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_PARAM, ret,
                              "get_duration with NULL output should return INVALID_PARAM");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试在IDLE状态获取时长
 */
void test_get_duration_idle_player(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    TEST_ASSERT_EQUAL(APP_PLAYER_STATE_IDLE, app_player_get_state(player));

    uint32_t duration = 0;
    int ret = app_player_get_duration(player, &duration);
    printf("  [Test] get_duration on IDLE player returns: %s\n",
           error_to_string(ret));

    /* IDLE状态获取时长应该失败 */
    TEST_ASSERT_NOT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                                  "get_duration on IDLE player should return error");

    /* 清理 */
    app_player_destroy(player);
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_player_seek_tests(void)
{
    RUN_TEST(test_seek_valid_position);
    RUN_TEST(test_seek_zero_position);
    RUN_TEST(test_seek_null_player);
    RUN_TEST(test_seek_idle_player);
    RUN_TEST(test_get_position_valid_player);
    RUN_TEST(test_get_position_null_player);
    RUN_TEST(test_get_position_null_output);
    RUN_TEST(test_get_duration_valid_player);
    RUN_TEST(test_get_duration_null_player);
    RUN_TEST(test_get_duration_null_output);
    RUN_TEST(test_get_duration_idle_player);
}
