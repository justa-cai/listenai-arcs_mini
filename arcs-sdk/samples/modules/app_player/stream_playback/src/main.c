/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief 流式播放示例
 *
 * 演示如何使用 app_player 的流式播放接口播放实时生成的 PCM 数据。
 *
 * 核心 API:
 *   - app_player_play_stream()   开始流式播放，指定音频格式
 *   - app_player_write_stream()  写入 PCM 数据
 *   - app_player_finish_stream() 结束流式播放
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "lisa_log.h"
#include "lisa_thread.h"
#include "lisa_gpio.h"
#include "app_player.h"

#ifdef CONFIG_BOARD_ARCS_MINI
#include "pinmux.h"
#elif defined(CONFIG_BOARD_ARCS_EVB)
#include "IOMuxManager.h"
#endif

#define TAG "STREAM_SAMPLE"

/* 音频参数 */
#define SAMPLE_RATE 16000
#define CHANNELS    1
#define BITS        16

static app_player_t *g_player = NULL;
static volatile bool g_completed = false;

/*
    为满足不同板型示例场景，重定向gpioa设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_MINI

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

/* PA 控制回调 */
static int pa_ctrl_callback(int onoff)
{
    LISA_LOGI(TAG, "PA %s", onoff ? "ON" : "OFF");
    return lisa_gpio_write_pin(lisa_device_get(PA_GPIO_DEVICE), PA_PIN_NUM,
                               onoff ? LISA_GPIO_HIGH : LISA_GPIO_LOW);
}

/* 播放器事件回调 */
static void event_callback(app_player_t *player, app_player_event_t event, void *user_data)
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
             g_completed = true;
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
 * @brief 生成正弦波 PCM 数据
 * @param freq      频率 (Hz)
 * @param duration  时长 (ms)
 * @param out_size  输出数据大小
 * @return PCM 数据缓冲区（需调用 lisa_mem_free 释放）
 */
static int16_t *generate_sine_wave(int freq, int duration, int *out_size)
{
    int samples = (SAMPLE_RATE * duration) / 1000;
    int size = samples * sizeof(int16_t);
    int16_t *buf = lisa_mem_alloc(size);
    if (!buf) return NULL;

    for (int i = 0; i < samples; i++) {
        buf[i] = (int16_t)(16384.0f * sinf(2.0f * M_PI * freq * i / SAMPLE_RATE));
    }
    *out_size = size;
    return buf;
}

int main(void)
{
    LISA_LOGI(TAG, "=== 流式播放示例 ===");

    /* 1. 初始化 PA GPIO */
    lisa_device_t *gpio = lisa_device_get(PA_GPIO_DEVICE);
    lisa_gpio_configure(gpio, PA_PIN_NUM, LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_LOW);

    /* 2. 初始化 app_player */
    app_player_config_t config = { .pa_ctrl_callback = pa_ctrl_callback };
    app_player_init(&config);

    /* 3. 创建播放器并注册回调 */
    g_player = app_player_create("stream_demo");
    app_player_register_callback(g_player, event_callback, NULL);

    /* 4. 开始流式播放 */
    app_player_play_stream(g_player, SAMPLE_RATE, CHANNELS, BITS);
    lisa_thread_mdelay(50);

    /* 5. 生成并写入 PCM 数据（播放 1kHz 正弦波 1 秒） */
    int data_size;
    int16_t *pcm_data = generate_sine_wave(1000, 1000, &data_size);
    if (pcm_data) {
        app_player_write_stream(g_player, (uint8_t *)pcm_data, data_size, 1000);
        lisa_mem_free(pcm_data);
    }

    /* 6. 结束流式播放 */
    app_player_finish_stream(g_player);

    /* 7. 等待播放完成 */
    while (!g_completed) {
        lisa_thread_mdelay(100);
    }

    /* 8. 清理 */
    app_player_destroy(g_player);

    LISA_LOGI(TAG, "=== 示例结束 ===");

    while (1) lisa_thread_mdelay(1000);
    return 0;
}
