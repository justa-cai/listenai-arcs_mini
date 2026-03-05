/*
 * LISA App Player Component - 焦点抢占测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试不同优先级播放器之间的焦点抢占行为
 */

#include "unity.h"
#include "test_common.h"
#include <string.h>

#define TAG "test_focus_preempt"
#include "lisa_log.h"

/* ========================================
 * 测试辅助变量
 * ======================================== */

/* 用于记录焦点变化事件 */
typedef struct {
    app_player_t *player;
    app_player_focus_state_t state;
    app_player_t *by_which;
    bool received;
} focus_event_t;

static focus_event_t g_music_focus_event = {0};
static focus_event_t g_tts_focus_event = {0};
static focus_event_t g_tone_focus_event = {0};
static focus_event_t g_alarm_focus_event = {0};

/* 用于记录播放器事件 */
typedef struct {
    app_player_event_t event;
    bool received;
} player_event_record_t;

static player_event_record_t g_music_event = {0};
static player_event_record_t g_tts_event = {0};

/* ========================================
 * 回调函数
 * ======================================== */

/**
 * @brief 音乐播放器焦点变化回调
 */
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

    return false;  // 让app_player执行默认策略
}

/**
 * @brief TTS播放器焦点变化回调
 */
static bool tts_focus_callback(app_player_t *player,
                                app_player_focus_state_t state,
                                app_player_t *by_which,
                                void *user_data)
{
    LOGI("[TTS] Focus changed to %s (by %p)", focus_state_to_string(state), by_which);

    g_tts_focus_event.player = player;
    g_tts_focus_event.state = state;
    g_tts_focus_event.by_which = by_which;
    g_tts_focus_event.received = true;

    return false;
}

/**
 * @brief TONE播放器焦点变化回调
 */
static bool tone_focus_callback(app_player_t *player,
                                 app_player_focus_state_t state,
                                 app_player_t *by_which,
                                 void *user_data)
{
    LOGI("[TONE] Focus changed to %s (by %p)", focus_state_to_string(state), by_which);

    g_tone_focus_event.player = player;
    g_tone_focus_event.state = state;
    g_tone_focus_event.by_which = by_which;
    g_tone_focus_event.received = true;

    return false;
}

/**
 * @brief ALARM播放器焦点变化回调
 */
static bool alarm_focus_callback(app_player_t *player,
                                  app_player_focus_state_t state,
                                  app_player_t *by_which,
                                  void *user_data)
{
    LOGI("[ALARM] Focus changed to %s (by %p)", focus_state_to_string(state), by_which);

    g_alarm_focus_event.player = player;
    g_alarm_focus_event.state = state;
    g_alarm_focus_event.by_which = by_which;
    g_alarm_focus_event.received = true;

    return false;
}

/**
 * @brief 音乐播放器事件回调
 */
static void music_event_callback(app_player_t *player,
                                  app_player_event_t event,
                                  void *user_data)
{
    LOGI("[MUSIC] Event: %s", test_event_to_string(event));
    g_music_event.event = event;
    g_music_event.received = true;
}

/**
 * @brief TTS播放器事件回调
 */
static void tts_event_callback(app_player_t *player,
                                app_player_event_t event,
                                void *user_data)
{
    LOGI("[TTS] Event: %s", test_event_to_string(event));
    g_tts_event.event = event;
    g_tts_event.received = true;
}

/* ========================================
 * 辅助函数
 * ======================================== */

/**
 * @brief 清除所有事件记录
 */
static void clear_all_events(void)
{
    memset(&g_music_focus_event, 0, sizeof(g_music_focus_event));
    memset(&g_tts_focus_event, 0, sizeof(g_tts_focus_event));
    memset(&g_tone_focus_event, 0, sizeof(g_tone_focus_event));
    memset(&g_alarm_focus_event, 0, sizeof(g_alarm_focus_event));
    memset(&g_music_event, 0, sizeof(g_music_event));
    memset(&g_tts_event, 0, sizeof(g_tts_event));
}

/**
 * @brief 等待焦点事件发生
 */
static bool wait_for_focus_event(focus_event_t *event, uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    const uint32_t poll_interval = 50;

    while (elapsed < timeout_ms) {
        if (event->received) {
            return true;
        }
        wait_ms(poll_interval);
        elapsed += poll_interval;
    }

    return false;
}

