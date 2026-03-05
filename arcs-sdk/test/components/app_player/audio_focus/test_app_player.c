/*
 * LISA App Player Component - 测试主程序
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 本文件作为测试运行器,负责运行所有测试用例
 */

#define TAG "samples"
#include "lisa_log.h"

#include "unity.h"
#include "test_common.h"
#include <stdio.h>
#include "net_connect.h"
#include "app_player.h"
#include "app_tone.h"
#include "lisa_gpio.h"
#include "IOMuxManager.h"

#define PA_PIN_NUM 27
#define PA_GPIO_DEVICE "gpioa"

/* 测试套件循环次数配置 - 可修改此值来设置循环次数 */
#ifndef TEST_SUITE_LOOP_COUNT
#define TEST_SUITE_LOOP_COUNT 100  // 默认运行1次
#endif

/*
    为满足不同板型示例场景，重定向gpioa设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, PA_PIN_NUM, CSK_IOMUX_FUNC_DEFAULT);
}
#endif

/* ========================================
 * Unity 测试框架钩子函数
 * ======================================== */

/**
 * @brief 在每个测试用例之前执行
 */
void setUp(void)
{
    /* 可以在此处添加通用的测试前置操作 */
}

/**
 * @brief 在每个测试用例之后执行
 */
void tearDown(void)
{
    if (g_tts_player) app_player_reset(g_tts_player);
    if (g_music_player) app_player_reset(g_music_player);
    if (g_tone_player) app_player_reset(g_tone_player);
    if (g_alarm_player) app_player_reset(g_alarm_player);
}

/**
 * @brief PA控制回调函数
 */
static int pa_control_callback(int onoff)
{
    LOGI("PA %s", onoff ? "ON" : "OFF");
    return lisa_gpio_write_pin(lisa_device_get(PA_GPIO_DEVICE), PA_PIN_NUM, onoff ? LISA_GPIO_HIGH : LISA_GPIO_LOW);
}

/* ========================================
 * 主函数
 * ======================================== */

int main(void)
{
    int ret;

    app_tone_init(0x30200000);

    /* 连接网络 */
    net_connect();

    lisa_device_t *gpio_dev = lisa_device_get(PA_GPIO_DEVICE);
    if (!lisa_device_ready(gpio_dev)) {
        LOGE(TAG, "Error: %s device not ready", PA_GPIO_DEVICE);
        return -1;
    }
    ret = lisa_gpio_configure(gpio_dev, PA_PIN_NUM, LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_LOW);
    if (ret != 0) {
        LOGE(TAG, "GPIO configure failed: %d", ret);
        return -1;
    }

    /* 初始化 app_player 模块（带焦点管理） */
    int focus_config_count = 0;
    const app_player_focus_channel_config_t *focus_configs = get_focus_configs(&focus_config_count);

    app_player_config_t app_config = {
        .pa_ctrl_callback = pa_control_callback,
        .focus_configs = focus_configs,
        .focus_config_count = focus_config_count
    };
    ret = app_player_init(&app_config);
    if (ret != APP_PLAYER_OK) {
        LOGE("App player init failed: %d", ret);
        return -1;
    }

    /* 创建4个场景播放器实例 */
    LOGI("Creating players...");

    g_tts_player = app_player_create(PLAYER_NAME_TTS);
    if (g_tts_player == NULL) {
        LOGE("Failed to create TTS player");
        return -1;
    }

    g_music_player = app_player_create(PLAYER_NAME_MUSIC);
    if (g_music_player == NULL) {
        LOGE("Failed to create MUSIC player");
        return -1;
    }

    g_tone_player = app_player_create(PLAYER_NAME_TONE);
    if (g_tone_player == NULL) {
        LOGE("Failed to create TONE player");
        return -1;
    }

    g_alarm_player = app_player_create(PLAYER_NAME_ALARM);
    if (g_alarm_player == NULL) {
        LOGE("Failed to create ALARM player");
        return -1;
    }

    LOGI("All players created successfully");
    LOGI("Focus management enabled with %d channels", focus_config_count);

    /* 测试套件循环配置 */
    int test_loop_count = TEST_SUITE_LOOP_COUNT;

    printf("\n****************************************\n");
    printf("  Test Suite will run %d time(s)\n", test_loop_count);
    printf("****************************************\n");

    /* 循环运行测试套件 */
    int final_result = 0;
    for (int loop_index = 0; loop_index < test_loop_count; loop_index++) {
        printf("\n\n");
        printf("========================================\n");
        printf("  Test Suite Run: %d/%d\n", loop_index + 1, test_loop_count);
        printf("========================================\n");

        /* 开始运行测试套件 */
        UNITY_BEGIN();

        /* 运行各个测试模块 */
        printf("\n========================================\n");
        printf("  Running Focus Preemption Tests\n");
        printf("========================================\n");
        run_focus_preempt_tests();

        printf("\n========================================\n");
        printf("  Running Focus Behavior Tests\n");
        printf("========================================\n");
        run_focus_behavior_tests();

        printf("\n========================================\n");
        printf("  Running Focus Callback Tests\n");
        printf("========================================\n");
        run_focus_callback_tests();

        printf("\n========================================\n");
        printf("  Running Focus Concurrency Tests\n");
        printf("========================================\n");
        run_focus_concurrency_tests();

        printf("\n========================================\n");
        printf("  Test Suite Run %d/%d Completed\n", loop_index + 1, test_loop_count);
        printf("========================================\n");

        /* 结束本轮测试并记录结果 */
        int loop_result = UNITY_END();
        if (loop_result != 0) {
            final_result = loop_result;
        }
    }

    printf("\n");
    printf("========================================\n");
    printf("  All %d Test Suite Run(s) Completed\n", test_loop_count);
    printf("========================================\n");

    /* 清理：销毁所有播放器 */
    LOGI("Cleaning up...");
    if (g_tts_player) app_player_destroy(g_tts_player);
    if (g_music_player) app_player_destroy(g_music_player);
    if (g_tone_player) app_player_destroy(g_tone_player);
    if (g_alarm_player) app_player_destroy(g_alarm_player);

    return final_result;
}
