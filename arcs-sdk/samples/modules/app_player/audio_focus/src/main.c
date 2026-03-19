/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>

#define TAG "audio_focus_sample"
#include "lisa_log.h"

#include "IOMuxManager.h"
#include "app_player.h"
#include "lisa_gpio.h"
#include "tone.h"
#include "app_tone.h"

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

// 播放器实例
static app_player_t *tone_player = NULL;
static app_player_t *tts_player = NULL;
static app_player_t *music_player = NULL;

// 控制变量
static bool tts_play_completed = false;

/**
 * @brief 播放器事件回调
 */
static void player_event_callback(app_player_t *player, app_player_event_t event, void *user_data)
{
    const char *player_name = (const char *)user_data;

    switch (event) {
    case APP_PLAYER_EVENT_PREPARED:
        LOGI("[%s] Player prepared", player_name);
        break;
    case APP_PLAYER_EVENT_PLAYING:
        LOGI("[%s] Player playing", player_name);
        break;
    case APP_PLAYER_EVENT_PAUSED:
        LOGI("[%s] Player paused", player_name);
        break;
    case APP_PLAYER_EVENT_COMPLETED:
        if(player == tts_player) {
            tts_play_completed = true;
        }
        LOGI("[%s] Player completed", player_name);
        break;
    case APP_PLAYER_EVENT_ERROR:
        LOGE("[%s] Player error", player_name);
        break;
    case APP_PLAYER_EVENT_STOPPED:
        LOGI("[%s] Player stopped", player_name);
        break;
    default:
        break;
    }
}

/**
 * @brief 焦点变化回调 - 音乐播放器专用
 *
 * @note 此回调仅用于监听焦点变化事件，不需要手动执行 pause/resume/stop
 *       app_player 会根据配置的 behavior 自动执行相应策略
 * @return false 表示让 app_player 执行默认策略，true 表示完全接管
 */
static bool music_focus_change_callback(app_player_t *player,
                                        app_player_focus_state_t state,
                                        app_player_t *by_which,
                                        void *user_data)
{
    const char *player_name = (const char *)user_data;

    switch (state) {
        case APP_PLAYER_FOCUS_FOREGROUND:
            LOGI("[%s] Got FOREGROUND focus (by player %p)", player_name, by_which);

            // 如果是 tone 完成后恢复焦点，先不做任何操作
            if (by_which == tone_player) {
                LOGI("[%s] Tone completed, but waiting for TTS...", player_name);
                // 返回 true 阻止自动恢复播放
                return true;
            }
            break;

        case APP_PLAYER_FOCUS_BACKGROUND:
            LOGI("[%s] Moved to BACKGROUND (by player %p)", player_name, by_which);
            break;

        case APP_PLAYER_FOCUS_NONE:
            LOGI("[%s] Lost focus (by player %p)", player_name, by_which);
            break;
    }

    // 返回 false，让 app_player 根据配置自动执行策略
    return false;
}

/**
 * @brief 焦点变化回调 - 通用版本
 */
static bool focus_change_callback(app_player_t *player,
                                   app_player_focus_state_t state,
                                   app_player_t *by_which,
                                   void *user_data)
{
    const char *player_name = (const char *)user_data;

    switch (state) {
        case APP_PLAYER_FOCUS_FOREGROUND:
            LOGI("[%s] Got FOREGROUND focus (by player %p)", player_name, by_which);
            break;

        case APP_PLAYER_FOCUS_BACKGROUND:
            LOGI("[%s] Moved to BACKGROUND (by player %p)", player_name, by_which);
            break;

        case APP_PLAYER_FOCUS_NONE:
            LOGI("[%s] Lost focus (by player %p)", player_name, by_which);
            break;
    }

    // 返回 false，让 app_player 根据配置自动执行策略
    return false;
}

/**
 * @brief PA控制回调函数
 */
static int pa_control_callback(int onoff)
{
    LOGI("PA %s", onoff ? "ON" : "OFF");
    return lisa_gpio_write_pin(lisa_device_get(PA_GPIO_DEVICE), PA_PIN_NUM, onoff ? LISA_GPIO_HIGH : LISA_GPIO_LOW);
}

