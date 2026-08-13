/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
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
#include "bt_avrcp.h"


/* 音频模式选择宏：
 * 0 = 透传模式（发送预编码的 SBC 数据）
 * 1 = 编码模式（发送 PCM 数据，由 session 编码为 SBC）
 */
#define USE_PCM_ENCODE_MODE  1

// 音频参数配置
#define SAMPLE_RATE         44100   // 采样率 44.1kHz
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

static const char *avrcp_key_name(uint8_t key_id)
{
    switch (key_id) {
    case BT_AVRCP_KEY_ID_PLAY:
        return "PLAY";
    case BT_AVRCP_KEY_ID_PAUSE:
        return "PAUSE";
    case BT_AVRCP_KEY_ID_STOP:
        return "STOP";
    case BT_AVRCP_KEY_ID_FORWARD:
        return "FORWARD";
    case BT_AVRCP_KEY_ID_BACKWARD:
        return "BACKWARD";
    case BT_AVRCP_KEY_ID_VOLUME_UP:
        return "VOLUME_UP";
    case BT_AVRCP_KEY_ID_VOLUME_DOWN:
        return "VOLUME_DOWN";
    case BT_AVRCP_KEY_ID_MUTE:
        return "MUTE";
    default:
        return "UNKNOWN";
    }
}

static void bt_avrcp_key_cb(uint8_t conidx, uint8_t key_id)
{
    LOGI("AVRCP key: conidx=%u key_id=0x%02x (%s)",
         conidx, key_id, avrcp_key_name(key_id));
}

static int parse_bt_addr(const char *str, gap_bdaddr_t *addr)
{
    char *end = NULL;
    unsigned long value;

    if (!str || !addr) {
        return -1;
    }

    memset(addr, 0, sizeof(*addr));
    addr->addr_type = 0;

    for (int i = 0; i < 6; i++) {
        value = strtoul(str, &end, 16);
        if ((end == str) || (value > 0xFF)) {
            return -1;
        }

        addr->addr[i] = (uint8_t)value;
        if (i < 5) {
            if (*end != ':') {
                return -1;
            }
            str = end + 1;
        } else if (*end != '\0') {
            return -1;
        }
    }

    return 0;
}


static const char *bt_paired_transport_name(uint8_t transport)
{
    switch (transport) {
    case BT_PAIRED_TRANSPORT_BLE:
        return "BLE";
    case BT_PAIRED_TRANSPORT_CLASSIC:
        return "Classic";
    default:
        return "Unknown";
    }
}

static void bt_print_addr(const gap_bdaddr_t *addr)
{
    LOGI("%02X:%02X:%02X:%02X:%02X:%02X type=%u",
         addr->addr[0], addr->addr[1], addr->addr[2],
         addr->addr[3], addr->addr[4], addr->addr[5],
         addr->addr_type);
}

static int cmd_bt_paired_list(int argc, char *argv[])
{
    bt_paired_info_t list[BT_PAIRED_MAX_COUNT];
    uint8_t count = 0;
    int ret;

    (void)argc;
    (void)argv;

    ret = bt_paired_list_get(list, BT_PAIRED_MAX_COUNT, &count);
    if (ret != 0 && ret != -ENOSPC) {
        LOGE("bt_paired_list_get failed: %d", ret);
        return ret;
    }

    LOGI("Paired device count: %u", count);
    for (uint8_t i = 0; i < count && i < BT_PAIRED_MAX_COUNT; i++) {
        LOGI("paired[%u]: transport=%s name=%.*s", i,
             bt_paired_transport_name(list[i].transport),
             list[i].name_len, (const char *)list[i].name);
        bt_print_addr(&list[i].addr);
    }

    if (ret == -ENOSPC) {
        LOGW("Paired list truncated to %u entries", BT_PAIRED_MAX_COUNT);
    }

    return ret;
}

static int cmd_bt_paired_name(int argc, char *argv[])
{
    gap_bdaddr_t addr;
    char name[BT_PAIRED_NAME_MAX_LEN + 1];
    int ret;

    if (argc < 2) {
        LOGE("Usage: bt_paired_name <XX:XX:XX:XX:XX:XX>");
        return -1;
    }

    if (parse_bt_addr(argv[1], &addr) != 0) {
        LOGE("Invalid bluetooth address: %s", argv[1]);
        return -1;
    }

    ret = bt_paired_name_get(&addr, name, sizeof(name));
    if (ret != 0) {
        LOGE("bt_paired_name_get failed: %d", ret);
        return ret;
    }

    LOGI("Paired device name: %s", name);
    return 0;
}

static int cmd_bt_paired_remove(int argc, char *argv[])
{
    gap_bdaddr_t addr;
    int ret;

    if (argc < 2 || strcmp(argv[1], "all") == 0) {
        ret = bt_paired_remove(NULL);
        LOGI("Remove all paired devices ret=%d", ret);
        return ret;
    }

    if (parse_bt_addr(argv[1], &addr) != 0) {
        LOGE("Invalid bluetooth address: %s", argv[1]);
        LOGE("Usage: bt_paired_remove [all|XX:XX:XX:XX:XX:XX]");
        return -1;
    }

    ret = bt_paired_remove(&addr);
    LOGI("Remove paired device %s ret=%d", argv[1], ret);
    return ret;
}

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
    g_audio_start = false;
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

