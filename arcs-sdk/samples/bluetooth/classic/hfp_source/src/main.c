/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
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
#include "bt_avrcp.h"

#define WIFI_MGR_SEARCH_AP_BUFFER_SIZE 10
#define WIFI_MGR_AUTO_CONNECT_INTERVAL_MS 2000

// 音频参数配置
#define FRAME_TIME_MS  7.5
#define BT_AUDIO_STOP_TIMEOUT_MS 2000U

#define AUDIO_MODE_STR      "Encode (PCM → MSBC)"

// 音频任务相关
static TaskHandle_t audio_task_handle = NULL;
static bool g_audio_start = false;
static bt_audio_format_t g_audio_format = {0};

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

lisa_device_t *audio_dev;
#define LISA_AUDIO_DEVICE_NAME "audio0"

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
    g_audio_start = false;
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
    int ret;
    lisa_audio_play_config_t play_config = {
        .format = {
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

    if (!audio_dev) {
        LOGE("audio device is NULL");
        vintf_profile_close();
        return;
    }

    /* Ensure the DAC returns to IDLE before applying the negotiated HFP format. */
    (void)lisa_audio_play_stop(audio_dev);

    ret = lisa_audio_play_config(audio_dev, &play_config);
    if (ret != 0) {
        LOGE("lisa_audio_play_config failed: %d", ret);
        vintf_profile_close();
        return;
    }

    ret = lisa_audio_play_start(audio_dev);
    if (ret != 0) {
        LOGE("lisa_audio_play_start failed: %d", ret);
        vintf_profile_close();
        return;
    }

    g_audio_start = true;
    if (xTaskCreate(audio_source_task, "audio_src", 4096, NULL, 8, &audio_task_handle) != pdPASS) {
        g_audio_start = false;
        (void)lisa_audio_play_stop(audio_dev);
        LOGE("Failed to create audio source task");
        vintf_profile_close();
        return;
    }
    
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

    info.type = VINTF_PROFILE_CAPTURE;
    info.open_complete_callback = open_complete_handler;
    info.audio_data_callback = capture_callback_handler;
    info.user_data = NULL;

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
    bt_audio_error_t err;
    bool was_running = g_audio_start;

    (void)argc;
    (void)argv;

    if (!was_running) {
        LOGI("Audio not running, abort pending open if any");
    }
    
    // 1. 停止音频任务
    g_audio_start = false;
    
    // 等待任务结束
    int timeout = 50;
    while (audio_task_handle != NULL && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    err = was_running ? vintf_profile_close() :
                        vintf_profile_abort_open(BT_AUDIO_STOP_TIMEOUT_MS);
    if (err == BT_AUDIO_ERR_INVALID_STATE) {
        err = vintf_profile_close();
    }
    if (err != BT_AUDIO_OK) {
        LOGW("vintf_profile stop ret=%d", err);
    }

    if (audio_dev) {
        (void)lisa_audio_play_stop(audio_dev);
    }
    
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