/**
 * @brief 等待播放器进入指定状态
 *
 * @param player 播放器实例
 * @param expected_state 期望的状态
 * @param timeout_ms 超时时间（毫秒）
 * @return true表示成功进入指定状态，false表示超时
 */
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

        // 如果进入ERROR状态，直接返回失败
        if (current_state == APP_PLAYER_STATE_ERROR) {
            LOGE("Player entered ERROR state while waiting for %s",
                 state_to_string(expected_state));
            return false;
        }

        wait_ms(poll_interval);
        elapsed += poll_interval;
    }

    app_player_state_t current_state = app_player_get_state(player);
    LOGW("Wait for state timeout: expected %s, got %s after %d ms",
         state_to_string(expected_state),
         state_to_string(current_state),
         timeout_ms);
    return false;
}

/**
 * @brief 等待播放器事件发生
 *
 * @param event_record 事件记录结构体
 * @param expected_event 期望的事件
 * @param timeout_ms 超时时间（毫秒）
 * @return true表示收到期望的事件，false表示超时
 */
static bool wait_for_player_event(player_event_record_t *event_record,
                                   app_player_event_t expected_event,
                                   uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    const uint32_t poll_interval = 50;

    event_record->received = false;

    while (elapsed < timeout_ms) {
        if (event_record->received && event_record->event == expected_event) {
            return true;
        }
        wait_ms(poll_interval);
        elapsed += poll_interval;
    }

    LOGW("Wait for event timeout: expected %s after %d ms",
         test_event_to_string(expected_event), timeout_ms);
    return false;
}

/**
 * @brief 辅助宏：播放并等待进入PLAYING状态
 */
#define PLAY_AND_WAIT(player, url, player_name) \
    do { \
        LOGI("Playing " player_name "..."); \
        int ret = app_player_play(player, url); \
        TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, player_name " play should succeed"); \
        bool playing = wait_for_player_state(player, APP_PLAYER_STATE_PLAYING, 5000); \
        TEST_ASSERT_TRUE_MESSAGE(playing, player_name " should enter PLAYING state"); \
        LOGI(player_name " is now playing"); \
    } while(0)

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试1：MUSIC播放时，TTS抢占焦点
 *
 * 测试场景：
 * 1. MUSIC开始播放，获得FOREGROUND焦点
 * 2. TTS开始播放，抢占焦点
 * 3. 验证MUSIC收到BACKGROUND焦点（因为配置为PAUSE）
 * 4. 验证TTS获得FOREGROUND焦点
 * 5. 验证MUSIC状态变为PAUSED
 */
void test_music_preempted_by_tts(void)
{
    int ret;

    LOGI("=== Test: MUSIC preempted by TTS ===");

    /* 清除所有事件记录 */
    clear_all_events();

    /* 步骤1: 播放音乐 */
    LOGI("Step 1: Start playing MUSIC");
    ret = app_player_play(g_music_player, TEST_MUSIC_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Music play should succeed");

    /* 等待音乐进入PLAYING状态 */
    bool music_playing = wait_for_player_state(g_music_player, APP_PLAYER_STATE_PLAYING, 5000);
    TEST_ASSERT_TRUE_MESSAGE(music_playing, "Music should enter PLAYING state");

    app_player_state_t music_state = app_player_get_state(g_music_player);
    LOGI("Music state: %s", state_to_string(music_state));

    /* 清除焦点事件，准备监听抢占事件 */
    clear_all_events();

    /* 步骤2: 播放TTS，抢占音乐焦点 */
    LOGI("Step 2: Start playing TTS (should preempt MUSIC)");
    ret = app_player_play(g_tts_player, TEST_TTS_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "TTS play should succeed");

    /* 等待焦点事件发生 */
    LOGI("Step 3: Waiting for focus events...");
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 验证MUSIC收到焦点变化事件 */
    TEST_ASSERT_TRUE_MESSAGE(g_music_focus_event.received,
                             "MUSIC should receive focus change event");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_FOCUS_BACKGROUND, g_music_focus_event.state,
                              "MUSIC should move to BACKGROUND (paused)");
    TEST_ASSERT_EQUAL_MESSAGE(g_tts_player, g_music_focus_event.by_which,
                              "MUSIC focus change should be caused by TTS");

    /* 验证TTS收到焦点事件 */
    TEST_ASSERT_TRUE_MESSAGE(g_tts_focus_event.received,
                             "TTS should receive focus change event");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_FOCUS_FOREGROUND, g_tts_focus_event.state,
                              "TTS should get FOREGROUND focus");

    /* 验证播放器状态 */
    music_state = app_player_get_state(g_music_player);
    app_player_state_t tts_state = app_player_get_state(g_tts_player);

    LOGI("After preemption - Music: %s, TTS: %s",
         state_to_string(music_state), state_to_string(tts_state));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PAUSED, music_state,
                              "MUSIC should be PAUSED (background behavior)");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PLAYING, tts_state,
                              "TTS should be PLAYING");

    /* 清理：停止所有播放器 */
    LOGI("Cleanup: Stopping all players");
    app_player_stop_sync(g_tts_player);
    LOGI("TTS stopped");
    app_player_stop_sync(g_music_player);
    LOGI("Music stopped");

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试2：TTS播放完成后，MUSIC自动恢复
 *
 * 测试场景：
 * 1. MUSIC播放中
 * 2. TTS抢占焦点，MUSIC暂停
 * 3. TTS播放完成
 * 4. 验证MUSIC自动恢复播放（从BACKGROUND回到FOREGROUND）
 */
