/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdint.h>
#include "audio_pcm.h"
#include "audio_pcm_8k.h"

#include "FreeRTOS.h"
#include "task.h"

#include "shell.h"

#define TAG "app-bt"
#include "lisa_log.h"

#include "lisa_shell.h"
#include "shell_passthrough.h"
#include "lisa_bluetooth.h"
#include "lisa_audio.h"
#include "fs/user_fs.h"
#include "bt_audio_interface_virtual.h"

#define WIFI_MGR_SEARCH_AP_BUFFER_SIZE 10
#define WIFI_MGR_AUTO_CONNECT_INTERVAL_MS 2000

// 音频参数配置
#define FRAME_TIME_MS  7.5

#define AUDIO_MODE_STR      "Encode (PCM → MSBC)"

// 音频任务相关
static TaskHandle_t audio_task_handle = NULL;
static bool g_audio_start = false;
static bt_audio_format_t g_audio_format = {0};

lisa_device_t *audio_dev;
#define LISA_AUDIO_DEVICE_NAME "audio0"

/* 转换采样率 */
static lisa_audio_rate_t convert_sample_rate(uint32_t rate)
{
    switch (rate) {
        case 8000:
            return LISA_AUDIO_RATE_8K;
        case 16000:
            return LISA_AUDIO_RATE_16K;
        case 24000:
            return LISA_AUDIO_RATE_24K;
        case 32000:
            return LISA_AUDIO_RATE_32K;
        case 48000:
            return LISA_AUDIO_RATE_48K;
        case 96000:
            return LISA_AUDIO_RATE_96K;
        default:
            return LISA_AUDIO_RATE_48K;
    }
}

/* 转换声道配置 */
static lisa_audio_channel_t convert_channels(uint8_t channels)
{
    switch (channels) {
        case 1:
            return LISA_AUDIO_CH_LEFT;
        case 2:
            return LISA_AUDIO_CH_STEREO;
        default:
            return LISA_AUDIO_CH_STEREO;
    }
}

/* 转换采样位深 */
static lisa_audio_bits_t convert_bits(uint8_t bits)
{
    switch (bits) {
        case 16:
            return LISA_AUDIO_BIT_16;
        case 24:
            return LISA_AUDIO_BIT_24;
        case 32:
            return LISA_AUDIO_BIT_32;
        default:
            return LISA_AUDIO_BIT_16;
    }
}

// 音频发送任务
static void audio_source_task(void *pvParameters)
{
    const uint8_t *audio_data = NULL;
    size_t audio_data_len = 0;
    size_t send_size = 0;

    if (g_audio_format.sample_rate == 8000) {
        audio_data = audio_pcm_8k;
        audio_data_len = audio_pcm_8k_len;
    } else {
        audio_data = audio_pcm;
        audio_data_len = audio_pcm_len;
    }
    send_size = g_audio_format.sample_rate * FRAME_TIME_MS  / 1000 * g_audio_format.channels * 
                (g_audio_format.bits_per_sample / 8);
    
    LOGI("Audio source task started");

    size_t offset = 0;
    
    while (g_audio_start) {
        if (offset + send_size > audio_data_len) {
            // 数据不足，循环播放
            offset = 0;
        }

        int ret = vintf_playback_write(&audio_data[offset], send_size);
        
        if (ret < 0) {
            LOGE("playback_write failed: %d", ret);
            break;
        }
        
        offset += send_size;
    }
    
    audio_task_handle = NULL;
    LOGI("Audio source task stopped");
    vTaskDelete(NULL);
}

static void capture_callback_handler(const void *data, size_t len, void *user_data)
{
    // LOGI("Capture callback received %zu bytes", len);
    
    uint32_t samples = len / (g_audio_format.bits_per_sample / 8);

    int ret = lisa_audio_play_write(audio_dev, data, samples);
    if (ret < 0) {
        LISA_LOGE(TAG, "lisa_audio_play_write failed: %d", ret);
    }
}