int main(int argc, char **argv)
{
    int ret;

    LOGI("=== Audio Focus Management Sample ===");

    /* 连接网络 */
    net_connect();

    /* 初始化本地提示音文件 */
    app_tone_init(0x30200000);

    /* 初始化PA控制GPIO */
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

    /* 定义焦点通道配置 */
    app_player_focus_channel_config_t focus_configs[] = {
        {
            .name = "tone",
            .priority = 0,  // 最高优先级（本地提示音）
            .capture_names = (const char *[]){"tts"},
            .capture_count = 1,
            .behavior = {
                .on_background = APP_PLAYER_FOCUS_LOSS_STOP,
                .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
            }
        },
        {
            .name = "tts",
            .priority = 1,  // 中等优先级
            .capture_names = (const char *[]){"tone"},
            .capture_count = 1,
            .behavior = {
                .on_background = APP_PLAYER_FOCUS_LOSS_STOP,
                .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
            }
        },
        {
            .name = "music",
            .priority = 2,  // 最低优先级
            .capture_names = NULL,
            .capture_count = 0,
            .behavior = {
                .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
                .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
            }
        }
    };

    /* 初始化 app_player 模块（带焦点管理） */
    app_player_config_t app_config = {
        .pa_ctrl_callback = pa_control_callback,
        .focus_configs = focus_configs,
        .focus_config_count = 3
    };

    ret = app_player_init(&app_config);
    if (ret != APP_PLAYER_OK) {
        LOGE("App player init failed: %d", ret);
        return -1;
    }

    /* 创建 3 个播放器实例 */
    LOGI("Creating players...");

    tone_player = app_player_create("tone");
    if (tone_player == NULL) {
        LOGE("Failed to create tone player");
        return -1;
    }

    tts_player = app_player_create("tts");
    if (tts_player == NULL) {
        LOGE("Failed to create tts player");
        return -1;
    }

    music_player = app_player_create("music");
    if (music_player == NULL) {
        LOGE("Failed to create music player");
        return -1;
    }

    /* 注册事件回调 */
    app_player_register_callback(tone_player, player_event_callback, "TONE");
    app_player_register_callback(tts_player, player_event_callback, "TTS");
    app_player_register_callback(music_player, player_event_callback, "MUSIC");

    /* 注册焦点变化回调 */
    app_player_register_focus_cb(tone_player, focus_change_callback, "TONE");
    app_player_register_focus_cb(tts_player, focus_change_callback, "TTS");
    app_player_register_focus_cb(music_player, music_focus_change_callback, "MUSIC");

    LOGI("All players created and registered");
    LOGI("");

    /* ========== 场景演示 ========== */

    /* 场景 1: 播放音乐 */
    LOGI("=== Scenario 1: Playing music ===");
    app_player_play(music_player, "https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/haier-ac/%E9%9D%92%E8%97%8F%E9%AB%98%E5%8E%9F.mp3");
    LOGI("Music is playing...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    /* 场景 2: 播放本地提示音（会抢占音乐，音乐自动暂停） */
    LOGI("");
    LOGI("=== Scenario 2: Playing tone (music will pause) ===");
    const char *tone_url = app_tone_get_url(TONE_ID_0);
    app_player_play(tone_player, tone_url);
    LOGI("Waiting for tone to complete...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* 场景 3: tone 播放完毕后，等待 3 秒再播放 TTS */
    LOGI("");
    LOGI("=== Scenario 3: Tone completed, waiting 3s before TTS ===");
    LOGI("Music should NOT resume yet (by_which=tone_player, blocked in callback)");
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* 场景 4: 播放 TTS（会再次抢占音乐焦点） */
    LOGI("");
    LOGI("=== Scenario 4: Playing TTS (music stays paused) ===");
    app_player_play(tts_player, "https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/weather.mp3");
    LOGI("TTS is playing...");
    vTaskDelay(pdMS_TO_TICKS(8000));

    /* 场景 5: TTS 播放完毕，音乐自动恢复 */
    LOGI("");
    LOGI("=== Scenario 5: TTS completed, music should auto-resume ===");
    while(!tts_play_completed) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    /* 清理 */
    vTaskDelay(pdMS_TO_TICKS(5000));
    LOGI("");
    LOGI("=== Stopping all players ===");
    app_player_stop_sync(tone_player);
    app_player_stop_sync(tts_player);
    app_player_stop_sync(music_player);

    LOGI("");
    LOGI("=== Demo completed ===");

    /* 主循环 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
