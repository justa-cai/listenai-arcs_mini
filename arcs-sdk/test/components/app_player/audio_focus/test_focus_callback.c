/*
 * LISA App Player Component - 焦点回调机制测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试焦点变化回调的各种场景
 */

#include "unity.h"
#include "test_common.h"
#include <string.h>

#define TAG "test_focus_callback"
#include "lisa_log.h"

/* ========================================
 * 回调测试辅助变量
 * ======================================== */

typedef struct {
    bool called;
    app_player_t *player;
    app_player_focus_state_t state;
    app_player_t *by_which;
    void *user_data;
    int call_count;
} callback_record_t;

static callback_record_t g_music_cb_record = {0};
static callback_record_t g_tts_cb_record = {0};

/* 用于测试 user_data 传递 */
static int g_test_user_data_value = 12345;

/* ========================================
 * 测试回调函数
 * ======================================== */

/**
 * @brief 返回 false 的回调（使用默认策略）
 */
static bool callback_return_false(app_player_t *player,
                                   app_player_focus_state_t state,
                                   app_player_t *by_which,
                                   void *user_data)
{
    LOGI("[CB-FALSE] Focus changed to %s (by %p), user_data=%p",
         focus_state_to_string(state), by_which, user_data);

    g_music_cb_record.called = true;
    g_music_cb_record.player = player;
    g_music_cb_record.state = state;
    g_music_cb_record.by_which = by_which;
    g_music_cb_record.user_data = user_data;
    g_music_cb_record.call_count++;

    return false;  // 执行默认策略
}

/**
 * @brief 返回 true 的回调（完全接管处理）
 */
static bool callback_return_true(app_player_t *player,
                                  app_player_focus_state_t state,
                                  app_player_t *by_which,
                                  void *user_data)
{
    LOGI("[CB-TRUE] Focus changed to %s (by %p), TAKING OVER handling",
         focus_state_to_string(state), by_which);

    g_music_cb_record.called = true;
    g_music_cb_record.player = player;
    g_music_cb_record.state = state;
    g_music_cb_record.by_which = by_which;
    g_music_cb_record.user_data = user_data;
    g_music_cb_record.call_count++;

    if (state == APP_PLAYER_FOCUS_BACKGROUND) {
        // 自定义处理：降低音量而不是暂停
        LOGI("[CB-TRUE] Custom action: reducing volume to 30%%");
        app_player_set_volume(player, 30);
        return true;  // 接管处理，阻止默认的 PAUSE
    }

    return false;  // 其他状态使用默认策略
}

/**
 * @brief 条件性接管的回调
 */
static bool callback_conditional(app_player_t *player,
                                  app_player_focus_state_t state,
                                  app_player_t *by_which,
                                  void *user_data)
{
    LOGI("[CB-COND] Focus changed to %s (by %p)",
         focus_state_to_string(state), by_which);

    g_music_cb_record.called = true;
    g_music_cb_record.call_count++;

    // 只接管 FOREGROUND 状态，其他使用默认策略
    if (state == APP_PLAYER_FOCUS_FOREGROUND) {
        LOGI("[CB-COND] Taking over FOREGROUND handling");
        return true;
    }

    return false;
}

/**
 * @brief TTS 播放器的回调
 */
static bool tts_callback(app_player_t *player,
                          app_player_focus_state_t state,
                          app_player_t *by_which,
                          void *user_data)
{
    LOGI("[TTS-CB] Focus changed to %s", focus_state_to_string(state));

    g_tts_cb_record.called = true;
    g_tts_cb_record.state = state;
    g_tts_cb_record.call_count++;

    return false;
}

/* ========================================
 * 辅助函数
 * ======================================== */

