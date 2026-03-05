/*
 * LISA App Player Component - 焦点行为策略测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试不同的 app_player_focus_behavior_t 配置组合
 * 使用 app_player_set/get_focus_behavior() 动态修改策略
 */

#include "unity.h"
#include "test_common.h"
#include <string.h>

#define TAG "test_focus_behavior"
#include "lisa_log.h"

/* ========================================
 * 焦点事件记录
 * ======================================== */

typedef struct {
    app_player_t *player;
    app_player_focus_state_t state;
    app_player_t *by_which;
    bool received;
} focus_event_t;

static focus_event_t g_music_focus_event = {0};

/* ========================================
 * 焦点回调函数
 * ======================================== */

static bool music_focus_callback(app_player_t *player,
                                   app_player_focus_state_t state,
                                   app_player_t *by_which,
                                   void *user_data)
{
    LOGI("[MUSIC] Focus changed to %s (by %p)", focus_state_to_string(state), by_which);

    g_music_focus_event.player = player;
    g_music_focus_event.state = state;
    g_music_focus_event.by_which = by_which;
    g_music_focus_event.received = true;

    return false;  // 使用默认策略
}

/* ========================================
 * 辅助函数
 * ======================================== */

static void clear_focus_events(void)
{
    memset(&g_music_focus_event, 0, sizeof(g_music_focus_event));
}

static bool wait_for_player_state(app_player_t *player,
                                   app_player_state_t expected_state,
                                   uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    const uint32_t poll_interval = 50;

    while (elapsed < timeout_ms) {
        app_player_state_t current_state = app_player_get_state(player);
        if (current_state == expected_state) {
            return true;
        }

        if (current_state == APP_PLAYER_STATE_ERROR) {
            LOGE("Player entered ERROR state");
            return false;
        }

        wait_ms(poll_interval);
        elapsed += poll_interval;
    }

    return false;
}

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试1：IGNORE + IGNORE - 完全忽略焦点变化
 *
 * 测试场景：
 * 1. 保存 MUSIC 原始配置
 * 2. 修改 MUSIC 为 IGNORE + IGNORE
 * 3. MUSIC 播放中
 * 4. TTS 抢占焦点
 * 5. 验证 MUSIC 仍然在播放（IGNORE 策略）
 * 6. 恢复原始配置
 */
