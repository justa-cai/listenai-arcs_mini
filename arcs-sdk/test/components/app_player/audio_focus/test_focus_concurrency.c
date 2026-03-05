/*
 * LISA App Player Component - 焦点管理并发安全测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 【测试目标】
 * 验证音频焦点管理系统在多线程并发环境下的线程安全性和正确性
 *
 * 【测试覆盖场景】
 * 1. 多播放器同时并发播放 - 验证焦点仲裁的并发正确性
 * 2. 高频操作压力测试 - 快速连续播放/停止，测试系统稳定性
 * 3. 回调函数线程安全 - 并发触发焦点回调，验证数据一致性
 * 4. 混合操作并发 - 播放/暂停/恢复/停止混合执行
 * 5. 动态配置修改安全 - 播放中修改焦点行为策略的线程安全性
 * 6. 动态回调更换安全 - 播放中更换回调函数的线程安全性
 *
 * 【并发控制机制】
 * - 使用FreeRTOS信号量同步多个任务的启动
 * - 使用互斥锁保护共享数据结构（事件记录、错误计数）
 * - 任务同步：确保所有任务同时开始，避免时序影响测试结果
 *
 * 【事件记录系统】
 * - 记录所有焦点变化事件及其时间戳
 * - 线程安全的事件追加（使用互斥锁保护）
 * - 支持最多100个事件记录，用于事后分析
 *
 * 【错误处理】
 * - 可接受的错误：高频操作时的状态转换错误 (ERR_INVALID_STATE)
 * - 不可接受：系统崩溃、死锁、数据损坏
 * - 错误阈值：压力测试<5次，混合操作<10次，其他0次
 *
 * 【测试用例列表】
 * 1. test_concurrent_play_multiple_players    - 4播放器并发播放测试
 * 2. test_rapid_play_stop_stress              - 高频播放停止压力测试
 * 3. test_callback_thread_safety              - 回调线程安全测试
 * 4. test_mixed_operations_concurrency        - 混合操作并发测试
 * 5. test_dynamic_behavior_change_safety      - 动态修改策略安全测试
 * 6. test_dynamic_callback_change_safety      - 动态更换回调安全测试
 *
 * 【性能指标】
 * - 任务栈大小: 4096字节
 * - 并发任务数: 2-4个
 * - 超时时间: 30秒
 * - 信号量等待: 1秒
 *
 * 【注意事项】
 * - 测试耗时较长（约30秒），请耐心等待
 * - 如果测试超时，检查是否存在死锁
 * - 高错误计数可能表示焦点管理存在竞态条件
 */

#include "unity.h"
#include "test_common.h"
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define TAG "test_focus_concurrency"
#include "lisa_log.h"

/* ========================================
 * 并发测试辅助变量
 * ======================================== */

/* 用于同步多个线程的信号量 */
static SemaphoreHandle_t g_sync_sem = NULL;
static SemaphoreHandle_t g_completion_sem = NULL;

/* 用于记录并发事件 */
typedef struct {
    app_player_t *player;
    app_player_focus_state_t state;
    TickType_t timestamp;
    bool called;
} focus_event_t;

#define MAX_EVENTS 100
static focus_event_t g_events[MAX_EVENTS];
static volatile int g_event_count = 0;
static SemaphoreHandle_t g_events_mutex = NULL;

/* 用于错误统计 */
static volatile int g_error_count = 0;
static SemaphoreHandle_t g_error_mutex = NULL;

/* 测试控制标志 */
static volatile bool g_test_running = false;
static volatile int g_active_tasks = 0;

static void set_active_tasks(int value)
{
    taskENTER_CRITICAL();
    g_active_tasks = value;
    taskEXIT_CRITICAL();
    LOGI("[ACTIVE-TASKS] set to %d", value);
}

static void decrement_active_tasks(void)
{
    taskENTER_CRITICAL();
    if (g_active_tasks > 0) {
        g_active_tasks--;
    }
    taskEXIT_CRITICAL();
    LOGI("[ACTIVE-TASKS] decremented, current=%d", g_active_tasks);
}

static int get_active_tasks(void)
{
    int value;
    taskENTER_CRITICAL();
    value = g_active_tasks;
    taskEXIT_CRITICAL();
    LOGD("[ACTIVE-TASKS] read current=%d", value);
    return value;
}

/* ========================================
 * 辅助函数
 * ======================================== */

/**
 * @brief 记录焦点事件
 */