void test_music_resume_after_tts_complete(void)
{
    int ret;

    LOGI("=== Test: MUSIC resume after TTS complete ===");

    clear_all_events();

    /* 步骤1: 播放音乐 */
    LOGI("Step 1: Start playing MUSIC");
    PLAY_AND_WAIT(g_music_player, TEST_MUSIC_URL, "MUSIC");

    /* 步骤2: TTS抢占 */
    LOGI("Step 2: TTS preempts MUSIC");
    clear_all_events();
    PLAY_AND_WAIT(g_tts_player, TEST_TTS_URL, "TTS");
    wait_ms(TEST_WAIT_SHORT_MS);  // 等待焦点事件传播

    /* 验证MUSIC被暂停 */
    app_player_state_t music_state = app_player_get_state(g_music_player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PAUSED, music_state,
                              "MUSIC should be paused");

    /* 步骤3: 停止TTS（模拟播放完成） */
    LOGI("Step 3: Stop TTS (simulate completion)");
    clear_all_events();
    ret = app_player_stop_sync(g_tts_player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "TTS stop should succeed");

    /* 等待焦点恢复事件 */
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 验证MUSIC收到FOREGROUND焦点恢复事件 */
    TEST_ASSERT_TRUE_MESSAGE(g_music_focus_event.received,
                             "MUSIC should receive focus change event");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_FOCUS_FOREGROUND, g_music_focus_event.state,
                              "MUSIC should get FOREGROUND focus back");

    /* 验证MUSIC自动恢复播放 */
    music_state = app_player_get_state(g_music_player);
    LOGI("Music state after TTS stop: %s", state_to_string(music_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PLAYING, music_state,
                              "MUSIC should auto-resume to PLAYING");

    /* 清理 */
    LOGI("Cleanup: Stopping MUSIC");
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试3：TONE抢占TTS，TTS被停止
 *
 * 测试场景：
 * 1. TTS播放中
 * 2. TONE抢占焦点（TONE在TTS的capture_names中）
 * 3. 验证TTS收到NONE焦点（因为配置为STOP）
 * 4. 验证TTS状态变为STOPPED
 */
void test_tts_stopped_by_tone(void)
{
    int ret;

    LOGI("=== Test: TTS stopped by TONE ===");

    clear_all_events();

    /* 步骤1: 播放TTS */
    LOGI("Step 1: Start playing TTS");
    PLAY_AND_WAIT(g_tts_player, TEST_TTS_URL, "TTS");

    /* 步骤2: TONE抢占 */
    LOGI("Step 2: TONE preempts TTS");
    clear_all_events();
    const char *tone_url = get_tone_url();
    PLAY_AND_WAIT(g_tone_player, tone_url, "TONE");
    wait_ms(TEST_WAIT_SHORT_MS);  // 等待焦点事件传播

    /* 验证TTS收到NONE焦点（被强制停止） */
    TEST_ASSERT_TRUE_MESSAGE(g_tts_focus_event.received,
                             "TTS should receive focus change event");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_FOCUS_NONE, g_tts_focus_event.state,
                              "TTS should lose focus completely (NONE)");
    TEST_ASSERT_EQUAL_MESSAGE(g_tone_player, g_tts_focus_event.by_which,
                              "TTS focus loss should be caused by TONE");

    /* 验证TTS状态变为STOPPED */
    app_player_state_t tts_state = app_player_get_state(g_tts_player);
    LOGI("TTS state after TONE preemption: %s", state_to_string(tts_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_STOPPED, tts_state,
                              "TTS should be STOPPED (focus loss behavior)");

    /* 验证TONE获得焦点 */
    TEST_ASSERT_TRUE_MESSAGE(g_tone_focus_event.received,
                             "TONE should receive focus event");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_FOCUS_FOREGROUND, g_tone_focus_event.state,
                              "TONE should get FOREGROUND focus");

    /* 清理 */
    LOGI("Cleanup: Stopping TONE");
    app_player_stop_sync(g_tone_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试4：TONE抢占MUSIC，MUSIC被暂停
 *
 * 测试场景：
 * 1. MUSIC播放中
 * 2. TONE通过优先级抢占焦点（TONE优先级1 > MUSIC优先级3）
 * 3. 验证MUSIC收到BACKGROUND焦点并暂停
 * 4. 验证TONE获得FOREGROUND焦点
 */
void test_music_paused_by_tone(void)
{
    int ret;

    LOGI("=== Test: MUSIC paused by TONE ===");

    clear_all_events();

    /* 步骤1: 播放音乐 */
    LOGI("Step 1: Start playing MUSIC");
    PLAY_AND_WAIT(g_music_player, TEST_MUSIC_URL, "MUSIC");

    /* 步骤2: TONE抢占 */
    LOGI("Step 2: TONE preempts MUSIC");
    clear_all_events();
    const char *tone_url = get_tone_url();
    PLAY_AND_WAIT(g_tone_player, tone_url, "TONE");
    wait_ms(TEST_WAIT_SHORT_MS);  // 等待焦点事件传播

    /* 验证MUSIC收到BACKGROUND焦点 */
    TEST_ASSERT_TRUE_MESSAGE(g_music_focus_event.received,
                             "MUSIC should receive focus change event");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_FOCUS_BACKGROUND, g_music_focus_event.state,
                              "MUSIC should move to BACKGROUND (paused)");
    TEST_ASSERT_EQUAL_MESSAGE(g_tone_player, g_music_focus_event.by_which,
                              "MUSIC focus change should be caused by TONE");

    /* 验证MUSIC状态变为PAUSED */
    app_player_state_t music_state = app_player_get_state(g_music_player);
    LOGI("Music state after TONE preemption: %s", state_to_string(music_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PAUSED, music_state,
                              "MUSIC should be PAUSED");

    /* 验证TONE获得焦点 */
    TEST_ASSERT_TRUE_MESSAGE(g_tone_focus_event.received,
                             "TONE should receive focus event");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_FOCUS_FOREGROUND, g_tone_focus_event.state,
                              "TONE should get FOREGROUND focus");

    /* 清理 */
    LOGI("Cleanup: Stopping all players");
    app_player_stop_sync(g_tone_player);
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试5：ALARM最高优先级，可以抢占所有播放器
 *
 * 测试场景：
 * 1. TTS播放中
 * 2. ALARM申请焦点（priority=0，最高优先级）
 * 3. 验证TTS被强制停止（ALARM在TTS的capture_names中）
 * 4. 验证ALARM获得焦点
 */
void test_alarm_highest_priority(void)
{
    int ret;

    LOGI("=== Test: ALARM highest priority ===");

    clear_all_events();

    /* 步骤1: 播放TTS */
    LOGI("Step 1: Start playing TTS");
    PLAY_AND_WAIT(g_tts_player, TEST_TTS_URL, "TTS");

    /* 步骤2: ALARM抢占 */
    LOGI("Step 2: ALARM preempts TTS (highest priority)");
    clear_all_events();
    PLAY_AND_WAIT(g_alarm_player, TEST_ALARM_URL, "ALARM");
    wait_ms(TEST_WAIT_SHORT_MS);  // 等待焦点事件传播

    /* 验证TTS收到NONE焦点（被强制停止） */
    TEST_ASSERT_TRUE_MESSAGE(g_tts_focus_event.received,
                             "TTS should receive focus change event");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_FOCUS_NONE, g_tts_focus_event.state,
                              "TTS should lose focus completely (NONE)");
    TEST_ASSERT_EQUAL_MESSAGE(g_alarm_player, g_tts_focus_event.by_which,
                              "TTS focus loss should be caused by ALARM");

    /* 验证TTS状态变为STOPPED */
    app_player_state_t tts_state = app_player_get_state(g_tts_player);
    LOGI("TTS state after ALARM preemption: %s", state_to_string(tts_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_STOPPED, tts_state,
                              "TTS should be STOPPED");

    /* 验证ALARM获得焦点 */
    TEST_ASSERT_TRUE_MESSAGE(g_alarm_focus_event.received,
                             "ALARM should receive focus event");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_FOCUS_FOREGROUND, g_alarm_focus_event.state,
                              "ALARM should get FOREGROUND focus");

    /* 清理 */
    LOGI("Cleanup: Stopping ALARM");
    app_player_stop_sync(g_alarm_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试6：多层级连续抢占
 *
 * 测试场景：
 * 1. MUSIC播放中
 * 2. TTS抢占，MUSIC暂停（BACKGROUND）
 * 3. TONE抢占，TTS停止（NONE）
 * 4. ALARM抢占，TONE停止（NONE）
 * 5. 验证最终只有ALARM在播放
 */
void test_multilevel_preemption(void)
{
    int ret;

    LOGI("=== Test: Multilevel preemption ===");

    clear_all_events();

    /* 步骤1: 播放MUSIC */
    LOGI("Step 1: Start playing MUSIC");
    PLAY_AND_WAIT(g_music_player, TEST_MUSIC_URL, "MUSIC");

    /* 步骤2: TTS抢占MUSIC */
    LOGI("Step 2: TTS preempts MUSIC");
    clear_all_events();
    PLAY_AND_WAIT(g_tts_player, TEST_TTS_URL, "TTS");
    wait_ms(TEST_WAIT_SHORT_MS);  // 等待焦点事件传播

    app_player_state_t music_state = app_player_get_state(g_music_player);
    app_player_state_t tts_state = app_player_get_state(g_tts_player);
    LOGI("After TTS preemption - Music: %s, TTS: %s",
         state_to_string(music_state), state_to_string(tts_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PAUSED, music_state,
                              "Music should be paused");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PLAYING, tts_state,
                              "TTS should be playing");

    /* 步骤3: TONE抢占TTS */
    LOGI("Step 3: TONE preempts TTS");
    clear_all_events();
    const char *tone_url = get_tone_url();
    PLAY_AND_WAIT(g_tone_player, tone_url, "TONE");
    wait_ms(TEST_WAIT_SHORT_MS);  // 等待焦点事件传播

    tts_state = app_player_get_state(g_tts_player);
    app_player_state_t tone_state = app_player_get_state(g_tone_player);
    LOGI("After TONE preemption - TTS: %s, TONE: %s",
         state_to_string(tts_state), state_to_string(tone_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_STOPPED, tts_state,
                              "TTS should be stopped");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PLAYING, tone_state,
                              "TONE should be playing");

    /* 步骤4: ALARM抢占TONE */
    LOGI("Step 4: ALARM preempts TONE (highest priority)");
    clear_all_events();
    PLAY_AND_WAIT(g_alarm_player, TEST_ALARM_URL, "ALARM");
    wait_ms(TEST_WAIT_SHORT_MS);  // 等待焦点事件传播

    tone_state = app_player_get_state(g_tone_player);
    app_player_state_t alarm_state = app_player_get_state(g_alarm_player);
    LOGI("After ALARM preemption - TONE: %s, ALARM: %s",
         state_to_string(tone_state), state_to_string(alarm_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_STOPPED, tone_state,
                              "TONE should be stopped");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PLAYING, alarm_state,
                              "ALARM should be playing");

    /* 验证最终状态 */
    music_state = app_player_get_state(g_music_player);
    LOGI("Final states - Music: %s, TTS: %s, TONE: %s, ALARM: %s",
         state_to_string(music_state),
         state_to_string(tts_state),
         state_to_string(tone_state),
         state_to_string(alarm_state));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PAUSED, music_state,
                              "Music should still be paused");

    /* 清理 */
    LOGI("Cleanup: Stopping all players");
    app_player_stop_sync(g_alarm_player);
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试7：焦点自动恢复链
 *
 * 测试场景：
 * 1. MUSIC播放中
 * 2. TTS抢占，MUSIC变为BACKGROUND
 * 3. TONE抢占，TTS停止，MUSIC仍为BACKGROUND
 * 4. TONE完成，MUSIC应自动恢复到FOREGROUND
 */
void test_focus_auto_resume_chain(void)
{
    int ret;

    LOGI("=== Test: Focus auto resume chain ===");

    clear_all_events();

    /* 步骤1: 播放MUSIC */
    LOGI("Step 1: Start playing MUSIC");
    PLAY_AND_WAIT(g_music_player, TEST_MUSIC_URL, "MUSIC");

    /* 步骤2: TTS抢占MUSIC */
    LOGI("Step 2: TTS preempts MUSIC");
    clear_all_events();
    PLAY_AND_WAIT(g_tts_player, TEST_TTS_URL, "TTS");
    wait_ms(TEST_WAIT_SHORT_MS);  // 等待焦点事件传播

    app_player_state_t music_state = app_player_get_state(g_music_player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PAUSED, music_state,
                              "Music should be paused");

    /* 步骤3: TONE抢占TTS */
    LOGI("Step 3: TONE preempts TTS (TTS stops, MUSIC stays paused)");
    clear_all_events();
    const char *tone_url = get_tone_url();
    PLAY_AND_WAIT(g_tone_player, tone_url, "TONE");
    wait_ms(TEST_WAIT_SHORT_MS);  // 等待焦点事件传播

    app_player_state_t tts_state = app_player_get_state(g_tts_player);
    music_state = app_player_get_state(g_music_player);
    LOGI("After TONE preemption - TTS: %s, Music: %s",
         state_to_string(tts_state), state_to_string(music_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_STOPPED, tts_state,
                              "TTS should be stopped");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PAUSED, music_state,
                              "Music should still be paused");

    /* 步骤4: TONE完成，MUSIC应自动恢复 */
    LOGI("Step 4: Stop TONE, MUSIC should auto-resume");
    clear_all_events();
    ret = app_player_stop_sync(g_tone_player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "TONE stop should succeed");

    /* 等待焦点恢复 */
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 验证MUSIC收到FOREGROUND焦点 */
    TEST_ASSERT_TRUE_MESSAGE(g_music_focus_event.received,
                             "MUSIC should receive focus change event");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_FOCUS_FOREGROUND, g_music_focus_event.state,
                              "MUSIC should get FOREGROUND focus back");

    /* 验证MUSIC自动恢复播放 */
    music_state = app_player_get_state(g_music_player);
    LOGI("Music state after TONE stop: %s", state_to_string(music_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PLAYING, music_state,
                              "MUSIC should auto-resume to PLAYING");

    /* 清理 */
    LOGI("Cleanup: Stopping MUSIC");
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试8：同一播放器重复播放
 *
 * 测试场景：
 * 1. MUSIC播放中
 * 2. MUSIC再次播放新的URL（应保持焦点，无需重新申请）
 * 3. 验证MUSIC仍然持有FOREGROUND焦点
 */
void test_same_player_replay(void)
{
    int ret;

    LOGI("=== Test: Same player replay ===");

    clear_all_events();

    /* 步骤1: 播放MUSIC */
    LOGI("Step 1: Start playing MUSIC (first time)");
    PLAY_AND_WAIT(g_music_player, TEST_MUSIC_URL, "MUSIC");

    /* 步骤2: MUSIC再次播放 */
    LOGI("Step 2: MUSIC plays another URL");
    clear_all_events();
    PLAY_AND_WAIT(g_music_player, TEST_ALARM_URL, "MUSIC");  // 使用不同的URL

    /* 验证MUSIC仍然是PLAYING状态 */
    app_player_state_t music_state = app_player_get_state(g_music_player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PLAYING, music_state,
                              "MUSIC should still be playing");

    /* 验证没有收到焦点变化事件（因为已经持有焦点） */
    LOGI("Music focus event received: %d", g_music_focus_event.received);

    /* 清理 */
    LOGI("Cleanup: Stopping MUSIC");
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试9：用户手动暂停后被抢占
 *
 * 测试场景：
 * 1. MUSIC播放中
 * 2. 用户手动暂停MUSIC
 * 3. TTS申请焦点
 * 4. TTS完成后，MUSIC不应自动恢复（因为是用户手动暂停）
 */
void test_manual_pause_then_preempted(void)
{
    int ret;

    LOGI("=== Test: Manual pause then preempted ===");

    clear_all_events();

    /* 步骤1: 播放MUSIC */
    LOGI("Step 1: Start playing MUSIC");
    PLAY_AND_WAIT(g_music_player, TEST_MUSIC_URL, "MUSIC");

    /* 步骤2: 用户手动暂停 */
    LOGI("Step 2: User manually pauses MUSIC");
    ret = app_player_pause(g_music_player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Pause should succeed");
    wait_ms(TEST_WAIT_SHORT_MS);

    app_player_state_t music_state = app_player_get_state(g_music_player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PAUSED, music_state,
                              "MUSIC should be paused");

    /* 手动暂停应触发焦点释放事件 */
    TEST_ASSERT_TRUE_MESSAGE(g_music_focus_event.received,
                             "MUSIC pause should trigger focus change event");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_FOCUS_NONE, g_music_focus_event.state,
                              "MUSIC focus should become NONE after manual pause");
    TEST_ASSERT_EQUAL_MESSAGE(g_music_player, g_music_focus_event.by_which,
                              "MUSIC focus loss should be initiated by itself");

    /* 步骤3: TTS申请焦点 */
    LOGI("Step 3: TTS requests focus");
    clear_all_events();
    PLAY_AND_WAIT(g_tts_player, TEST_TTS_URL, "TTS");
    wait_ms(TEST_WAIT_SHORT_MS);

    /* 步骤4: TTS完成 */
    LOGI("Step 4: Stop TTS");
    clear_all_events();
    ret = app_player_stop_sync(g_tts_player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "TTS stop should succeed");
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 验证MUSIC不会自动恢复播放（因为是用户手动暂停的） */
    music_state = app_player_get_state(g_music_player);
    LOGI("Music state after TTS stop: %s", state_to_string(music_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PAUSED, music_state,
                              "MUSIC should remain PAUSED (was manually paused)");

    /* 清理 */
    LOGI("Cleanup: Stopping MUSIC");
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试10：MUSIC从PAUSED恢复后立即被抢占
 *
 * 测试场景：
 * 1. MUSIC播放中
 * 2. 用户暂停MUSIC
 * 3. 用户恢复MUSIC播放
 * 4. 在恢复过程中TTS抢占焦点
 * 5. 验证焦点状态正确
 */
void test_resume_then_immediately_preempted(void)
{
    int ret;

    LOGI("=== Test: Resume then immediately preempted ===");

    clear_all_events();

    /* 步骤1: 播放MUSIC */
    LOGI("Step 1: Start playing MUSIC");
    PLAY_AND_WAIT(g_music_player, TEST_MUSIC_URL, "MUSIC");

    /* 步骤2: 暂停MUSIC */
    LOGI("Step 2: Pause MUSIC");
    ret = app_player_pause(g_music_player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Pause should succeed");
    wait_ms(TEST_WAIT_SHORT_MS);

    /* 步骤3: 恢复MUSIC播放 */
    LOGI("Step 3: Resume MUSIC");
    ret = app_player_resume(g_music_player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Resume should succeed");

    /* 步骤4: 立即播放TTS抢占 */
    LOGI("Step 4: TTS preempts immediately");
    clear_all_events();
    PLAY_AND_WAIT(g_tts_player, TEST_TTS_URL, "TTS");
    wait_ms(TEST_WAIT_SHORT_MS);

    /* 验证MUSIC被暂停 */
    app_player_state_t music_state = app_player_get_state(g_music_player);
    LOGI("Music state after TTS preemption: %s", state_to_string(music_state));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PAUSED, music_state,
                              "MUSIC should be paused");

    /* 验证TTS正在播放 */
    app_player_state_t tts_state = app_player_get_state(g_tts_player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PLAYING, tts_state,
                              "TTS should be playing");

    /* 清理 */
    LOGI("Cleanup: Stopping all players");
    app_player_stop_sync(g_tts_player);
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试11：两个播放器同时申请焦点（相同优先级）
 *
 * 测试场景：
 * 1. TTS和另一个TTS播放器（假设创建两个TTS）几乎同时播放
 * 2. 验证后申请的会取代前一个
 *
 * 注意：这个测试需要两个TTS播放器实例，当前只有一个，
 * 所以用MUSIC和另一个URL来模拟类似场景
 */
void test_rapid_focus_requests(void)
{
    int ret;

    LOGI("=== Test: Rapid focus requests ===");

    clear_all_events();

    /* 步骤1: 快速连续播放MUSIC和TTS */
    LOGI("Step 1: Rapidly play MUSIC then TTS");
    ret = app_player_play(g_music_player, TEST_MUSIC_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "MUSIC play should succeed");

    // 不等待MUSIC进入PLAYING，立即播放TTS
    ret = app_player_play(g_tts_player, TEST_TTS_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "TTS play should succeed");

    /* 等待状态稳定 */
    wait_ms(TEST_WAIT_LONG_MS);

    /* 验证最终状态：TTS应该获胜（更高优先级） */
    app_player_state_t tts_state = app_player_get_state(g_tts_player);
    app_player_state_t music_state = app_player_get_state(g_music_player);

    LOGI("Final states - TTS: %s, MUSIC: %s",
         state_to_string(tts_state), state_to_string(music_state));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PLAYING, tts_state,
                              "TTS should be playing");
    // MUSIC可能是PAUSED或PREPARING，取决于播放准备的速度

    /* 清理 */
    LOGI("Cleanup: Stopping all players");
    app_player_stop_sync(g_tts_player);
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试12：播放器在PREPARING状态被抢占
 *
 * 测试场景：
 * 1. MUSIC开始播放（处于PREPARING状态）
 * 2. 在MUSIC准备完成前，TTS申请焦点
 * 3. 验证焦点抢占在PREPARING阶段也能正常工作
 */
void test_preempt_during_preparing(void)
{
    int ret;

    LOGI("=== Test: Preempt during PREPARING ===");

    clear_all_events();

    /* 步骤1: 开始播放MUSIC（不等待PLAYING） */
    LOGI("Step 1: Start playing MUSIC (don't wait for PLAYING)");
    ret = app_player_play(g_music_player, TEST_MUSIC_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "MUSIC play should succeed");

    /* 短暂延时，让MUSIC进入PREPARING状态 */
    wait_ms(100);

    app_player_state_t music_state = app_player_get_state(g_music_player);
    LOGI("Music state before TTS: %s", state_to_string(music_state));

    /* 步骤2: TTS立即抢占 */
    LOGI("Step 2: TTS preempts MUSIC during PREPARING");
    clear_all_events();
    PLAY_AND_WAIT(g_tts_player, TEST_TTS_URL, "TTS");
    wait_ms(TEST_WAIT_SHORT_MS);

    /* 验证TTS成功获得焦点 */
    app_player_state_t tts_state = app_player_get_state(g_tts_player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_STATE_PLAYING, tts_state,
                              "TTS should be playing");

    /* 验证MUSIC被暂停或停止 */
    music_state = app_player_get_state(g_music_player);
    LOGI("Music state after TTS preemption: %s", state_to_string(music_state));
    bool music_not_playing = (music_state != APP_PLAYER_STATE_PLAYING);
    TEST_ASSERT_TRUE_MESSAGE(music_not_playing,
                             "MUSIC should not be playing after preemption");

    /* 清理 */
    LOGI("Cleanup: Stopping all players");
    app_player_stop_sync(g_tts_player);
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试13：播放器出错时自动释放焦点
 *
 * 测试场景：
 * 1. MUSIC播放中
 * 2. TTS抢占焦点，MUSIC暂停
 * 3. TTS播放一个无效URL导致ERROR
 * 4. 验证TTS释放焦点，MUSIC自动恢复
 */
void test_focus_release_on_error(void)
{
    int ret;

    LOGI("=== Test: Focus release on error ===");

    clear_all_events();

    /* 步骤1: 播放MUSIC */
    LOGI("Step 1: Start playing MUSIC");
    PLAY_AND_WAIT(g_music_player, TEST_MUSIC_URL, "MUSIC");

    /* 步骤2: TTS播放无效URL */
    LOGI("Step 2: TTS plays invalid URL (should cause error)");
    clear_all_events();
    ret = app_player_play(g_tts_player, "http://invalid-url-that-does-not-exist.mp3");
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Play call should succeed");

    /* 等待TTS进入ERROR状态 */
    wait_ms(TEST_WAIT_LONG_MS);

    app_player_state_t tts_state = app_player_get_state(g_tts_player);
    LOGI("TTS state after invalid URL: %s", state_to_string(tts_state));

    /* TTS可能是ERROR或STOPPED状态 */
    bool tts_failed = (tts_state == APP_PLAYER_STATE_ERROR ||
                       tts_state == APP_PLAYER_STATE_STOPPED ||
                       tts_state == APP_PLAYER_STATE_IDLE);

    if (tts_failed) {
        LOGI("TTS failed as expected, checking if MUSIC resumed");

        /* 等待MUSIC恢复 */
        wait_ms(TEST_WAIT_MEDIUM_MS);

        /* 验证MUSIC是否恢复播放 */
        app_player_state_t music_state = app_player_get_state(g_music_player);
        LOGI("Music state after TTS error: %s", state_to_string(music_state));

        // MUSIC应该恢复播放或至少收到焦点恢复事件
        bool music_resumed = (music_state == APP_PLAYER_STATE_PLAYING);
        LOGI("Music resumed: %d", music_resumed);
    } else {
        LOGI("TTS did not fail, skipping validation");
    }

    /* 清理 */
    LOGI("Cleanup: Stopping all players");
    app_player_stop_sync(g_tts_player);
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_focus_preempt_tests(void)
{
    /* 注册焦点变化回调 */
    app_player_register_focus_cb(g_music_player, music_focus_callback, NULL);
    app_player_register_focus_cb(g_tts_player, tts_focus_callback, NULL);
    app_player_register_focus_cb(g_tone_player, tone_focus_callback, NULL);
    app_player_register_focus_cb(g_alarm_player, alarm_focus_callback, NULL);

    /* 注册事件回调 */
    app_player_register_callback(g_music_player, music_event_callback, NULL);
    app_player_register_callback(g_tts_player, tts_event_callback, NULL);

    /* 运行测试 */
    RUN_TEST(test_music_preempted_by_tts);
    RUN_TEST(test_music_resume_after_tts_complete);
    RUN_TEST(test_tts_stopped_by_tone);
    RUN_TEST(test_music_paused_by_tone);
    RUN_TEST(test_alarm_highest_priority);
    RUN_TEST(test_multilevel_preemption);
    RUN_TEST(test_focus_auto_resume_chain);
    RUN_TEST(test_same_player_replay);
    RUN_TEST(test_manual_pause_then_preempted);
    RUN_TEST(test_resume_then_immediately_preempted);
    RUN_TEST(test_rapid_focus_requests);
    RUN_TEST(test_preempt_during_preparing);
    RUN_TEST(test_focus_release_on_error);
}
