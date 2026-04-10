/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "FreeRTOS.h"
#include "task.h"

#define TAG "mem_audio_sample"
#include "lisa_log.h"

#include "IOMuxManager.h"
#include "app_player.h"
#include "lisa_gpio.h"
#include <stdio.h>

/*
 * 音频资源配置
 * 这些地址和大小需要根据实际烧录的音频文件进行配置
 * 默认配置：Flash地址 0x30100000，预留空间 128KB
 */
#define AUDIO_FLASH_ADDR    0x30100000  /* 音频数据在Flash中的起始地址 */
#define AUDIO_DATA_SIZE     8192        /* 音频数据大小（字节），需要根据实际文件大小调整 */

/*
    为满足不同板型示例场景，重定向gpioa设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_MINI
#include "pinmux.h"

#define PA_PIN_NUM PA_EN_PIN
#define PA_GPIO_DEVICE "gpioa"

#elif defined(CONFIG_BOARD_ARCS_EVB)

#define PA_PIN_NUM 27
#define PA_GPIO_DEVICE "gpioa"

void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, PA_PIN_NUM, CSK_IOMUX_FUNC_DEFAULT);
}
#endif

/**
 * @brief 播放器事件回调函数
 */
static void player_event_callback(app_player_t *player, app_player_event_t event, void *user_data)
{
    switch (event) {
    case APP_PLAYER_EVENT_PREPARED:
        LOGI("Player prepared");
        break;
    case APP_PLAYER_EVENT_PLAYING:
        LOGI("Player playing");
        break;
    case APP_PLAYER_EVENT_COMPLETED:
        LOGI("Player completed");
        break;
    case APP_PLAYER_EVENT_ERROR:
        LOGE("Player error");
        break;
    case APP_PLAYER_EVENT_STOPPED:
        LOGI("Player stopped");
        break;
    case APP_PLAYER_EVENT_PAUSED:
        LOGI("Player paused");
        break;
    default:
        break;
    }
}

/**
 * @brief PA控制回调函数
 */
static int pa_control_callback(int onoff)
{
    LOGI("PA %s", onoff ? "ON" : "OFF");
    return lisa_gpio_write_pin(lisa_device_get(PA_GPIO_DEVICE), PA_PIN_NUM,
                                onoff ? LISA_GPIO_HIGH : LISA_GPIO_LOW);
}

int main(int argc, char **argv)
{
    int ret;
    char mem_url[64];

    LOGI("=== Memory Audio Playback Sample ===");
    LOGI("Audio Flash Address: 0x%08X", AUDIO_FLASH_ADDR);
    LOGI("Audio Data Size: %d bytes", AUDIO_DATA_SIZE);

    /* 初始化PA控制GPIO */
    lisa_device_t *gpio_dev = lisa_device_get(PA_GPIO_DEVICE);
    if (!lisa_device_ready(gpio_dev)) {
        LOGE("Error: %s device not ready", PA_GPIO_DEVICE);
        return -1;
    }
    ret = lisa_gpio_configure(gpio_dev, PA_PIN_NUM, LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_LOW);
    if (ret != 0) {
        LOGE("GPIO configure failed: %d", ret);
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

    /* 创建播放器实例 */
    app_player_t *player = app_player_create("mem_audio");
    if (player == NULL) {
        LOGE("Failed to create player");
        return -1;
    }

    /* 注册事件回调 */
    ret = app_player_register_callback(player, player_event_callback, NULL);
    if (ret != APP_PLAYER_OK) {
        LOGE("Failed to register callback: %d", ret);
        app_player_destroy(player);
        return -1;
    }

    /* 构造mem://协议URL */
    snprintf(mem_url, sizeof(mem_url), "mem://addr=%usize=%u",
             AUDIO_FLASH_ADDR, AUDIO_DATA_SIZE);
    LOGI("Memory URL: %s", mem_url);

    /* ========== 演示场景 ========== */

    /* 场景 1: 播放内存音频 */
    LOGI("");
    LOGI("=== Scenario 1: Play memory audio ===");
    ret = app_player_play(player, mem_url);
    if (ret != APP_PLAYER_OK) {
        LOGE("Failed to play: %d", ret);
        app_player_destroy(player);
        return -1;
    }
    LOGI("Playing audio from memory...");
    vTaskDelay(pdMS_TO_TICKS(500));

    /* 场景 2: 暂停播放 */
    LOGI("");
    LOGI("=== Scenario 2: Pause playback ===");
    ret = app_player_pause(player);
    if (ret != APP_PLAYER_OK) {
        LOGE("Failed to pause: %d", ret);
    } else {
        LOGI("Audio paused");
    }
    vTaskDelay(pdMS_TO_TICKS(500));

    /* 场景 3: 恢复播放 */
    LOGI("");
    LOGI("=== Scenario 3: Resume playback ===");
    ret = app_player_resume(player);
    if (ret != APP_PLAYER_OK) {
        LOGE("Failed to resume: %d", ret);
    } else {
        LOGI("Audio resumed");
    }
    vTaskDelay(pdMS_TO_TICKS(500));

    /* 场景 4: 停止播放 */
    LOGI("");
    LOGI("=== Scenario 4: Stop playback ===");
    ret = app_player_stop(player);
    if (ret != APP_PLAYER_OK) {
        LOGE("Failed to stop: %d", ret);
    } else {
        LOGI("Audio stopped");
    }

    LOGI("");
    LOGI("=== Sample completed ===");
    LOGI("You can modify AUDIO_FLASH_ADDR and AUDIO_DATA_SIZE");
    LOGI("to match your actual audio file location and size.");

    /* 主循环 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
