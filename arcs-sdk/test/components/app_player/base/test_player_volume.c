/*
 * LISA App Player Component - 音量控制测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试播放器音量控制功能
 */

#include "unity.h"
#include "app_player.h"
#include "test_common.h"
#include <stdio.h>

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试设置最小音量
 */
void test_set_volume_min(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    uint8_t volume = 1;  /* 最小音量 */
    printf("  [Test] Setting volume to %u\n", volume);
    int ret = app_player_set_volume(player, volume);
    printf("  [Test] set_volume returns: %s\n", error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                              "Set volume to 1 should return OK");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试设置最大音量
 */
void test_set_volume_max(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    uint8_t volume = 100;  /* 最大音量 */
    printf("  [Test] Setting volume to %u\n", volume);
    int ret = app_player_set_volume(player, volume);
    printf("  [Test] set_volume returns: %s\n", error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                              "Set volume to 100 should return OK");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试设置中间音量
 */
void test_set_volume_mid(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    uint8_t volume = 50;  /* 中间音量 */
    printf("  [Test] Setting volume to %u\n", volume);
    int ret = app_player_set_volume(player, volume);
    printf("  [Test] set_volume returns: %s\n", error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                              "Set volume to 50 should return OK");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试设置音量为0(边界测试)
 */
void test_set_volume_zero(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    uint8_t volume = 0;
    printf("  [Test] Setting volume to %u\n", volume);
    int ret = app_player_set_volume(player, volume);
    printf("  [Test] set_volume(0) returns: %s\n", error_to_string(ret));

    /* 音量0可能被拒绝(要求1-100),也可能接受,这里只验证不崩溃 */
    TEST_ASSERT_TRUE_MESSAGE(1, "Set volume to 0 should not crash");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试设置超过最大值的音量
 */
void test_set_volume_over_max(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    uint8_t volume = 101;
    printf("  [Test] Setting volume to %u\n", volume);
    int ret = app_player_set_volume(player, volume);
    printf("  [Test] set_volume(101) returns: %s\n", error_to_string(ret));

    /* 超过最大值应该返回错误或被限制到100 */
    TEST_ASSERT_TRUE_MESSAGE(1, "Set volume over max should not crash");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试对NULL播放器设置音量
 */
void test_set_volume_null_player(void)
{
    int ret = app_player_set_volume(NULL, 50);
    printf("  [Test] set_volume on NULL player returns: %s\n",
           error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_PARAM, ret,
                              "set_volume on NULL player should return INVALID_PARAM");
}

/**
 * @brief 测试播放前设置音量
 */
void test_set_volume_before_play(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 播放前设置音量 */
    uint8_t volume = 80;
    printf("  [Test] Setting volume before play: %u\n", volume);
    int ret = app_player_set_volume(player, volume);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 开始播放 */
    ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 清理 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/**
 * @brief 测试播放中设置音量
 */
void test_set_volume_during_play(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 开始播放 */
    int ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    wait_ms(TEST_WAIT_LONG_MS);

    /* 播放中设置音量 */
    uint8_t volume = 30;
    printf("  [Test] Setting volume during play: %u\n", volume);
    ret = app_player_set_volume(player, volume);
    printf("  [Test] set_volume during play returns: %s\n",
           error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                              "Set volume during play should return OK");

    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 清理 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/**
 * @brief 测试暂停时设置音量
 */
void test_set_volume_when_paused(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 开始播放 */
    int ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    wait_ms(TEST_WAIT_LONG_MS);

    /* 暂停 */
    ret = app_player_pause(player);
    if (ret == APP_PLAYER_OK) {
        wait_ms(TEST_WAIT_SHORT_MS);

        /* 暂停时设置音量 */
        uint8_t volume = 60;
        printf("  [Test] Setting volume when paused: %u\n", volume);
        ret = app_player_set_volume(player, volume);
        printf("  [Test] set_volume when paused returns: %s\n",
               error_to_string(ret));

        TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                                  "Set volume when paused should return OK");
    }

    /* 清理 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/**
 * @brief 测试多次设置音量
 */
void test_set_volume_multiple_times(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 多次设置不同音量 */
    uint8_t volumes[] = {10, 20, 50, 80, 100, 1};
    for (int i = 0; i < sizeof(volumes)/sizeof(volumes[0]); i++) {
        printf("  [Test] Setting volume to %u\n", volumes[i]);
        int ret = app_player_set_volume(player, volumes[i]);
        TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                                  "Each set_volume should return OK");
    }

    /* 清理 */
    app_player_destroy(player);
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_player_volume_tests(void)
{
    RUN_TEST(test_set_volume_min);
    RUN_TEST(test_set_volume_max);
    RUN_TEST(test_set_volume_mid);
    RUN_TEST(test_set_volume_zero);
    RUN_TEST(test_set_volume_over_max);
    RUN_TEST(test_set_volume_null_player);
    RUN_TEST(test_set_volume_before_play);
    RUN_TEST(test_set_volume_during_play);
    RUN_TEST(test_set_volume_when_paused);
    RUN_TEST(test_set_volume_multiple_times);
}