static void clear_callback_records(void)
{
    memset(&g_music_cb_record, 0, sizeof(g_music_cb_record));
    memset(&g_tts_cb_record, 0, sizeof(g_tts_cb_record));
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
 * @brief 测试1：焦点回调返回 false - 使用默认策略
 *
 * 测试场景：
 * 1. MUSIC 注册返回 false 的回调
 * 2. MUSIC 播放中
 * 3. TTS 抢占焦点
 * 4. 验证回调被调用
 * 5. 验证 MUSIC 执行默认的 PAUSE 策略
 */
void test_callback_return_false_uses_default_policy(void)
{
    LOGI("=== Test: Callback return false uses default policy ===");

    clear_callback_records();

    /* 注册返回 false 的回调 */
    int ret = app_player_register_focus_cb(g_music_player, callback_return_false, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Register callback should succeed");

    /* 步骤1: MUSIC 开始播放 */
    LOGI("Step 1: Start playing MUSIC");
    ret = app_player_play(g_music_player, TEST_MUSIC_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Play should succeed");

    bool playing = wait_for_player_state(g_music_player, APP_PLAYER_STATE_PLAYING, 5000);
    TEST_ASSERT_TRUE_MESSAGE(playing, "MUSIC should enter PLAYING state");

    /* 步骤2: TTS 抢占焦点 */
    LOGI("Step 2: TTS preempts MUSIC");
    clear_callback_records();
    ret = app_player_play(g_tts_player, TEST_TTS_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "TTS play should succeed");

    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 验证回调被调用 */
    TEST_ASSERT_TRUE_MESSAGE(g_music_cb_record.called,
                             "Focus callback should be called");
    TEST_ASSERT_EQUAL_MESSAGE(g_music_player, g_music_cb_record.player,
                              "Callback should receive correct player");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_FOCUS_BACKGROUND, g_music_cb_record.state,
                              "Callback should receive BACKGROUND state");
    TEST_ASSERT_EQUAL_MESSAGE(g_tts_player, g_music_cb_record.by_which,
                              "Callback should know it was preempted by TTS");

    /* 验证执行了默认的 PAUSE 策略 */
    app_player_state_t music_state = app_player_get_state(g_music_player);
    LOGI("MUSIC state after callback: %s", state_to_string(music_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PAUSED, music_state,
                              "MUSIC should be PAUSED (default policy executed)");

    /* 清理 */
    LOGI("Cleanup");
    app_player_stop_sync(g_tts_player);
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试2：焦点回调返回 true - 完全接管处理
 *
 * 测试场景：
 * 1. MUSIC 注册返回 true 的回调
 * 2. MUSIC 播放中
 * 3. TTS 抢占焦点
 * 4. 验证回调被调用并返回 true
 * 5. 验证 MUSIC 不执行默认的 PAUSE 策略，仍在播放
 */
void test_callback_return_true_overrides_default(void)
{
    LOGI("=== Test: Callback return true overrides default policy ===");

    clear_callback_records();

    /* 注册返回 true 的回调 */
    int ret = app_player_register_focus_cb(g_music_player, callback_return_true, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Register callback should succeed");

    /* 步骤1: MUSIC 开始播放 */
    LOGI("Step 1: Start playing MUSIC");
    ret = app_player_play(g_music_player, TEST_MUSIC_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Play should succeed");

    bool playing = wait_for_player_state(g_music_player, APP_PLAYER_STATE_PLAYING, 5000);
    TEST_ASSERT_TRUE_MESSAGE(playing, "MUSIC should enter PLAYING state");

    /* 步骤2: TTS 抢占焦点 */
    LOGI("Step 2: TTS preempts MUSIC");
    clear_callback_records();
    ret = app_player_play(g_tts_player, TEST_TTS_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "TTS play should succeed");

    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 验证回调被调用 */
    TEST_ASSERT_TRUE_MESSAGE(g_music_cb_record.called,
                             "Focus callback should be called");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_FOCUS_BACKGROUND, g_music_cb_record.state,
                              "Callback should receive BACKGROUND state");

    /* 验证 MUSIC 没有被暂停（回调返回 true 接管了处理） */
    app_player_state_t music_state = app_player_get_state(g_music_player);
    LOGI("MUSIC state after callback (return true): %s", state_to_string(music_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PLAYING, music_state,
                              "MUSIC should still be PLAYING (callback took over)");

    /* 清理 */
    LOGI("Cleanup");
    app_player_stop_sync(g_tts_player);
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试3：user_data 正确传递
 *
 * 测试场景：
 * 1. MUSIC 注册回调时传递 user_data
 * 2. 触发焦点变化
 * 3. 验证回调收到正确的 user_data
 */
void test_callback_user_data_passed_correctly(void)
{
    LOGI("=== Test: Callback user_data passed correctly ===");

    clear_callback_records();

    /* 注册回调并传递 user_data */
    int ret = app_player_register_focus_cb(g_music_player,
                                            callback_return_false,
                                            &g_test_user_data_value);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Register callback should succeed");

    /* 步骤1: MUSIC 播放 */
    LOGI("Step 1: Start playing MUSIC");
    ret = app_player_play(g_music_player, TEST_MUSIC_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Play should succeed");
    wait_for_player_state(g_music_player, APP_PLAYER_STATE_PLAYING, 5000);

    /* 步骤2: TTS 抢占 */
    LOGI("Step 2: TTS preempts MUSIC");
    clear_callback_records();
    ret = app_player_play(g_tts_player, TEST_TTS_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "TTS play should succeed");
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 验证 user_data 正确传递 */
    TEST_ASSERT_TRUE_MESSAGE(g_music_cb_record.called,
                             "Callback should be called");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&g_test_user_data_value, g_music_cb_record.user_data,
                                   "Callback should receive correct user_data pointer");

    int *received_value = (int *)g_music_cb_record.user_data;
    TEST_ASSERT_EQUAL_INT_MESSAGE(g_test_user_data_value, *received_value,
                                   "user_data value should match");

    /* 清理 */
    LOGI("Cleanup");
    app_player_stop_sync(g_tts_player);
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试4：动态更换焦点回调
 *
 * 测试场景：
 * 1. MUSIC 先注册回调 A
 * 2. 触发焦点变化，验证回调 A 被调用
 * 3. 更换为回调 B
 * 4. 再次触发焦点变化，验证回调 B 被调用
 */
void test_callback_can_be_changed_dynamically(void)
{
    LOGI("=== Test: Callback can be changed dynamically ===");

    clear_callback_records();

    /* 步骤1: 注册第一个回调（return false） */
    LOGI("Step 1: Register first callback (return false)");
    int ret = app_player_register_focus_cb(g_music_player, callback_return_false, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Register first callback should succeed");

    /* 步骤2: MUSIC 播放 */
    LOGI("Step 2: Start playing MUSIC");
    ret = app_player_play(g_music_player, TEST_MUSIC_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Play should succeed");
    wait_for_player_state(g_music_player, APP_PLAYER_STATE_PLAYING, 5000);

    /* 步骤3: TTS 抢占，验证第一个回调被调用 */
    LOGI("Step 3: TTS preempts, verify first callback called");
    clear_callback_records();
    ret = app_player_play(g_tts_player, TEST_TTS_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "TTS play should succeed");
    wait_ms(TEST_WAIT_MEDIUM_MS);

    TEST_ASSERT_TRUE_MESSAGE(g_music_cb_record.called,
                             "First callback should be called");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, g_music_cb_record.call_count,
                                   "First callback should be called once");

    /* 验证默认策略执行（MUSIC被暂停） */
    app_player_state_t music_state = app_player_get_state(g_music_player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PAUSED, music_state,
                              "MUSIC should be paused (first callback returned false)");

    /* 步骤4: 停止TTS，MUSIC 恢复 */
    LOGI("Step 4: Stop TTS, MUSIC resumes");
    app_player_stop_sync(g_tts_player);
    wait_ms(TEST_WAIT_MEDIUM_MS);
    wait_for_player_state(g_music_player, APP_PLAYER_STATE_PLAYING, 3000);

    /* 步骤5: 更换为第二个回调（return true） */
    LOGI("Step 5: Change to second callback (return true)");
    clear_callback_records();
    ret = app_player_register_focus_cb(g_music_player, callback_return_true, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Register second callback should succeed");

    /* 步骤6: TTS 再次抢占，验证第二个回调被调用 */
    LOGI("Step 6: TTS preempts again, verify second callback called");
    ret = app_player_play(g_tts_player, TEST_TTS_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "TTS play should succeed");
    wait_ms(TEST_WAIT_MEDIUM_MS);

    TEST_ASSERT_TRUE_MESSAGE(g_music_cb_record.called,
                             "Second callback should be called");

    /* 验证 MUSIC 没有被暂停（第二个回调返回true） */
    music_state = app_player_get_state(g_music_player);
    LOGI("MUSIC state after second callback: %s", state_to_string(music_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PLAYING, music_state,
                              "MUSIC should still be playing (second callback returned true)");

    /* 清理 */
    LOGI("Cleanup");
    app_player_stop_sync(g_tts_player);
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试5：条件性接管回调
 *
 * 测试场景：
 * 1. 注册条件性接管的回调（只接管 FOREGROUND，其他使用默认策略）
 * 2. 验证不同焦点状态的处理逻辑
 */
void test_callback_conditional_override(void)
{
    LOGI("=== Test: Callback conditional override ===");

    clear_callback_records();

    /* 注册条件性回调 */
    int ret = app_player_register_focus_cb(g_music_player, callback_conditional, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Register callback should succeed");

    /* 步骤1: MUSIC 播放 */
    LOGI("Step 1: Start playing MUSIC");
    ret = app_player_play(g_music_player, TEST_MUSIC_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Play should succeed");
    wait_for_player_state(g_music_player, APP_PLAYER_STATE_PLAYING, 5000);

    /* 步骤2: TTS 抢占（MUSIC 变为 BACKGROUND） */
    LOGI("Step 2: TTS preempts (MUSIC -> BACKGROUND)");
    clear_callback_records();
    ret = app_player_play(g_tts_player, TEST_TTS_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "TTS play should succeed");
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 验证回调被调用，但没有接管（返回false） */
    TEST_ASSERT_TRUE_MESSAGE(g_music_cb_record.called,
                             "Callback should be called for BACKGROUND");

    /* 验证默认策略执行（MUSIC 被暂停） */
    app_player_state_t music_state = app_player_get_state(g_music_player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PAUSED, music_state,
                              "MUSIC should be paused (callback didn't override BACKGROUND)");

    /* 步骤3: TTS 停止（MUSIC 恢复到 FOREGROUND） */
    LOGI("Step 3: Stop TTS (MUSIC -> FOREGROUND)");
    clear_callback_records();
    app_player_stop_sync(g_tts_player);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 验证回调被调用并接管了 FOREGROUND 处理 */
    TEST_ASSERT_TRUE_MESSAGE(g_music_cb_record.called,
                             "Callback should be called for FOREGROUND");

    /* 由于回调接管了 FOREGROUND，可能不会自动恢复播放 */
    /* 这取决于回调的具体实现逻辑 */

    /* 清理 */
    LOGI("Cleanup");
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试6：多个播放器都有焦点回调
 *
 * 测试场景：
 * 1. MUSIC 和 TTS 都注册焦点回调
 * 2. TTS 抢占 MUSIC
 * 3. 验证两个回调都被正确调用
 */
void test_multiple_players_with_callbacks(void)
{
    LOGI("=== Test: Multiple players with callbacks ===");

    clear_callback_records();

    /* 注册两个播放器的回调 */
    int ret = app_player_register_focus_cb(g_music_player, callback_return_false, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Register MUSIC callback should succeed");

    ret = app_player_register_focus_cb(g_tts_player, tts_callback, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Register TTS callback should succeed");

    /* 步骤1: MUSIC 播放 */
    LOGI("Step 1: Start playing MUSIC");
    ret = app_player_play(g_music_player, TEST_MUSIC_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Play should succeed");
    wait_for_player_state(g_music_player, APP_PLAYER_STATE_PLAYING, 5000);

    /* 步骤2: TTS 抢占 */
    LOGI("Step 2: TTS preempts MUSIC");
    clear_callback_records();
    ret = app_player_play(g_tts_player, TEST_TTS_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "TTS play should succeed");
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 验证两个回调都被调用 */
    TEST_ASSERT_TRUE_MESSAGE(g_music_cb_record.called,
                             "MUSIC callback should be called");
    TEST_ASSERT_TRUE_MESSAGE(g_tts_cb_record.called,
                             "TTS callback should be called");

    /* 验证焦点状态 */
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_FOCUS_BACKGROUND, g_music_cb_record.state,
                              "MUSIC should receive BACKGROUND");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_FOCUS_FOREGROUND, g_tts_cb_record.state,
                              "TTS should receive FOREGROUND");

    /* 清理 */
    LOGI("Cleanup");
    app_player_stop_sync(g_tts_player);
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_focus_callback_tests(void)
{
    LOGI("========================================");
    LOGI("  Running Focus Callback Tests");
    LOGI("========================================\n");

    RUN_TEST(test_callback_return_false_uses_default_policy);
    RUN_TEST(test_callback_return_true_overrides_default);
    RUN_TEST(test_callback_user_data_passed_correctly);
    RUN_TEST(test_callback_can_be_changed_dynamically);
    RUN_TEST(test_callback_conditional_override);
    RUN_TEST(test_multiple_players_with_callbacks);

    LOGI("\n========================================");
    LOGI("  Focus Callback Tests Completed");
    LOGI("========================================\n");
}
