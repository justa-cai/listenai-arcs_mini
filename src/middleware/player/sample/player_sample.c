/**
 * @file main.c
 * @brief 播放器管理示例程序
 * 
 * 演示如何使用 player_mgr 播放本地提示音、TTS 和网络音频
 */

#include <stdio.h>
#include <string.h>
#include "lisa_log.h"
#include "evs_utils.h"
#include "app_tone.h"
#include "lisa_thread.h"
#include "player_mgr.h"

#define TAG "player_sample"

// ============== 平台相关的 weak 函数（需要用户实现）==============

/**
 * @brief DAC 初始化
 * @return 0 成功，其他失败
 */
__attribute__((weak)) int platform_dac_init(void)
{
    return 0;
}

/**
 * @brief PA（功放）初始化
 * @return 0 成功，其他失败
 */
__attribute__((weak)) int platform_pa_init(void)
{
    return 0;
}

/**
 * @brief 网络连接初始化
 * @return 0 成功，其他失败
 */
__attribute__((weak)) int platform_network_connect(void)
{
    return 0;
}

/**
 * @brief 本地资源加载（如挂载文件系统）
 * @return 0 成功，其他失败
 */
__attribute__((weak)) int platform_resource_load(void)
{
    return 0;
}

__attribute__((weak)) void platform_utils_init(const char *item_id, char *url)
{

}

// ============== 播放器配置 ==============

static player_config_t s_player_configs[] = {
    {
        .id = AIP,
        .name = "AIP",
        .priority = 30,
        .capture_ids = {AIP, TTS, LOCAL},
        .capture_count = 3,
        .fg_action = PLAY_ACTION_RECOGNIZE,
        .bg_action = PLAY_ACTION_RECOGNIZE_END,
        .none_action = PLAY_ACTION_RECOGNIZE_END,
    },
    {
        .id = TTS,
        .name = "TTS",
        .priority = 10,
        .capture_ids = {AIP, ALERT},
        .capture_count = 2,
        .fg_action = PLAY_ACTION_PLAY,
        .bg_action = PLAY_ACTION_STOP,
        .none_action = PLAY_ACTION_STOP,
    },
    {
        .id = ALERT,
        .name = "ALERT",
        .priority = 40,
        .capture_ids = {AIP, TTS, LOCAL},
        .capture_count = 3,
        .fg_action = PLAY_ACTION_PLAY,
        .bg_action = PLAY_ACTION_STOP,
        .none_action = PLAY_ACTION_STOP,
    },
    {
        .id = CONTENT,
        .name = "CONTENT",
        .priority = 50,
        .capture_ids = {AIP, TTS, ALERT, LOCAL},
        .capture_count = 4,
        .fg_action = PLAY_ACTION_PLAY,
        .bg_action = PLAY_ACTION_PAUSE,
        .none_action = PLAY_ACTION_STOP,
    },
    {
        .id = LOCAL,
        .name = "LOCAL",
        .priority = 10,
        .capture_ids = {AIP},
        .capture_count = 1,
        .fg_action = PLAY_ACTION_PLAY,
        .bg_action = PLAY_ACTION_STOP,
        .none_action = PLAY_ACTION_STOP,
    },
};

// ============== 回调函数 ==============

static void on_play_status(int player_id, uint16_t status, void *arg)
{
    LISA_LOGI(TAG, "[STATUS] player_id=%d, status=%d", player_id, status);
}

static void on_focus_change(int player_id, focus_state_e state, int by_which, void *arg)
{
    const char *state_str = "UNKNOWN";
    switch (state) {
        case FOREGROUND: state_str = "FOREGROUND"; break;
        case BACKGROUND: state_str = "BACKGROUND"; break;
        case FOCUS_NONE: state_str = "NONE"; break;
    }
    LISA_LOGI(TAG, "[FOCUS] player_id=%d, state=%s, by_which=%d", player_id, state_str, by_which);
}

// ============== 平台初始化 ==============

static int platform_init(void)
{
    int ret;

    LISA_LOGI(TAG, "========== platform init start ==========");

    ret = platform_dac_init();
    if (ret != 0) {
        LISA_LOGE(TAG, "DAC init failed: %d", ret);
        return ret;
    }

    ret = platform_pa_init();
    if (ret != 0) {
        LISA_LOGE(TAG, "PA init failed: %d", ret);
        return ret;
    }

    ret = platform_resource_load();
    if (ret != 0) {
        LISA_LOGE(TAG, "resource load failed: %d", ret);
        return ret;
    }

    ret = platform_network_connect();
    if (ret != 0) {
        LISA_LOGE(TAG, "network connect failed: %d", ret);
        return ret;
    }

    LISA_LOGI(TAG, "========== platform init end ==========");
    return 0;
}

// ============== 播放示例 ==============

/**
 * @brief 示例1：播放本地提示音
 */
static void demo_play_local_tone(void)
{
    LISA_LOGI(TAG, ">>> example 1: play local tone");
    player_mgr_play(LOCAL, app_tone_get_url(0), 0);
}

/**
 * @brief 示例2：播放 TTS
 */
static void demo_play_tts(void)
{
    LISA_LOGI(TAG, ">>> example 2: play TTS");
    player_mgr_play(TTS, "https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/weather.mp3", 300);
}

/**
 * @brief 示例3：播放网络音乐
 */
static void demo_play_url(void)
{
    LISA_LOGI(TAG, ">>> example 3: play network music");
    player_mgr_play(CONTENT, "https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/weather.mp3", 0);
}

// ============== 主函数 ==============

int player_sample_init(void)
{
    int ret;

    LISA_LOGI(TAG, "========================================");
    LISA_LOGI(TAG, "        player manager sample program");
    LISA_LOGI(TAG, "========================================");

    // 1. 平台初始化
    ret = platform_init();
    if (ret != 0) {
        LISA_LOGE(TAG, "platform init failed");
        return -1;
    }

    // 2. 初始化播放管理器
    ret = player_mgr_init(s_player_configs, sizeof(s_player_configs) / sizeof(s_player_configs[0]));
    if (ret != 0) {
        LISA_LOGE(TAG, "player manager init failed: %d", ret);
        return -1;
    }
    LISA_LOGI(TAG, "player manager init success");

    // 3. 注册回调
    player_mgr_register_status_cb(TTS, on_play_status, NULL);
    player_mgr_register_status_cb(LOCAL, on_play_status, NULL);
    player_mgr_register_status_cb(CONTENT, on_play_status, NULL);

    player_mgr_register_focus_cb(TTS, on_focus_change, NULL);
    player_mgr_register_focus_cb(LOCAL, on_focus_change, NULL);
    player_mgr_register_focus_cb(CONTENT, on_focus_change, NULL);

    // 4. 运行示例
    demo_play_local_tone();  // 播放本地提示音
	lisa_thread_delay(20);
    demo_play_tts();      // 播放 TTS
    // demo_play_url();      // 播放网络音乐

    LISA_LOGI(TAG, "example program running...");

    return 0;
}
