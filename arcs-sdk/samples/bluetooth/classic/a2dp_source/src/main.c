/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdint.h>
#include "audio_sbc.h"
#include "audio_pcm.h"

#include "FreeRTOS.h"
#include "task.h"

#include "shell.h"

#define TAG "app-bt"
#include "lisa_log.h"

#include "lisa_shell.h"
#include "shell_passthrough.h"
#include "lisa_bluetooth.h"
#include "fs/user_fs.h"
#include "bt_audio_interface_virtual.h"

/* 音频模式选择宏：
 * 0 = 透传模式（发送预编码的 SBC 数据）
 * 1 = 编码模式（发送 PCM 数据，由 session 编码为 SBC）
 */
#define USE_PCM_ENCODE_MODE  1

// 音频参数配置
#define SAMPLE_RATE         48000   // 采样率 48kHz
#define CHANNELS            2       // 立体声
#define BITS_PER_SAMPLE     16      // 16位采样

#if USE_PCM_ENCODE_MODE
/* 编码模式：发送 PCM 数据 */
#define PCM_FRAME_SIZE      (SAMPLE_RATE / 100 * CHANNELS * (BITS_PER_SAMPLE / 8))  // 10ms PCM 数据
#define AUDIO_MODE_STR      "Encode (PCM → SBC)"
#else
/* 透传模式：发送预编码的 SBC 数据 */
#define SBC_FRAME_SIZE      (43)  // SBC 帧大小(字节)
#define AUDIO_MODE_STR      "Passthrough (pre-encoded SBC)"
#endif

// 音频任务相关
static TaskHandle_t audio_task_handle = NULL;
static bool g_audio_start = false;

// 音频发送任务 - 使用 framework 架构
static void audio_source_task(void *pvParameters)
{
#if USE_PCM_ENCODE_MODE
    /* 编码模式：发送 PCM 数据 */
    extern const uint8_t audio_pcm[];
    extern const uint32_t audio_pcm_len;
    const uint8_t *audio_data = audio_pcm;
    size_t audio_data_len = audio_pcm_len;
    size_t send_size = PCM_FRAME_SIZE;
    
    LOGI("Audio source task started (PCM encode mode)");
    LOGI("PCM data size: %zu bytes, frame size: %zu bytes", audio_data_len, send_size);
#else
    /* 透传模式：发送预编码的 SBC 数据 */
    extern const uint8_t audio_sbc[];
    extern const size_t audio_sbc_len;
    const uint8_t *audio_data = audio_sbc;
    size_t audio_data_len = audio_sbc_len;
    size_t send_size = SBC_FRAME_SIZE;
    
    LOGI("Audio source task started (SBC passthrough mode)");
    LOGI("SBC data size: %zu bytes, frame size: %zu bytes", audio_data_len, send_size);
#endif
    
    size_t offset = 0;
    
    while (g_audio_start) {
        if (offset + send_size >= audio_data_len) {
            // 数据不足，循环播放
            offset = 0;
            // LOGI("Audio loop: restarting from beginning");
        }
        
        // 通过虚拟声卡接口写入音频数据
        // interface 会根据模式自动处理：
        // 1. 透传模式：直接发送 SBC 数据到蓝牙
        // 2. 编码模式：将 PCM 喂给 session 编码后发送
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

static void open_complete_handler(const bt_audio_format_t format)
{   
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
    return lisa_bluetooth_inquiry_start(GAPM_DISC_TYPE_GEN_DISC, MAX_DISCOVERED_DEVICES);
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

    info.type = VINTF_PROFILE_PLAYBACK;
    info.open_complete_callback = open_complete_handler;
    info.audio_data_callback = NULL;
    info.user_data = NULL;

#if USE_PCM_ENCODE_MODE
    err = bt_vintf_set_encode_mode(true);  // true = 编码模式（PCM → SBC）
    if (err != BT_AUDIO_OK) {
        LOGE("Failed to set encode mode: %d", err);
        return -1;
    }
    LOGI("Mode: Encode PCM to SBC");
#else
    err = bt_vintf_set_encode_mode(false);  // false = 透传模式（预编码 SBC）
    if (err != BT_AUDIO_OK) {
        LOGE("Failed to set passthrough mode: %d", err);
        return -1;
    }
    LOGI("Mode: Passthrough pre-encoded SBC");
    err = bt_vintf_set_frame_params(2667, 43);
    if (err != BT_AUDIO_OK) {
        LOGE("Failed to set frame params: %d", err);
        return -1;
    }
#endif

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

static int cmd_bt_audio_volume(int argc, char *argv[])
{
    if (argc < 2) {
        LOGE("Usage: bt_audio_volume <0-100>");
        return -1;
    }
    
    int volume = atoi(argv[1]);
    if (volume < 0 || volume > 100) {
        LOGE("Volume must be between 0 and 100");
        return -1;
    }

    bt_audio_error_t err = vintf_set_volume(volume);
    if (err != BT_AUDIO_OK) {
        LOGE("Failed to set volume: %d", err);
        return -1;
    }
    
    LOGI("Volume set to %d%%", volume);
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

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 bt_audio_volume, cmd_bt_audio_volume, set bt audio volume);

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