static void open_complete_handler(const bt_audio_format_t format)
{
    lisa_audio_play_config_t play_config = {
        .format = {
            .sample_rate = format.sample_rate,
            .channels = format.channels,
            .sample_bits = format.bits_per_sample,
            .sample_rate = convert_sample_rate(format.sample_rate),
            .channels = convert_channels(format.channels),
            .sample_bits = convert_bits(format.bits_per_sample),
        },
        .gain = {
            .analog_gain = -10,
            .digital_gain = -10,
        },
        .buffer_count = 3,
        .buffer_samples = format.sample_rate * 16 / 1000,
    };

    g_audio_format = format;
    
    lisa_audio_play_config(audio_dev, &play_config);
    lisa_audio_play_start(audio_dev);
    
    if (xTaskCreate(audio_source_task, "audio_src", 4096, NULL, 8, &audio_task_handle) != pdPASS) {
        LOGE("Failed to create audio source task");
        vintf_profile_close();
        return;
    }
    g_audio_start = true;
    
    LOGI("BT audio source started successfully");
}

static int cmd_bt_inquiry(void)
{
    return lisa_bluetooth_inquiry_start(GAPM_DISC_TYPE_GEN_DISC, 5);
}

static int cmd_bt_connect(int argc, char *argv[])
{
    int ret;
    
    if (argc < 2) {
        LOGE("Usage: bt_connect <device_name>");
        return -1;
    }

    const char *device_name = argv[1];
    LOGI("Connecting to device: %s", device_name);
    
     ret = lisa_bluetooth_connect_by_name(device_name);
     if (ret != 0) {
         LOGE("Failed to connect to device '%s': %d", device_name, ret);
     }
     return ret;
}

static int cmd_bt_audio_start(int argc, char *argv[])
{
    bt_audio_error_t err;
    
    if (g_audio_start) {
        LOGW("Audio already running");
        return 0;
    }

    vintf_open_info_t info;

    info.type = VINTF_PROFILE_CAPTURE;
    info.open_complete_callback = open_complete_handler;
    info.audio_data_callback = capture_callback_handler;
    info.user_data = NULL;

    err = vintf_profile_open(&info);
    if (err != BT_AUDIO_OK) {
        LOGE("vintf_profile_open failed: %d", err);
    }
    
    LOGI("Virtual interface opened successfully");

    return 0;
}

static int cmd_bt_audio_stop(int argc, char *argv[])
{
    if (!g_audio_start) {
        LOGI("Audio not running");
        return 0;
    }
    
    // 1. 停止音频任务
    g_audio_start = false;
    
    // 等待任务结束
    int timeout = 50;
    while (audio_task_handle != NULL && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    vintf_profile_close();
    
    LOGI("BT audio source stopped");
    return 0;
}

static void bt_stack_classic_discover_ind(const lisa_bt_discovery_info_t *info)
{
    if (info) {
        LOGI("Discovered device: %02X:%02X:%02X:%02X:%02X:%02X, RSSI=%d, Name=%.*s",
             info->addr.addr[0], info->addr.addr[1], info->addr.addr[2],
             info->addr.addr[3], info->addr.addr[4], info->addr.addr[5],
             info->rssi,
             info->name_len, info->name);
    }
}

int main(int argc, char **argv)
{
    int ret = 0;
    
    extern uint8_t _sshram[], _eshram[];
    memset(_sshram, 0, (_eshram - _sshram));

    atcmd_init();

    ret = lisa_shell_init();
    if (ret != 0) {
        LOGI("Failed to initialize shell (error: %d)\n", ret);
    }

    /* Initialize filesystem for BT pairing info storage */
    user_fs_init();
    lisa_kv_init();

    lisa_bluetooth_init(NULL);

    vTaskDelay(pdMS_TO_TICKS(50));
    lisa_bluetooth_register_discovery_callback(bt_stack_classic_discover_ind);

    audio_dev = lisa_device_get(LISA_AUDIO_DEVICE_NAME);

    ret = bt_vintf_init();
    if (ret != BT_AUDIO_OK) {
        LOGI("Failed to initialize virtual interface (error: %d)", ret);
    } else {
        LOGI("Virtual BT interface initialized successfully");
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}

// 蓝牙音频控制命令
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 bt_inquiry, cmd_bt_inquiry, bt inquiry audio);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 bt_connect, cmd_bt_connect, bt connect to device by name);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 bt_audio_start, cmd_bt_audio_start, start bt audio);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 bt_audio_stop, cmd_bt_audio_stop, stop bt audio);

static int at_passthrough_handler(char *data, unsigned short len)
{
    atcmd_handler(data, len);

    return 0;
}

// Register AT passthrough mode command
// Usage:
//   Interactive mode: AT (then input AT+?, no space needed)
//   Single-line mode: AT AT+? (execute at once)
SHELL_EXPORT_PASSTROUGH(SHELL_CMD_PERMISSION(0), AT, AT>>, at_passthrough_handler, AT command passthrough mode);