static void record_focus_event(app_player_t *player, app_player_focus_state_t state)
{
    if (xSemaphoreTake(g_events_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (g_event_count < MAX_EVENTS) {
            g_events[g_event_count].player = player;
            g_events[g_event_count].state = state;
            g_events[g_event_count].timestamp = xTaskGetTickCount();
            g_events[g_event_count].called = true;
            g_event_count++;
        }
        xSemaphoreGive(g_events_mutex);
    }
}

/**
 * @brief 清空事件记录
 */
static void clear_events(void)
{
    if (xSemaphoreTake(g_events_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        memset(g_events, 0, sizeof(g_events));
        g_event_count = 0;
        xSemaphoreGive(g_events_mutex);
    }
}

/**
 * @brief 增加错误计数
 */
static void increment_error_count(void)
{
    if (xSemaphoreTake(g_error_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        g_error_count++;
        xSemaphoreGive(g_error_mutex);
    }
}

/**
 * @brief 获取错误计数
 */
static int get_error_count(void)
{
    int count = 0;
    if (xSemaphoreTake(g_error_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        count = g_error_count;
        xSemaphoreGive(g_error_mutex);
    }
    return count;
}

/**
 * @brief 重置错误计数
 */
static void reset_error_count(void)
{
    if (xSemaphoreTake(g_error_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        g_error_count = 0;
        xSemaphoreGive(g_error_mutex);
    }
}

/**
 * @brief 并发测试用焦点回调
 */
static bool concurrent_focus_callback(app_player_t *player,
                                       app_player_focus_state_t state,
                                       app_player_t *by_which,
                                       void *user_data)
{
    LOGI("[CONCURRENT-CB] Player %p focus -> %s (by %p)",
         player, focus_state_to_string(state), by_which);

    record_focus_event(player, state);

    return false;  // 使用默认策略
}

/**
 * @brief 等待所有任务完成
 */
static bool wait_for_all_tasks_complete(uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    const uint32_t poll_interval = 50;

    while (elapsed < timeout_ms) {
        if (get_active_tasks() == 0) {
            return true;
        }
        wait_ms(poll_interval);
        elapsed += poll_interval;
    }

    return false;
}

/* ========================================
 * 并发任务函数
 * ======================================== */

/**
 * @brief 重复播放任务
 */
static void repeated_play_task(void *params)
{
    app_player_t *player = (app_player_t *)params;
    const char *url = NULL;
    const char *name = "unknown";

    if (player == g_tts_player) {
        url = TEST_TTS_URL;
        name = "TTS";
    } else if (player == g_music_player) {
        url = TEST_MUSIC_URL;
        name = "MUSIC";
    } else if (player == g_tone_player) {
        url = get_tone_url();
        name = "TONE";
    } else if (player == g_alarm_player) {
        url = TEST_ALARM_URL;
        name = "ALARM";
    }

    LOGI("[TASK-%s] Started", name);

    /* 等待同步信号 */
    xSemaphoreTake(g_sync_sem, portMAX_DELAY);

    /* 重复播放多次 */
    for (int i = 0; i < 5 && g_test_running; i++) {
        LOGI("[TASK-%s] Play iteration %d", name, i + 1);

        int ret = app_player_play(player, url);
        if (ret != APP_PLAYER_OK) {
            LOGE("[TASK-%s] Play failed: %s", name, error_to_string(ret));
            increment_error_count();
        }

        /* 短暂播放 */
        wait_ms(200 + (i * 50));

        /* 停止 */
        ret = app_player_stop_sync(player);
        if (ret != APP_PLAYER_OK) {
            LOGE("[TASK-%s] Stop failed: %s", name, error_to_string(ret));
            increment_error_count();
        }

        wait_ms(100);
    }

    LOGI("[TASK-%s] Completed", name);

    LOGI("[TASK-%s] Final state before exit: state=%s, error_count=%d", name,
         state_to_string(app_player_get_state(player)), get_error_count());

    decrement_active_tasks();
    xSemaphoreGive(g_completion_sem);
    vTaskDelete(NULL);
}

/**
 * @brief 同时播放和停止任务
 */
static void play_stop_stress_task(void *params)
{
    app_player_t *player = (app_player_t *)params;
    const char *url = NULL;
    const char *name = "unknown";

    if (player == g_tts_player) {
        url = TEST_TTS_URL;
        name = "TTS";
    } else if (player == g_music_player) {
        url = TEST_MUSIC_URL;
        name = "MUSIC";
    }

    LOGI("[STRESS-%s] Started", name);

    /* 等待同步信号 */
    xSemaphoreTake(g_sync_sem, portMAX_DELAY);

    /* 快速连续播放停止 */
    for (int i = 0; i < 10 && g_test_running; i++) {
        int ret = app_player_play(player, url);
        if (ret != APP_PLAYER_OK && ret != APP_PLAYER_ERR_INVALID_STATE) {
            increment_error_count();
        }

        wait_ms(50);

        ret = app_player_stop_sync(player);
        if (ret != APP_PLAYER_OK && ret != APP_PLAYER_ERR_INVALID_STATE) {
            increment_error_count();
        }

        wait_ms(50);
    }

    LOGI("[STRESS-%s] Completed", name);

    decrement_active_tasks();
    xSemaphoreGive(g_completion_sem);
    vTaskDelete(NULL);
}

/**
 * @brief 混合操作任务（播放、暂停、恢复、停止）
 */
static void mixed_operations_task(void *params)
{
    app_player_t *player = (app_player_t *)params;
    const char *url = NULL;
    const char *name = "unknown";

    if (player == g_music_player) {
        url = TEST_MUSIC_URL;
        name = "MUSIC";
    }

    LOGI("[MIXED-%s] Started", name);

    /* 等待同步信号 */
    xSemaphoreTake(g_sync_sem, portMAX_DELAY);

    for (int i = 0; i < 3 && g_test_running; i++) {
        /* 播放 */
        int ret = app_player_play(player, url);
        if (ret != APP_PLAYER_OK) {
            increment_error_count();
        }
        wait_ms(300);

        /* 暂停 */
        ret = app_player_pause(player);
        if (ret != APP_PLAYER_OK && ret != APP_PLAYER_ERR_INVALID_STATE) {
            increment_error_count();
        }
        wait_ms(200);

        /* 恢复 */
        ret = app_player_resume_sync(player);
        if (ret != APP_PLAYER_OK && ret != APP_PLAYER_ERR_INVALID_STATE) {
            increment_error_count();
        }
        wait_ms(300);

        /* 停止 */
        ret = app_player_stop_sync(player);
        if (ret != APP_PLAYER_OK) {
            increment_error_count();
        }
        wait_ms(100);
    }

    LOGI("[MIXED-%s] Completed", name);

    decrement_active_tasks();
    xSemaphoreGive(g_completion_sem);
    vTaskDelete(NULL);
}

/* ========================================
 * 测试用例
 * ======================================== */

/**
 * @brief 测试1：并发播放多个播放器
 *
 * 测试场景：
 * 1. 4个播放器同时启动播放任务
 * 2. 每个播放器重复播放-停止操作
 * 3. 验证焦点管理正确处理并发请求
 * 4. 验证没有死锁或崩溃
 */
void test_concurrent_play_multiple_players(void)
{
    LOGI("=== Test: Concurrent play multiple players ===");

    /* 初始化同步对象 */
    if (!g_sync_sem) {
        g_sync_sem = xSemaphoreCreateCounting(10, 0);
    }
    if (!g_completion_sem) {
        g_completion_sem = xSemaphoreCreateCounting(10, 0);
    }

    clear_events();
    reset_error_count();
    g_test_running = true;
    set_active_tasks(4);

    /* 注册焦点回调 */
    app_player_register_focus_cb(g_tts_player, concurrent_focus_callback, NULL);
    app_player_register_focus_cb(g_music_player, concurrent_focus_callback, NULL);
    app_player_register_focus_cb(g_tone_player, concurrent_focus_callback, NULL);
    app_player_register_focus_cb(g_alarm_player, concurrent_focus_callback, NULL);

    /* 创建并发任务 */
    LOGI("Creating concurrent tasks for 4 players...");
    xTaskCreate(repeated_play_task, "play_tts", 4096, g_tts_player, 5, NULL);
    xTaskCreate(repeated_play_task, "play_music", 4096, g_music_player, 5, NULL);
    xTaskCreate(repeated_play_task, "play_tone", 4096, g_tone_player, 5, NULL);
    xTaskCreate(repeated_play_task, "play_alarm", 4096, g_alarm_player, 5, NULL);

    wait_ms(100);  // 让任务就绪

    /* 同时启动所有任务 */
    LOGI("Starting all tasks simultaneously...");
    for (int i = 0; i < 4; i++) {
        xSemaphoreGive(g_sync_sem);
    }

    /* 等待所有任务完成 */
    LOGI("Waiting for tasks to complete...");
    bool completed = wait_for_all_tasks_complete(30000);
    TEST_ASSERT_TRUE_MESSAGE(completed, "All tasks should complete within timeout");

    g_test_running = false;

    /* 等待额外的清理时间 */
    wait_ms(500);

    /* 验证没有出现错误 */
    int errors = get_error_count();
    LOGI("Total errors during concurrent test: %d", errors);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, errors, "Should have no errors during concurrent operations");

    /* 验证事件被记录 */
    LOGI("Total focus events recorded: %d", g_event_count);
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, g_event_count, "Should have recorded focus events");

    /* 清理 */
    LOGI("Cleanup");
    app_player_reset(g_tts_player);
    app_player_reset(g_music_player);
    app_player_reset(g_tone_player);
    app_player_reset(g_alarm_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试2：高频播放停止压力测试
 *
 * 测试场景：
 * 1. 两个播放器快速连续播放-停止
 * 2. 测试焦点管理在高频操作下的稳定性
 * 3. 验证没有竞态条件导致的问题
 */
void test_rapid_play_stop_stress(void)
{
    LOGI("=== Test: Rapid play-stop stress ===");

    clear_events();
    reset_error_count();
    g_test_running = true;
    set_active_tasks(2);

    /* 创建压力测试任务 */
    LOGI("Creating stress test tasks...");
    xTaskCreate(play_stop_stress_task, "stress_tts", 4096, g_tts_player, 5, NULL);
    xTaskCreate(play_stop_stress_task, "stress_music", 4096, g_music_player, 5, NULL);

    wait_ms(100);

    /* 启动任务 */
    LOGI("Starting stress test...");
    xSemaphoreGive(g_sync_sem);
    xSemaphoreGive(g_sync_sem);

    /* 等待完成 */
    LOGI("Waiting for stress test to complete...");
    bool completed = wait_for_all_tasks_complete(30000);
    TEST_ASSERT_TRUE_MESSAGE(completed, "Stress test should complete within timeout");

    g_test_running = false;
    wait_ms(500);

    /* 验证错误数在可接受范围内 */
    int errors = get_error_count();
    LOGI("Total errors during stress test: %d", errors);
    /* 允许少量状态错误，因为快速操作可能导致状态不一致 */
    TEST_ASSERT_LESS_THAN_MESSAGE(5, errors, "Should have minimal errors during stress test");

    /* 清理 */
    LOGI("Cleanup");
    app_player_reset(g_tts_player);
    app_player_reset(g_music_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试3：回调函数中的并发安全性
 *
 * 测试场景：
 * 1. 多个播放器注册回调
 * 2. 并发触发焦点变化
 * 3. 验证回调执行的线程安全性
 */
void test_callback_thread_safety(void)
{
    LOGI("=== Test: Callback thread safety ===");

    clear_events();
    reset_error_count();
    g_test_running = true;
    set_active_tasks(3);

    /* 注册回调 */
    app_player_register_focus_cb(g_tts_player, concurrent_focus_callback, NULL);
    app_player_register_focus_cb(g_music_player, concurrent_focus_callback, NULL);
    app_player_register_focus_cb(g_tone_player, concurrent_focus_callback, NULL);

    /* 创建任务 */
    LOGI("Creating tasks for callback test...");
    xTaskCreate(repeated_play_task, "cb_tts", 4096, g_tts_player, 5, NULL);
    xTaskCreate(repeated_play_task, "cb_music", 4096, g_music_player, 5, NULL);
    xTaskCreate(repeated_play_task, "cb_tone", 4096, g_tone_player, 5, NULL);

    wait_ms(100);

    /* 启动任务 */
    LOGI("Starting callback test...");
    for (int i = 0; i < 3; i++) {
        xSemaphoreGive(g_sync_sem);
    }

    /* 等待完成 */
    bool completed = wait_for_all_tasks_complete(30000);
    TEST_ASSERT_TRUE_MESSAGE(completed, "Callback test should complete");

    g_test_running = false;
    wait_ms(500);

    /* 验证回调被正确调用 */
    LOGI("Focus events recorded: %d", g_event_count);
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, g_event_count, "Callbacks should be triggered");

    /* 验证没有严重错误 */
    int errors = get_error_count();
    LOGI("Errors: %d", errors);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, errors, "Should have no errors in callback test");

    /* 清理 */
    LOGI("Cleanup");
    app_player_reset(g_tts_player);
    app_player_reset(g_music_player);
    app_player_reset(g_tone_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试4：混合操作并发测试
 *
 * 测试场景：
 * 1. 一个播放器进行复杂操作（播放、暂停、恢复、停止）
 * 2. 其他播放器同时进行播放操作
 * 3. 验证焦点管理正确处理复杂并发场景
 */
void test_mixed_operations_concurrency(void)
{
    LOGI("=== Test: Mixed operations concurrency ===");

    clear_events();
    reset_error_count();
    g_test_running = true;
    set_active_tasks(3);

    /* 注册回调 */
    app_player_register_focus_cb(g_music_player, concurrent_focus_callback, NULL);
    app_player_register_focus_cb(g_tts_player, concurrent_focus_callback, NULL);
    app_player_register_focus_cb(g_tone_player, concurrent_focus_callback, NULL);

    /* 创建混合任务 */
    LOGI("Creating mixed operation tasks...");
    xTaskCreate(mixed_operations_task, "mixed_music", 4096, g_music_player, 5, NULL);
    xTaskCreate(repeated_play_task, "play_tts", 4096, g_tts_player, 5, NULL);
    xTaskCreate(repeated_play_task, "play_tone", 4096, g_tone_player, 5, NULL);

    wait_ms(100);

    /* 启动任务 */
    LOGI("Starting mixed operations test...");
    for (int i = 0; i < 3; i++) {
        xSemaphoreGive(g_sync_sem);
    }

    /* 等待完成 */
    bool completed = wait_for_all_tasks_complete(30000);
    TEST_ASSERT_TRUE_MESSAGE(completed, "Mixed operations test should complete");

    g_test_running = false;
    wait_ms(500);

    /* 验证 */
    int errors = get_error_count();
    LOGI("Errors during mixed operations: %d", errors);
    /* 允许少量错误，因为操作顺序复杂 */
    TEST_ASSERT_LESS_THAN_MESSAGE(10, errors, "Should have minimal errors");

    LOGI("Focus events: %d", g_event_count);
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, g_event_count, "Should have focus events");

    /* 清理 */
    LOGI("Cleanup");
    app_player_reset(g_music_player);
    app_player_reset(g_tts_player);
    app_player_reset(g_tone_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试5：焦点行为动态修改的线程安全性
 *
 * 测试场景：
 * 1. 一个任务不断播放和触发焦点变化
 * 2. 另一个任务动态修改焦点行为策略
 * 3. 验证动态修改不会导致崩溃或数据损坏
 */
void test_dynamic_behavior_change_safety(void)
{
    LOGI("=== Test: Dynamic behavior change safety ===");

    clear_events();
    reset_error_count();

    /* MUSIC 开始播放 */
    LOGI("Start MUSIC playing");
    int ret = app_player_play(g_music_player, TEST_MUSIC_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "MUSIC play should succeed");
    wait_ms(1000);

    /* 在播放过程中多次修改焦点行为 */
    for (int i = 0; i < 10; i++) {
        LOGI("Iteration %d: Changing focus behavior", i);

        app_player_focus_behavior_t behavior;

        if (i % 2 == 0) {
            behavior.on_background = APP_PLAYER_FOCUS_LOSS_PAUSE;
            behavior.on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP;
        } else {
            behavior.on_background = APP_PLAYER_FOCUS_LOSS_IGNORE;
            behavior.on_focus_lost = APP_PLAYER_FOCUS_LOSS_PAUSE;
        }

        ret = app_player_set_focus_behavior(g_music_player, &behavior);
        TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Set behavior should succeed");

        /* TTS 抢占 */
        LOGI("TTS preempts");
        ret = app_player_play(g_tts_player, TEST_TTS_URL);
        TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "TTS play should succeed");

        wait_ms(300);

        /* TTS 停止 */
        ret = app_player_stop_sync(g_tts_player);
        wait_ms(200);

        /* 验证 MUSIC 状态 */
        app_player_state_t music_state = app_player_get_state(g_music_player);
        LOGI("MUSIC state: %s", state_to_string(music_state));

        /* 如果被暂停，恢复播放 */
        if (music_state == APP_PLAYER_STATE_PAUSED) {
            app_player_resume_sync(g_music_player);
        } else if (music_state == APP_PLAYER_STATE_STOPPED || music_state == APP_PLAYER_STATE_IDLE) {
            app_player_play(g_music_player, TEST_MUSIC_URL);
        }

        wait_ms(200);
    }

    /* 验证没有出现错误 */
    int errors = get_error_count();
    LOGI("Errors: %d", errors);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, errors, "Should have no errors");

    /* 清理 */
    LOGI("Cleanup");
    app_player_stop_sync(g_music_player);
    app_player_stop_sync(g_tts_player);

    LOGI("=== Test completed ===\n");
}

/**
 * @brief 测试6：焦点回调动态更换的线程安全性
 *
 * 测试场景：
 * 1. 播放器正在播放
 * 2. 不断动态更换焦点回调
 * 3. 同时触发焦点变化
 * 4. 验证回调更换的线程安全性
 */
void test_dynamic_callback_change_safety(void)
{
    LOGI("=== Test: Dynamic callback change safety ===");

    clear_events();
    reset_error_count();

    /* MUSIC 开始播放 */
    LOGI("Start MUSIC playing");
    int ret = app_player_play(g_music_player, TEST_MUSIC_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "MUSIC play should succeed");
    wait_ms(1000);

    /* 在播放过程中多次更换回调 */
    for (int i = 0; i < 5; i++) {
        LOGI("Iteration %d: Changing focus callback", i);

        /* 交替设置不同的回调 */
        if (i % 2 == 0) {
            ret = app_player_register_focus_cb(g_music_player, concurrent_focus_callback, NULL);
        } else {
            ret = app_player_register_focus_cb(g_music_player, NULL, NULL);
        }
        TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "Register callback should succeed");

        /* TTS 抢占触发焦点变化 */
        LOGI("TTS preempts to trigger focus change");
        ret = app_player_play(g_tts_player, TEST_TTS_URL);
        TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret, "TTS play should succeed");

        wait_ms(300);

        /* TTS 停止 */
        app_player_stop_sync(g_tts_player);
        wait_ms(300);

        /* 恢复 MUSIC */
        app_player_state_t state = app_player_get_state(g_music_player);
        if (state != APP_PLAYER_STATE_PLAYING) {
            if (state == APP_PLAYER_STATE_PAUSED) {
                app_player_resume_sync(g_music_player);
            } else {
                app_player_play(g_music_player, TEST_MUSIC_URL);
            }
            wait_ms(500);
        }
    }

    /* 验证 */
    int errors = get_error_count();
    LOGI("Errors: %d", errors);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, errors, "Should have no errors");

    /* 清理 */
    LOGI("Cleanup");
    app_player_stop_sync(g_music_player);

    LOGI("=== Test completed ===\n");
}

/* ========================================
 * 测试初始化和清理
 * ======================================== */

/**
 * @brief 在测试套件开始前初始化
 */
void test_concurrency_setup(void)
{
    LOGI("Setting up concurrency test environment...");

    /* 创建互斥锁 */
    if (!g_events_mutex) {
        g_events_mutex = xSemaphoreCreateMutex();
    }
    if (!g_error_mutex) {
        g_error_mutex = xSemaphoreCreateMutex();
    }

    clear_events();
    reset_error_count();
    g_test_running = false;
    set_active_tasks(0);

    LOGI("Concurrency test environment ready");
}

/**
 * @brief 在测试套件结束后清理
 */
void test_concurrency_teardown(void)
{
    LOGI("Tearing down concurrency test environment...");

    g_test_running = false;

    /* 等待所有任务结束 */
    wait_ms(1000);

    /* 清理信号量 */
    if (g_sync_sem) {
        vSemaphoreDelete(g_sync_sem);
        g_sync_sem = NULL;
    }
    if (g_completion_sem) {
        vSemaphoreDelete(g_completion_sem);
        g_completion_sem = NULL;
    }
    if (g_events_mutex) {
        vSemaphoreDelete(g_events_mutex);
        g_events_mutex = NULL;
    }
    if (g_error_mutex) {
        vSemaphoreDelete(g_error_mutex);
        g_error_mutex = NULL;
    }

    LOGI("Concurrency test environment cleaned up");
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_focus_concurrency_tests(void)
{
    LOGI("========================================");
    LOGI("  Running Focus Concurrency Tests");
    LOGI("========================================\n");

    test_concurrency_setup();

    RUN_TEST(test_concurrent_play_multiple_players);
    RUN_TEST(test_rapid_play_stop_stress);
    RUN_TEST(test_callback_thread_safety);
    RUN_TEST(test_mixed_operations_concurrency);
    RUN_TEST(test_dynamic_behavior_change_safety);
    RUN_TEST(test_dynamic_callback_change_safety);

    test_concurrency_teardown();

    LOGI("\n========================================");
    LOGI("  Focus Concurrency Tests Completed");
    LOGI("========================================\n");
}