void test_behavior_ignore_ignore(void)
{
    LOGI("=== Test: Behavior IGNORE + IGNORE ===");

    // 保存原始配置
    app_player_focus_behavior_t original_behavior;
    int ret = app_player_get_focus_behavior(g_music_player, &original_behavior);
    TEST_ASSERT_EQUAL_MESSAGE(0, ret, "Get original behavior should succeed");
    LOGI("Original behavior: on_background=%d, on_focus_lost=%d",
         original_behavior.on_background, original_behavior.on_focus_lost);

    // 设置 IGNORE + IGNORE 策略
    app_player_focus_behavior_t test_behavior = {
        .on_background = APP_PLAYER_FOCUS_LOSS_IGNORE,
        .on_focus_lost = APP_PLAYER_FOCUS_LOSS_IGNORE,
    };
    ret = app_player_set_focus_behavior(g_music_player, &test_behavior);
    TEST_ASSERT_EQUAL_MESSAGE(0, ret, "Set behavior should succeed");

    clear_focus_events();

    /* 步骤1: MUSIC 开始播放 */
    LOGI("Step 1: Start playing MUSIC");
    ret = app_player_play(g_music_player, TEST_MUSIC_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "MUSIC play should succeed");

    bool playing = wait_for_player_state(g_music_player, APP_PLAYER_STATE_PLAYING, 5000);
    TEST_ASSERT_TRUE_MESSAGE(playing, "MUSIC should enter PLAYING state");

    /* 步骤2: TTS 抢占焦点 */
    LOGI("Step 2: TTS preempts MUSIC");
    clear_focus_events();
    ret = app_player_play(g_tts_player, TEST_TTS_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "TTS play should succeed");

    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 验证 MUSIC 收到焦点变化事件 */
    TEST_ASSERT_TRUE_MESSAGE(g_music_focus_event.received,
                             "MUSIC should receive focus change event");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_FOCUS_BACKGROUND, g_music_focus_event.state,
                              "MUSIC should move to BACKGROUND");

    /* 验证 MUSIC 仍然在播放（IGNORE 策略） */
    app_player_state_t music_state = app_player_get_state(g_music_player);
    LOGI("MUSIC state after preemption: %s", state_to_string(music_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PLAYING, music_state,
                              "MUSIC should still be PLAYING (IGNORE policy)");

    /* 清理：恢复原始配置 */
    LOGI("Cleanup: Restoring original behavior");
    app_player_stop_sync(g_tts_player);
    app_player_stop_sync(g_music_player);
    app_player_set_focus_behavior(g_music_player, &original_behavior);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试2：验证默认 PAUSE 行为
 *
 * 测试场景：
 * 1. 确认 MUSIC 当前配置为 on_background=PAUSE
 * 2. MUSIC 播放中
 * 3. TTS 抢占
 * 4. 验证 MUSIC 被暂停
 * 5. TTS 停止
 * 6. 验证 MUSIC 自动恢复
 */
void test_behavior_default_pause(void)
{
    LOGI("=== Test: Default PAUSE behavior ===");

    // 确认当前配置
    app_player_focus_behavior_t current_behavior;
    int ret = app_player_get_focus_behavior(g_music_player, &current_behavior);
    TEST_ASSERT_EQUAL_MESSAGE(0, ret, "Get behavior should succeed");

    LOGI("Current behavior: on_background=%d, on_focus_lost=%d",
         current_behavior.on_background, current_behavior.on_focus_lost);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_FOCUS_LOSS_PAUSE, current_behavior.on_background,
                              "Default on_background should be PAUSE");

    clear_focus_events();

    /* 步骤1: MUSIC 播放 */
    LOGI("Step 1: Start playing MUSIC");
    ret = app_player_play(g_music_player, TEST_MUSIC_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "MUSIC play should succeed");
    wait_for_player_state(g_music_player, APP_PLAYER_STATE_PLAYING, 5000);

    /* 步骤2: TTS 抢占 */
    LOGI("Step 2: TTS preempts MUSIC");
    clear_focus_events();
    ret = app_player_play(g_tts_player, TEST_TTS_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "TTS play should succeed");
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 验证 MUSIC 被暂停 */
    app_player_state_t music_state = app_player_get_state(g_music_player);
    LOGI("MUSIC state after TTS preemption: %s", state_to_string(music_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PAUSED, music_state,
                              "MUSIC should be PAUSED (on_background=PAUSE)");

    /* 步骤3: TTS 停止，MUSIC 应自动恢复 */
    LOGI("Step 3: Stop TTS, MUSIC should auto-resume");
    app_player_stop_sync(g_tts_player);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    music_state = app_player_get_state(g_music_player);
    LOGI("MUSIC state after TTS stopped: %s", state_to_string(music_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PLAYING, music_state,
                              "MUSIC should auto-resume to PLAYING");

    /* 清理 */
    LOGI("Cleanup");
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试3：修改为 STOP 策略验证不可恢复
 *
 * 测试场景：
 * 1. 修改 MUSIC 为 on_background=STOP
 * 2. MUSIC 播放中
 * 3. TTS 抢占
 * 4. 验证 MUSIC 停止（不是暂停）
 * 5. TTS 停止
 * 6. 验证 MUSIC 不会自动恢复（STOP 不可恢复）
 * 7. 恢复原始配置
 */
void test_behavior_stop_no_resume(void)
{
    LOGI("=== Test: STOP behavior - no auto-resume ===");

    // 保存原始配置
    app_player_focus_behavior_t original_behavior;
    int ret = app_player_get_focus_behavior(g_music_player, &original_behavior);
    TEST_ASSERT_EQUAL_MESSAGE(0, ret, "Get original behavior should succeed");

    // 修改为 STOP 策略
    app_player_focus_behavior_t test_behavior = {
        .on_background = APP_PLAYER_FOCUS_LOSS_STOP,
        .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
    };
    ret = app_player_set_focus_behavior(g_music_player, &test_behavior);
    TEST_ASSERT_EQUAL_MESSAGE(0, ret, "Set behavior should succeed");

    clear_focus_events();

    /* 步骤1: MUSIC 播放 */
    LOGI("Step 1: Start playing MUSIC");
    ret = app_player_play(g_music_player, TEST_MUSIC_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "MUSIC play should succeed");
    wait_for_player_state(g_music_player, APP_PLAYER_STATE_PLAYING, 5000);

    /* 步骤2: TTS 抢占 */
    LOGI("Step 2: TTS preempts MUSIC");
    clear_focus_events();
    ret = app_player_play(g_tts_player, TEST_TTS_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "TTS play should succeed");
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 验证 MUSIC 停止（不是暂停） */
    app_player_state_t music_state = app_player_get_state(g_music_player);
    LOGI("MUSIC state after TTS preemption: %s", state_to_string(music_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_STOPPED, music_state,
                              "MUSIC should be STOPPED (on_background=STOP)");

    /* 步骤3: TTS 停止 */
    LOGI("Step 3: Stop TTS");
    app_player_stop_sync(g_tts_player);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 验证 MUSIC 不会自动恢复 */
    music_state = app_player_get_state(g_music_player);
    LOGI("MUSIC state after TTS stopped: %s", state_to_string(music_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_STOPPED, music_state,
                              "MUSIC should remain STOPPED (STOP cannot auto-resume)");

    /* 恢复原始配置 */
    LOGI("Cleanup: Restoring original behavior");
    app_player_set_focus_behavior(g_music_player, &original_behavior);

    LOGI("=== Test completed ===\n");
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_focus_behavior_tests(void)
{
    LOGI("========================================");
    LOGI("  Running Focus Behavior Tests");
    LOGI("========================================\n");

    // 注册焦点回调
    app_player_register_focus_cb(g_music_player, music_focus_callback, NULL);

    RUN_TEST(test_behavior_ignore_ignore);
    RUN_TEST(test_behavior_default_pause);
    RUN_TEST(test_behavior_stop_no_resume);

    LOGI("\n========================================");
    LOGI("  Focus Behavior Tests Completed");
    LOGI("========================================\n");
}