static int cmd_bt_open(int argc, char *argv[])
{
    int ret;

    (void)argc;
    (void)argv;

    ret = lisa_bluetooth_open();
    LOGI("bt_open ret=%d opened=%d", ret, lisa_bluetooth_is_opened());
    return ret;
}

static int cmd_bt_status(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    LOGI("Bluetooth opened: %s", lisa_bluetooth_is_opened() ? "yes" : "no");
    LOGI("Audio running: %s", g_audio_start ? "yes" : "no");
    return 0;
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

static int cmd_bt_connect_by_index(int argc, char *argv[])
{
    int ret;
    
    if (argc < 2) {
        LOGE("Usage: bt_connect_index <device_index>");
        return -1;
    }

    uint8_t device_index = atoi(argv[1]);
    LOGI("Connecting to device at index: %d", device_index);
    
     ret = lisa_bluetooth_connect_by_index(device_index);
     if (ret != 0) {
         LOGE("Failed to connect to device at index %d: %d", device_index, ret);
     }
     return ret;
}

static int cmd_bt_connect_by_addr(int argc, char *argv[])
{
    gap_bdaddr_t addr;
    int ret;

    if (argc < 2) {
        LOGE("Usage: bt_connect_addr <XX:XX:XX:XX:XX:XX>");
        return -1;
    }

    if (parse_bt_addr(argv[1], &addr) != 0) {
        LOGE("Invalid bluetooth address: %s", argv[1]);
        LOGE("Usage: bt_connect_addr <XX:XX:XX:XX:XX:XX>");
        return -1;
    }

    ret = lisa_bluetooth_connect_by_addr(&addr);
    if (ret != 0) {
        LOGE("Failed to connect to device addr '%s': %d", argv[1], ret);
    }
    return ret;
}

static int cmd_bt_disconnect(int argc, char *argv[])
{
    int ret;

    if (argc < 2) {
        LOGE("Usage: bt_disconnect <device_name>");
        return -1;
    }

    ret = lisa_bluetooth_disconnect_by_name(argv[1]);
    if (ret != 0) {
        LOGE("Failed to disconnect device '%s': %d", argv[1], ret);
    }
    return ret;
}

static int cmd_bt_disconnect_by_index(int argc, char *argv[])
{
    uint8_t device_index;
    int ret;

    if (argc < 2) {
        LOGE("Usage: bt_disconnect_index <device_index>");
        return -1;
    }

    device_index = (uint8_t)atoi(argv[1]);
    ret = lisa_bluetooth_disconnect_by_index(device_index);
    if (ret != 0) {
        LOGE("Failed to disconnect device at index %d: %d", device_index, ret);
    }
    return ret;
}

static int cmd_bt_disconnect_by_addr(int argc, char *argv[])
{
    gap_bdaddr_t addr;
    int ret;

    if (argc < 2) {
        LOGE("Usage: bt_disconnect_addr <XX:XX:XX:XX:XX:XX>");
        return -1;
    }

    if (parse_bt_addr(argv[1], &addr) != 0) {
        LOGE("Invalid bluetooth address: %s", argv[1]);
        LOGE("Usage: bt_disconnect_addr <XX:XX:XX:XX:XX:XX>");
        return -1;
    }

    ret = lisa_bluetooth_disconnect_by_addr(&addr);
    if (ret != 0) {
        LOGE("Failed to disconnect device addr '%s': %d", argv[1], ret);
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
        return -1;
    }
    
    LOGI("Virtual interface open requested");

    return 0;
}

static int cmd_bt_audio_stop(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

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

static int cmd_bt_close(int argc, char *argv[])
{
    int ret;

    (void)argc;
    (void)argv;

    (void)cmd_bt_audio_stop(0, NULL);
    ret = lisa_bluetooth_close();
    LOGI("bt_close ret=%d opened=%d", ret, lisa_bluetooth_is_opened());
    return ret;
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
    lisa_bt_classic_register_avrcp_cb(bt_avrcp_key_cb);
    
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
                 bt_open, cmd_bt_open, open bluetooth stack);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_close, cmd_bt_close, close bluetooth stack);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_status, cmd_bt_status, show bluetooth status);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 bt_inquiry, cmd_bt_inquiry, bt inquiry audio);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 bt_connect, cmd_bt_connect, bt connect to device by name);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 bt_connect_index, cmd_bt_connect_by_index, bt connect to device by index);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_connect_addr, cmd_bt_connect_by_addr, bt connect to device by address);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_disconnect, cmd_bt_disconnect, bt disconnect device by name);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_disconnect_index, cmd_bt_disconnect_by_index, bt disconnect device by index);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_disconnect_addr, cmd_bt_disconnect_by_addr, bt disconnect device by address);

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

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_paired_list, cmd_bt_paired_list, list paired bt devices);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_paired_name, cmd_bt_paired_name, get paired bt device name by address);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_paired_remove, cmd_bt_paired_remove, remove paired bt device by address or all);

SHELL_EXPORT_PASSTROUGH(SHELL_CMD_PERMISSION(0), AT, AT>>, at_passthrough_handler, AT command passthrough mode);
