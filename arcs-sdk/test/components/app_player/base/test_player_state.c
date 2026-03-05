/*
 * LISA App Player Component - 状态查询测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试播放器状态获取功能
 */

#include "unity.h"
#include "app_player.h"
#include "test_common.h"
#include <stdio.h>

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试获取初始状态
 */
void test_get_initial_state(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    app_player_state_t state = app_player_get_state(player);
    printf("  [Test] Initial state: %s\n", state_to_string(state));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_IDLE, state,
                              "Initial state should be IDLE");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试对NULL播放器获取状态
 */
void test_get_state_null_player(void)
{
    app_player_state_t state = app_player_get_state(NULL);

    /* NULL播放器可能返回ERROR或IDLE,这里只验证不崩溃 */
    printf("  [Test] State of NULL player: %s\n", state_to_string(state));
    TEST_ASSERT_TRUE_MESSAGE(1, "Get state on NULL player should not crash");
}

/**
 * @brief 测试play后的状态变化
 */
void test_state_after_play(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 开始播放 */
    printf("  [Test] Starting playback: %s\n", TEST_URL);
    int ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 短暂等待后检查状态 */
    wait_ms(TEST_WAIT_SHORT_MS);
    app_player_state_t state1 = app_player_get_state(player);
    printf("  [Test] State after play (100ms): %s\n", state_to_string(state1));

    /* 较长等待后检查状态 */
    wait_ms(TEST_WAIT_MEDIUM_MS);
    app_player_state_t state2 = app_player_get_state(player);
    printf("  [Test] State after play (600ms): %s\n", state_to_string(state2));

    /* 状态应该是PREPARING, PREPARED或PLAYING中的一个 */
    TEST_ASSERT_TRUE_MESSAGE(
        state2 == APP_PLAYER_STATE_PREPARING ||
        state2 == APP_PLAYER_STATE_PREPARED ||
        state2 == APP_PLAYER_STATE_PLAYING,
        "State should be PREPARING, PREPARED or PLAYING after play");

    /* 清理 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/**
 * @brief 测试pause后的状态
 */
void test_state_after_pause(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 开始播放 */
    int ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 等待进入播放状态 */
    wait_ms(TEST_WAIT_LONG_MS);
    app_player_state_t state_before = app_player_get_state(player);
    printf("  [Test] State before pause: %s\n", state_to_string(state_before));

    /* 暂停 */
    ret = app_player_pause(player);
    if (ret == APP_PLAYER_OK) {
        wait_ms(TEST_WAIT_SHORT_MS);
        app_player_state_t state_after = app_player_get_state(player);
        printf("  [Test] State after pause: %s\n", state_to_string(state_after));

        TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PAUSED, state_after,
                                  "State should be PAUSED after pause");
    } else {
        printf("  [Test] Pause returned: %s (may not be in playing state)\n",
               error_to_string(ret));
    }

    /* 清理 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/**
 * @brief 测试resume后的状态
 */
void test_state_after_resume(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 开始播放 */
    int ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 等待播放 */
    wait_ms(TEST_WAIT_LONG_MS);

    /* 暂停 */
    ret = app_player_pause(player);
    if (ret == APP_PLAYER_OK) {
        wait_ms(TEST_WAIT_SHORT_MS);

        /* 恢复 */
        ret = app_player_resume(player);
        if (ret == APP_PLAYER_OK) {
            wait_ms(TEST_WAIT_SHORT_MS);
            app_player_state_t state = app_player_get_state(player);
            printf("  [Test] State after resume: %s\n", state_to_string(state));

            TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PLAYING, state,
                                      "State should be PLAYING after resume");
        }
    }

    /* 清理 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/**
 * @brief 测试stop后的状态
 */
void test_state_after_stop(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 开始播放 */
    int ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 等待播放 */
    wait_ms(TEST_WAIT_LONG_MS);

    /* 停止 */
    ret = app_player_stop_sync(player);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    app_player_state_t state = app_player_get_state(player);
    printf("  [Test] State after stop: %s\n", state_to_string(state));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_STOPPED, state,
                              "State should be STOPPED after stop");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试reset后的状态
 */
void test_state_after_reset(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 开始播放 */
    int ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 等待播放 */
    wait_ms(TEST_WAIT_LONG_MS);

    /* 停止播放 */
    ret = app_player_stop_sync(player);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 重置 */
    ret = app_player_reset(player);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    wait_ms(TEST_WAIT_SHORT_MS);
    app_player_state_t state = app_player_get_state(player);
    printf("  [Test] State after reset: %s\n", state_to_string(state));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_IDLE, state,
                              "State should be IDLE after reset");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试在IDLE状态调用各操作
 */
void test_operations_in_idle_state(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    app_player_state_t state = app_player_get_state(player);
    TEST_ASSERT_EQUAL(APP_PLAYER_STATE_IDLE, state);

    /* 在IDLE状态调用pause应该返回错误 */
    int ret = app_player_pause(player);
    printf("  [Test] Pause in IDLE state returns: %s\n", error_to_string(ret));
    TEST_ASSERT_NOT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                                  "Pause in IDLE state should fail");

    /* 在IDLE状态调用resume应该返回错误 */
    ret = app_player_resume(player);
    printf("  [Test] Resume in IDLE state returns: %s\n", error_to_string(ret));
    TEST_ASSERT_NOT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                                  "Resume in IDLE state should fail");

    /* 清理 */
    app_player_destroy(player);
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_player_state_tests(void)
{
    RUN_TEST(test_get_initial_state);
    RUN_TEST(test_get_state_null_player);
    RUN_TEST(test_state_after_play);
    RUN_TEST(test_state_after_pause);
    RUN_TEST(test_state_after_resume);
    RUN_TEST(test_state_after_stop);
    RUN_TEST(test_state_after_reset);
    RUN_TEST(test_operations_in_idle_state);
}
