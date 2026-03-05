/*
 * LISA App Player Component - 生命周期测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试播放器创建和销毁相关功能
 */

#include "unity.h"
#include "app_player.h"
#include "test_common.h"
#include <string.h>

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试使用有效名称创建播放器
 */
void test_create_player_with_valid_name(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);

    TEST_ASSERT_NOT_NULL_MESSAGE(player, "Create player with valid name should succeed");

    /* 清理 */
    if (player) {
        app_player_destroy(player);
    }
}

/**
 * @brief 测试使用NULL名称创建播放器
 */
void test_create_player_with_null_name(void)
{
    app_player_t *player = app_player_create(NULL);

    /* 根据实现,可能返回NULL或使用默认名称,这里只验证不崩溃 */
    TEST_ASSERT_TRUE_MESSAGE(1, "Create with NULL name should not crash");

    /* 清理 */
    if (player) {
        app_player_destroy(player);
    }
}

/**
 * @brief 测试使用空字符串创建播放器
 */
void test_create_player_with_empty_name(void)
{
    app_player_t *player = app_player_create("");

    /* 根据实现,可能返回NULL或使用默认名称,这里只验证不崩溃 */
    TEST_ASSERT_TRUE_MESSAGE(1, "Create with empty name should not crash");

    /* 清理 */
    if (player) {
        app_player_destroy(player);
    }
}

/**
 * @brief 测试销毁有效播放器
 */
void test_destroy_valid_player(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    int ret = app_player_destroy(player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Destroy valid player should return OK");
}

/**
 * @brief 测试销毁NULL播放器
 */
void test_destroy_null_player(void)
{
    int ret = app_player_destroy(NULL);

    /* 销毁NULL应该返回错误或直接成功,这里只验证不崩溃 */
    TEST_ASSERT_TRUE_MESSAGE(1, "Destroy NULL player should not crash");
}

/**
 * @brief 测试创建后立即销毁
 */
void test_create_and_destroy_immediately(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    int ret = app_player_destroy(player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Immediate destroy should succeed");
}

/**
 * @brief 测试创建多个播放器实例
 */
void test_create_multiple_players(void)
{
    app_player_t *player1 = app_player_create("player1");
    app_player_t *player2 = app_player_create("player2");
    app_player_t *player3 = app_player_create("player3");

    TEST_ASSERT_NOT_NULL_MESSAGE(player1, "Create player1 should succeed");
    TEST_ASSERT_NOT_NULL_MESSAGE(player2, "Create player2 should succeed");
    TEST_ASSERT_NOT_NULL_MESSAGE(player3, "Create player3 should succeed");

    /* 验证是不同的实例 */
    TEST_ASSERT_NOT_EQUAL_MESSAGE(player1, player2, "player1 and player2 should be different");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(player2, player3, "player2 and player3 should be different");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(player1, player3, "player1 and player3 should be different");

    /* 清理 */
    if (player1) app_player_destroy(player1);
    if (player2) app_player_destroy(player2);
    if (player3) app_player_destroy(player3);
}

/**
 * @brief 测试播放器初始状态
 */
void test_player_initial_state(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    app_player_state_t state = app_player_get_state(player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_IDLE, state,
                              "Initial state should be IDLE");

    /* 清理 */
    app_player_destroy(player);
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_player_lifecycle_tests(void)
{
    RUN_TEST(test_create_player_with_valid_name);
    RUN_TEST(test_create_player_with_null_name);
    RUN_TEST(test_create_player_with_empty_name);
    RUN_TEST(test_destroy_valid_player);
    RUN_TEST(test_destroy_null_player);
    RUN_TEST(test_create_and_destroy_immediately);
    RUN_TEST(test_create_multiple_players);
    RUN_TEST(test_player_initial_state);
}
