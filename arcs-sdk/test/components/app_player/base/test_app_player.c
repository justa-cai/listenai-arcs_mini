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
#include "lisa_gpio.h"
#include "IOMuxManager.h"

#define PA_PIN_NUM 27
#define PA_GPIO_DEVICE "gpioa"

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
    /* 可以在此处添加通用的测试清理操作 */
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
    printf("\n");
    printf("========================================\n");
    printf("  LISA App Player Component Unit Tests\n");
    printf("========================================\n");
    printf("Test URL: %s\n", TEST_URL);
    printf("========================================\n\n");

    int ret;

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

    /* 初始化 app_player 模块 */
    app_player_config_t app_config = {
        .pa_ctrl_callback = pa_control_callback
    };
    ret = app_player_init(&app_config);
    if (ret != APP_PLAYER_OK) {
        LOGE("App player init failed: %d", ret);
        return -1;
    }

    /* 开始运行测试套件 */
    UNITY_BEGIN();

    /* 运行各个测试模块 */
    printf("\n>>> Running Player Lifecycle Tests...\n");
    run_player_lifecycle_tests();

    printf("\n>>> Running Player Callback Tests...\n");
    run_player_callback_tests();

    printf("\n>>> Running Player State Tests...\n");
    run_player_state_tests();

    printf("\n>>> Running Player Play Tests...\n");
    run_player_play_tests();

    printf("\n>>> Running Player Control Tests...\n");
    run_player_control_tests();

    printf("\n>>> Running Player Seek Tests...\n");
    run_player_seek_tests();

    printf("\n>>> Running Player Volume Tests...\n");
    run_player_volume_tests();

    printf("\n>>> Running Player Stream Tests...\n");
    run_player_stream_tests();

    printf("\n========================================\n");
    printf("  All Tests Completed\n");
    printf("========================================\n");

    return UNITY_END();
}
