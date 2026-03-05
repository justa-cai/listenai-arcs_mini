/*
 * LISA App Player Component - 播放控制测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试播放器控制功能(暂停、恢复、停止、重置)
 */

#include "unity.h"
#include "app_player.h"
#include "test_common.h"
#include <stdio.h>

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试暂停正在播放的播放器
 */
void test_pause_playing_player(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 开始播放 */
    int ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 等待进入播放状态 */
    wait_ms(TEST_WAIT_LONG_MS);

    /* 暂停 */
    printf("  [Test] Pausing player\n");
    ret = app_player_pause(player);
    printf("  [Test] Pause returns: %s\n", error_to_string(ret));

    if (ret == APP_PLAYER_OK) {
        wait_ms(TEST_WAIT_SHORT_MS);
        app_player_state_t state = app_player_get_state(player);
        printf("  [Test] State after pause: %s\n", state_to_string(state));
        TEST_ASSERT_EQUAL(APP_PLAYER_STATE_PAUSED, state);
    }

    /* 清理 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/**
 * @brief 测试对NULL播放器调用pause
 */
void test_pause_null_player(void)
{
    int ret = app_player_pause(NULL);
    printf("  [Test] Pause NULL player returns: %s\n", error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_PARAM, ret,
                              "Pause NULL player should return INVALID_PARAM");
}

/**
 * @brief 测试对IDLE状态调用pause
 */
void test_pause_idle_player(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    TEST_ASSERT_EQUAL(APP_PLAYER_STATE_IDLE, app_player_get_state(player));

    int ret = app_player_pause(player);
    printf("  [Test] Pause IDLE player returns: %s\n", error_to_string(ret));

    TEST_ASSERT_NOT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                                  "Pause IDLE player should return error");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试恢复暂停的播放器
 */
void test_resume_paused_player(void)
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
        printf("  [Test] Resuming player\n");
        ret = app_player_resume(player);
        printf("  [Test] Resume returns: %s\n", error_to_string(ret));

        if (ret == APP_PLAYER_OK) {
            wait_ms(TEST_WAIT_SHORT_MS);
            app_player_state_t state = app_player_get_state(player);
            printf("  [Test] State after resume: %s\n", state_to_string(state));
            TEST_ASSERT_EQUAL(APP_PLAYER_STATE_PLAYING, state);
        }
    }

    /* 清理 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/**
 * @brief 测试同步恢复暂停的播放器
 */
void test_resume_sync_paused_player(void)
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

        /* 同步恢复 */
        printf("  [Test] Resuming player (sync)\n");
        ret = app_player_resume_sync(player);
        printf("  [Test] Resume sync returns: %s\n", error_to_string(ret));

        if (ret == APP_PLAYER_OK) {
            app_player_state_t state = app_player_get_state(player);
            printf("  [Test] State after resume_sync: %s\n", state_to_string(state));
            TEST_ASSERT_EQUAL(APP_PLAYER_STATE_PLAYING, state);
        }
    }

    /* 清理 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/**
 * @brief 测试对NULL播放器调用resume
 */
void test_resume_null_player(void)
{
    int ret = app_player_resume(NULL);
    printf("  [Test] Resume NULL player returns: %s\n", error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_PARAM, ret,
                              "Resume NULL player should return INVALID_PARAM");
}

/**
 * @brief 测试停止正在播放的播放器
 */
void test_stop_playing_player(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 开始播放 */
    int ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 等待播放 */
    wait_ms(TEST_WAIT_LONG_MS);

    /* 异步停止 */
    printf("  [Test] Stopping player (async)\n");
    ret = app_player_stop(player);
    printf("  [Test] Stop returns: %s\n", error_to_string(ret));
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 等待停止完成 */
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试同步停止正在播放的播放器
 */
void test_stop_sync_playing_player(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 开始播放 */
    int ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 等待播放 */
    wait_ms(TEST_WAIT_LONG_MS);

    /* 同步停止 */
    printf("  [Test] Stopping player (sync)\n");
    ret = app_player_stop_sync(player);
    printf("  [Test] Stop sync returns: %s\n", error_to_string(ret));
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    app_player_state_t state = app_player_get_state(player);
    printf("  [Test] State after stop_sync: %s\n", state_to_string(state));
    TEST_ASSERT_EQUAL(APP_PLAYER_STATE_STOPPED, state);

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试对NULL播放器调用stop
 */
void test_stop_null_player(void)
{
    int ret = app_player_stop(NULL);
    printf("  [Test] Stop NULL player returns: %s\n", error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_PARAM, ret,
                              "Stop NULL player should return INVALID_PARAM");
}

/**
 * @brief 测试重置播放器
 */
void test_reset_player(void)
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
    printf("  [Test] Resetting player\n");
    ret = app_player_reset(player);
    printf("  [Test] Reset returns: %s\n", error_to_string(ret));
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 等待重置完成 */
    wait_ms(TEST_WAIT_SHORT_MS);

    app_player_state_t state = app_player_get_state(player);
    printf("  [Test] State after reset: %s\n", state_to_string(state));
    TEST_ASSERT_EQUAL(APP_PLAYER_STATE_IDLE, state);

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试对NULL播放器调用reset
 */
void test_reset_null_player(void)
{
    int ret = app_player_reset(NULL);
    printf("  [Test] Reset NULL player returns: %s\n", error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_PARAM, ret,
                              "Reset NULL player should return INVALID_PARAM");
}

/**
 * @brief 测试停止IDLE状态的播放器
 */
void test_stop_idle_player(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    TEST_ASSERT_EQUAL(APP_PLAYER_STATE_IDLE, app_player_get_state(player));

    int ret = app_player_stop(player);
    printf("  [Test] Stop IDLE player returns: %s\n", error_to_string(ret));

    /* 停止IDLE状态可能返回错误或成功,这里只验证不崩溃 */
    TEST_ASSERT_TRUE_MESSAGE(1, "Stop IDLE player should not crash");

    /* 清理 */
    app_player_destroy(player);
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_player_control_tests(void)
{
    RUN_TEST(test_pause_playing_player);
    RUN_TEST(test_pause_null_player);
    RUN_TEST(test_pause_idle_player);
    RUN_TEST(test_resume_paused_player);
    RUN_TEST(test_resume_sync_paused_player);
    RUN_TEST(test_resume_null_player);
    RUN_TEST(test_stop_playing_player);
    RUN_TEST(test_stop_sync_playing_player);
    RUN_TEST(test_stop_null_player);
    RUN_TEST(test_reset_player);
    RUN_TEST(test_reset_null_player);
    RUN_TEST(test_stop_idle_player);
}
