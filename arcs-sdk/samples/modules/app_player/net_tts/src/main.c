/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "FreeRTOS.h"
#include "task.h"

#define TAG "samples"
#include "lisa_log.h"
#include "IOMuxManager.h"
#include "net_connect.h"
#include "app_player.h"
#include "lisa_gpio.h"

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

static void player_event_callback(app_player_t *player, app_player_event_t event, void *user_data)
{
    switch (event) {
    case APP_PLAYER_EVENT_PREPARED:
        LOGI("Player prepared");
        break;
    case APP_PLAYER_EVENT_PLAYING:
        LOGI("Player playing");
        break;
    case APP_PLAYER_EVENT_PAUSED:
        LOGI("Player paused");
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
    return lisa_gpio_write_pin(lisa_device_get(PA_GPIO_DEVICE), PA_PIN_NUM, onoff ? LISA_GPIO_HIGH : LISA_GPIO_LOW);;  // 返回0表示成功
}

int main(int argc, char **argv)
{
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

    /* 创建播放器实例 */
    app_player_t *player = app_player_create("tts");
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

    /* 播放网络音频资源 */
    const char *url = "https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/weather.mp3";
    LOGI("Playing URL: %s", url);

    ret = app_player_play(player, url);
    if (ret != APP_PLAYER_OK) {
        LOGE("Failed to play: %d", ret);
        app_player_destroy(player);
        return -1;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    ret = app_player_pause(player);
    if (ret != APP_PLAYER_OK) {
        LOGE("Failed to pause: %d", ret);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    ret = app_player_resume(player);
    if (ret != APP_PLAYER_OK) {
        LOGE("Failed to resume: %d", ret);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    ret = app_player_stop_sync(player);
    if (ret != APP_PLAYER_OK) {
        LOGE("Failed to stop: %d", ret);
    }

    /* 主循环 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
