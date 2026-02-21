# 应用启动流程分析 (src/main.c)

## 概述

本文档详细描述 `src/main.c` 的启动流程和主要功能模块。该文件是 AP 核心应用的主入口点，负责系统初始化、事件处理、网络配置等核心功能。

---

## 目录

- [1. 入口函数 main()](#1-入口函数-main)
- [2. 应用任务 app_task()](#2-应用任务-app_task)
- [3. 电源管理](#3-电源管理)
- [4. WiFi/BLE 配置](#4-wifible-ble配置)
- [5. 按键处理](#5-按键处理)
- [6. NVS 初始化](#6-nvs-初始化)
- [7. 助手控制器系统](#7-助手控制器系统)
- [8. 音频服务初始化](#8-音频服务初始化)

---

## 1. 入口函数 main()

**位置**: `src/main.c` 行 555-600

### 1.1 硬件初始化

```c
// 关闭看门狗
WDT_RegDef *wdt_hw = (WDT_RegDef *)IP_AP_WDT;
wdt_hw->REG_WREN.all = 0x5AA5;
wdt_hw->REG_CTRL.all = 0;

// 打印版本信息
printf("\r\n");
printf("project commit: %s\r\n", PROJECT_VERSION_COMMIT);
printf("project version: %s\r\n", PROJECT_VERSION_STR);
printf("\r\n");

// GPIO 初始化
GPIO_Initialize(GPIOB(), NULL, NULL);
usb_plug_detect_gpio_init();

// 电源管理初始化
power_config_t power_cfg = {
    .on_shutdown = shutdown,  // 注册关机回调
};
power_init(&power_cfg);
```

### 1.2 启动流程

```
main()
  ↓
[1] 等待电源按键长按 3 秒
  ├─ 如果按键提前释放 → 关机
  └─ 长按 3 秒 → 继续启动
  
[2] LED 闪烁提示 (50ms 开/50ms 关)
  
  app_led_blink(50, 50);

[3] 初始化外部 RAM 和 cJSON
  ├─ heap_caps_malloc_extmem_enable(16);  // 启用外部 RAM
  └─ cJSON_InitHooks(&cjson_hooks);        // cJSON 使用外部 RAM

[4] 解析配置文件
  config_init((const char *)0x300f0000);  // 从 Flash 0x300f0000 读取
  config_print(config);
  printf("config parse elapsed time: %d ms\n", time);

[5] 创建应用任务
  lisa_thread_create(&att, app_task, NULL);
```

### 1.3 关键配置

| 参数 | 值 | 说明 |
|------|-----|------|
| APP_TASK_STACK | 8192 bytes | 应用任务栈大小 |
| APP_TASK_PRIO | LISA_OS_PRIORITY_NORMAL | 应用任务优先级 |
| ARCS_DAC_USE_LITE_DAC | 1 | 使用 Lite DAC |
| BLE_ADV_START_MAX_RETRIES | 5 | BLE 广播最大重试次数 |
| BLE_ADV_START_RETRY_DELAY_MS | 100 | BLE 广播重试延迟 (ms) |
| POWER_BUTTON_HOLD_TIME_MS | 3000 | 电源按键长按时间 (3秒) |
| POWER_SAMPLE_INTERVAL_MS | 50 | 电源按键采样间隔 (50ms) |

---

## 2. 应用任务 app_task()

**位置**: `src/main.c` 行 357-538

### 2.1 完整初始化流程

```
app_task()
  ↓
[1] 核间通信初始化
  ├─ ipc_mem_init(1)
  └─ ipc_master_init(NULL)
  ↓
[2] 系统初始化
  ├─ ls_sys_init(TIMEZONE_SHANGHAI)
  ├─ 时区、RTC、系统服务
  └─ LISA_LOGI(TAG, "system init end")
  ↓
[3] Shell 初始化
  ├─ lisa_shell_init()
  ├─ 暂停系统默认日志输出
  └─ 重定向到 shell 输出
  ↓
[4] IC 消息初始化
  ├─ ic_message_init()
  └─ 建立 AP-CP 核心通信通道
  ↓
[5] 文件系统初始化
  ├─ #ifdef CONFIG_LISA_KV_TYPE_LSFS
  ├─   user_fs_init()
  └─ #endif
  ↓
[6] KV 存储初始化
  lisa_kv_init()
  ↓
[7] EVS 工具初始化
  ├─ evs_utils_init()
  └─ 事件系统工具
  ↓
[8] 设备 ID 更新
  update_device_id()
  ↓
[9] USB 启动
  user_usb_start()
  ↓
[10] USB-MSC 模式检查
  if (app_usb_msc_enabled()) {
     while(1) {
         lisa_thread_delay(1000);
     }
 }
  ↓
[11] Flash/配置解析
  config_init((const char *)0x300f0000)
  printf("config parse elapsed time: %d ms\n", time);
 config_print(config);
```

### 2.2 核心模块初始化

#### 2.2.1 NVS 初始化
```c
arcs_nvs_init()
  ├─ NVDS_FLASH_ADDRESS_OFFSET (0xFF8000)  // Flash 最后 32KB
  ├─ NVDS_FLASH_SIZE (0x8000)      // 32KB
  ├─ flash_if_init(&arcs_flash_dev, 0, 0)
  ├─ flash_get_page_info_by_offs(&arcs_flash_dev, arcs_nvs_fs.offset, &info)
  ├─ arcs_nvs_fs.sector_size = info.size
  └─ arcs_nvs_fs.sector_count = NVDS_FLASH_SIZE / info.size
```

#### 2.2.2 WiFi 预初始化
```c
ls_wifi_pre_init(_main_wifi_stack_init_done_cb);
LISA_LOGI(TAG, "BLE init start\n");
ls_ble_init();
LISA_LOGI(TAG, "BLE init end\n");
comm_service_init();
LISA_LOGI(TAG, "comm service init end");
```

#### 2.2.3 音频服务初始化
```c
// LED 服务
app_led_init();
app_led_off();

// 音频播放服务
extern void audio_play_service_start();
audio_play_service_start();
LISA_LOGI(TAG, "audio play service start");

// 音频录制服务
extern void audio_record_service_start();
audio_record_service_start();
LISA_LOGI(TAG, "audio record service start");
```

#### 2.2.4 PA (功率放大器) 初始化
```c
#if !defined(ARCS_DAC_USE_LITE_DAC)
    // PA init
    pa_manager_pre_init();
    pa_manager_init(PA_MGR_NONE);
    LISA_LOGI(TAG, "PA init end");
    
    // Speaker init
    listen_spk_init();
    LISA_LOGI(TAG, "speaker init end");
#endif

// PA 初始化（重复，行 468-470）
pa_manager_pre_init();
pa_manager_init(PA_MGR_NONE);
LISA_LOGI(TAG, "PA init end");
```

#### 2.2.5 Tone 和 Player 初始化
```c
// Tone init
app_tone_default_init();
LISA_LOGI(TAG, "tone init end");

// LisaPlayer init
app_player_init();
LISA_LOGI(TAG, "app_player init end");
```

#### 2.2.6 主应用启动
```c
// Welcome
extern void app_main(void);
app_main();
LISA_LOGI(TAG, "Application Ready!!!");
```

#### 2.2.7 按键初始化
```c
#if (CONFIG_FLEXIBLE_BUTTON)
    lisa_btn_init(button_callback_handle, NULL);
#endif
```

#### 2.2.8 其他初始化
```c
#if (CONFIG_BATTERY_COLLECTION)
    battery_adc_sample_init();
#endif
listen_mic_gain_init();
assistant_view_userdata_load();
video_camera_init();
camera_senor_release();
assist_controller_init();
```

---

## 3. 电源管理

### 3.1 电源引脚配置

| 引脚 | 功能 | 说明 |
|------|------|------|
| PB4 | 电源按键 | 输入，上拉，按下为低电平 |
| PB3 | 电源锁存 | 输出，高电平保持供电 |

**文件位置**:
- `src/power/power_manager.h` - 配置定义
- `src/power/power_manager.c` - 实现

### 3.2 开机流程

```
main()
  power_init(&power_cfg)              // 初始化电源管理，注册 shutdown 回调
      ↓
  power_wait_settle()              // 等待按键长按 3 秒
      ├─ USB 已连接？
      │   ├─ YES → power_latch_set(true);    → 直接开机
      │   └─ NO  ── 检测按键是否长按 3 秒
      │           ├─ 提前释放 → 关机
      │           └─ 长按 3 秒 → 继续开机
      └─ power_latch_set(true); → 设置电源锁存为高电平，保持供电
```

### 3.3 关机流程

```
shutdown()
  ├─ pa_manager_onoff(0);                  // 关闭 PA
  ├─ lisa_kv_poweroff_save();           // 保存 KV 数据
  ├─ app_led_off();                   // 关闭 LED
  ├─ lisa_display_blanking_on();      // 开启显示息屏
  └─ lisa_display_set_brightness(0); // 关闭背光
```

### 3.4 USB 优先开机机制

```
power_wait_settle()
  if (get_usb_status() == USB_STATUS_PLUG) {
    power_latch_set(true);  // 直接开机
    return true;
  }
  // 继续检查按键
  // 长按检测...
```

---

## 4. WiFi/BLE 配置

### 4.1 WiFi 配置存储格式

**存储位置**: NVS (Flash 最后 32KB)

**存储结构** (`arcs-sdk/modules/wifi_manager/include/wifi_manager/wifi_manager_wifi_ops.h`):
```c
typedef struct {
    char ssid[32];                      // WiFi 名称
    char pwd[64];                       // WiFi 密码
    char bssid[18];                     // BSSID (XX:XX:XX:XX:XX:XX格式)
    int channel;                        // 频道
    int rssi;                           // 信号强度
    wifi_mgr_wifi_encryption_mode_t encryption_mode; // 加密方式
} wifi_mgr_sta_config_t;
```

**存储 Keys**:
- `wifi-info-count` - 保存的 WiFi 数量
- `wifi-list` - WiFi 配置列表 (wifi_storage_item_t 数组)

### 4.2 WiFi 配置检查流程

**代码位置**: `src/main.c` 行 501-535

```c
// 1. 查询保存的 WiFi 网络
wifi_mgr_sta_config_t *matched_list = NULL;
ret = wifi_mgr_storage_search_ap(&matched_list, SEARCH_ALL, NULL);

if (ret > 0 && matched_list != NULL) {
    // 有保存的 WiFi
    printf("Found %d saved WiFi network(s)\n", ret);
    
    // 打印找到的 WiFi 配置（用于调试）
    for (int i = 0; i < ret; i++) {
        printf("WiFi config [%d]: SSID=[%s], BSSID=[%02x:%02x:%02x:%02x]\n", i + 1, 
               matched_list[i].ssid,
               matched_list[i].bssid[0], matched_list[i].bssid[1], 
               matched_list[i].bssid[2], matched_list[i].bssid[3],
               matched_list[i].bssid[4], matched_list[i].bssid[5]);
    }
    
    has_wifi_config = true;
    
    // Free the matched list
    free(matched_list);
} else if (ret == 0) {
    // 无保存的 WiFi
    printf("No saved WiFi networks found\n");
} else {
    printf("Failed to get saved WiFi networks, error: %d\n", ret);
}

// 2. 根据是否有 WiFi 配置决定启动模式
if (!has_wifi_config) {
    printf("No valid WiFi configuration found\n");
    change_info_page(LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_NETWORK);
    enter_ble_config(true);  // 进入 BLE 配置模式，播放提示音
} else {
    // 有 WiFi 配置，正常启动
    ;  // 等待 WiFi 自动连接
}
```

### 4.3 BLE 配置模式 (`enter_ble_config()`)

**代码位置**: `src/main.c` 行 189-230

```c
void enter_ble_config(bool play_audio)
{
    LISA_LOGI(TAG, "=== Entering BLE Config Mode ===");
    
    // 1. 如果需要，播放提示音
    if (play_audio) {
        LISA_LOGI(TAG, "Triggering CONTROLLER_EVENT_OPT_ENTER_BLE_CONFIG");
        assist_controller_trigger_event(CONTROLLER_EVENT_OPT_ENTER_BLE_CONFIG, NULL, 0);
    }
    
    // 2. 启动 BLE 广播（最多重试 5 次）
    LISA_LOGI(TAG, "Starting BLE advertising with max %d retries", BLE_ADV_START_MAX_RETRIES);
    
    int ble_ret = -1;
    for (int retry = 0; retry < BLE_ADV_START_MAX_RETRIES; retry++) {
        LISA_LOGI(TAG, "BLE advertising attempt %d/%d (address 0, mode BLE_ADV_GEN)", 
                  retry + 1, BLE_ADV_START_MAX_RETRIES);
        
        ble_ret = app_ble_adv_start(0, BLE_ADV_GEN);
        
        if (ble_ret == pdTRUE) {
            LISA_LOGI(TAG, "BLE advertising started successfully on attempt %d/%d", 
                      retry + 1, BLE_ADV_START_MAX_RETRIES);
            break;
        } else {
            if (retry < BLE_ADV_START_MAX_RETRIES - 1) {
                LISA_LOGW(TAG, "BLE advertising start failed on attempt %d/%d (ret: %d), retrying...", 
                          retry + 1, BLE_ADV_START_MAX_RETRIES, ble_ret);
                lisa_thread_delay(BLE_ADV_START_RETRY_DELAY_MS);
            } else {
                LISA_LOGE(TAG, "BLE advertising start failed on all %d attempts (final ret: %d)", 
                          BLE_ADV_START_MAX_RETRIES, ble_ret);
            }
        }
    }
    
    // 3. 检查 BLE 广播是否成功
    if (ble_ret == 0) {
        LISA_LOGI(TAG, "=== BLE Config Mode Ready ===");
    } else {
        LISA_LOGE(TAG, "=== BLE Config Mode Failed to Start ===");
    }
}
```

### 4.4 网络重置功能

**4.4.1 Factory Reset (出厂设置)**

**代码位置**: `src/main.c` 行 70-98

```c
static void factory_reset(void)
{
    printf("\n=== Starting Factory Reset ===\n");
    
    // 1. 清除 WiFi 配置
    printf("Clearing WiFi configurations...\n");
    int ret1 = lisa_kv_del("wifi-info-count");
    int ret2 = lisa_kv_del("wifi-list");
    
    // 2. 删除用户凭证
    printf("Deleting user credentials...\n");
    int ret3 = lisa_kv_del("user.pid");      // 用户 ID
    int ret4 = lisa_kv_del("user.sid");      // 会话 ID
    int ret5 = lisa_kv_del("user.volume");    // 音量
    int ret6 = lisa_kv_del("user.mic_gain");  // 麦克风增益
    int ret7 = lisa_kv_del("user.aec_gain");  // AEC 增益
    
    printf("=== Factory Reset Completed ===\n\n");
}
```

**4.4.2 Network Reset (网络重置)**

**代码位置**: `src/main.c` 行 100-112)

```c
static void network_reset(void)
{
    printf("\n=== Starting Network Reset ===\n");
    
    // 清除 WiFi 配置
    printf("Clearing WiFi configurations...\n");
    lisa_kv_del("wifi-info-count");
    lisa_kv_del("wifi-list");
    
    printf("=== Network Reset Completed ===\n\n");
}
```

**区别**:
- **Factory Reset**: 清除所有用户数据（WiFi + 用户凭证）
- **Network Reset**: 仅清除 WiFi 配置，保留用户凭证

---

## 5. 按键处理

### 5.1 按键事件类型

| 事件 | 触发条件 | 功能描述 |
|------|---------|-----------|
| LISA_BTN_PRESS_CLICK | 单击 | 播放随机音乐 |
| LISA_BTN_PRESS_DOUBLE_CLICK | 双击 | 停止音乐播放 |
| LISA_BTN_PRESS_TRIPLE_CLICK | 三击 | 切换信息页面 |
| LISA_BTN_PRESS_QUINTUPLE_CLICK | 五击 | 无操作 |
| LISA_BTN_PRESS_REPEAT_CLICK | 连续点击 | 同上 |
| LISA_BTN_PRESS_LONG_HOLD | 长按 | 关机（需检查 USB） |

### 5.2 按键处理流程

**代码位置**: `src/main.c` 行 248-326

```c
static void button_callback_handle(lisa_btn_event_t event, const lisa_btn_info_t *info)
{
    LISA_LOGI(TAG, "Button %d event: %d", info->id, event);
    
    // 只处理 Power 按键
    if (info->id != LISA_BTN_ID_POWER) {
        return;
    }
    
    switch (event) {
        case LISA_BTN_PRESS_CLICK:
            // 单击: 播放随机音乐
            LISA_LOGI(TAG, "Single click: playing random music");
            
            // 创建任务播放随机音乐
            lisa_thread_attr_t attr = {
                .name = "rand_music",
                .stack_size = 4096,
                .priority = LISA_OS_PRIORITY_NORMAL
            };
            lisa_thread_create(&attr, play_random_music_task, NULL);
            alarm_ring_stop();  // 停止闹钟
            break;
        
        case LISA_BTN_PRESS_DOUBLE_CLICK:
            // 双击: 停止音乐播放
            LISA_LOGI(TAG, "power button double click, stop music playback");
            audio_player_stop_by_user();
            break;
        
        case LISA_BTN_PRESS_TRIPLE_CLICK:
            // 三击: 调出小程序配置页
            LISA_LOGI(TAG, "power button triple click, toggle info page");
            assist_controller_trigger_event(CONTROLLER_EVENT_OPT_TOGGLE_INFO_PAGE, NULL, 0);
            break;
        
        case LISA_BTN_PRESS_QUADRUPLE_CLICK:
        case LISA_BTN_PRESS_QUINTUPLE_CLICK:
        case LISA_BTN_PRESS_REPEAT_CLICK:
            // 连续点击处理（相同于五击）
            break;
        
        case LISA_BTN_PRESS_LONG_HOLD:
            // 长按：关机
            if (get_usb_status() == USB_STATUS_PLUG) {
                LISA_LOGI(TAG, "USB connected, ignore long press shutdown");
            } else {
                LISA_LOGI(TAG, "power button long press hold, shutting down...");
                power_shutdown();
            }
            break;
        
        default:
            break;
    }
}
```

### 5.3 随机音乐播放任务

**代码位置**: `src/main.c` 行 232-245`

```c
static void play_random_music_task(void *arg)
{
    LISA_LOGI(TAG, "play_random_music_task started");
    
    char song_name[128] = {0};
    int ret = music_manager_fetch_and_play_random(song_name, sizeof(song_name));
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to play random music");
    } else {
        LISA_LOGI(TAG, "Playing random music: %s", song_name);
    }
    
    vTaskDelete(NULL);  // 删除任务
}
```

### 5.4 按键操作总结

| 操作 | 点击次数 | 功能 |
|------|---------|-----------|
| 单击 | 1 | 播放随机音乐 + 停止闹钟 |
| 双击 | 2 | 停止音乐播放 |
| 三击 | 3 | 切换信息页面 |
| 四击 | 4 | 无操作 |
| 五击 | 5 | 网络重置 + BLE 配置 |
| 八击 | 8+ | 恢复出厂设置 + BLE 配置 |
| 长按 | USB 未连接则关机，USB 连接则忽略 |

---

## 6. 助手控制器系统

### 6.1 事件定义 (`src/controller/assistant_controller.h`)

```c
typedef enum {
    // 音频状态事件
    CONTROLLER_EVENT_STATE_AUDIO_IDLE = 0,
    CONTROLLER_EVENT_STATE_AUDIO_PRE_IDLE,
    CONTROLLER_EVENT_STATE_AUDIO_WAKEUP,
    CONTROLLER_EVENT_STATE_AUDIO_CONNECT_CLOUD_FAILED,
    CONTROLLER_EVENT_STATE_AUDIO_RECORD_START,
    CONTROLLER_EVENT_STATE_AUDIO_RECORD_STOP,
    CONTROLLER_EVENT_STATE_VAD_END,
    CONTROLLER_EVENT_STATE_SESSION_END, 
    CONTROLLER_EVENT_STATE_AUDIO_PLAY_START,
    CONTROLLER_EVENT_STATE_AUDIO_PLAY_STOP,
    CONTROLLER_EVENT_STATE_AUDIO_UPDATE_MUSIC,
    
    // 云端事件
    CONTROLLER_EVENT_STATE_CLOUD_UPDATE_IAT_TEXT,
    CONTROLLER_EVENT_STATE_CLOUD_GET_ROLES_SUCCESS,
    CONTROLLER_EVENT_STATE_CLOUD_GET_ROLES_FAILED,
    
    // WiFi 事件
    CONTROLLER_EVENT_STATE_WIFI_CONNECTED,
    CONTROLLER_EVENT_STATE_WIFI_DISCONNECTED,
    CONTROLLER_EVENT_STATE_WIFI_CONNECTING,
    CONTROLLER_EVENT_STATE_WIFI_SCANNING,
    CONTROLLER_EVENT_STATE_WIFI_SCANNED,
    
    // UI 更新事件
    CONTROLLER_EVENT_STATE_ROLE_EMOJI_UPDATE,
    CONTROLLER_EVENT_STATE_MCP_EMOJI_UPDATE,
    CONTROLLER_EVENT_STATE_REPLY_TEXT_UPDATE,
    CONTROLLER_EVENT_STATE_STANDBY_TEXTS_UPDATE,
    CONTROLLER_EVENT_STATE_DEVICE_CONFIG_UPDATE,
    CONTROLLER_EVENT_STATE_OUTOF_LIMIT_ERROR,
    CONTROLLER_EVENT_BATTERY_INFO_UPDATE,
    
    // 操作事件
    CONTROLLER_EVENT_OPT_WIFI_CONNECT,
    CONTROLLER_EVENT_OPT_WIFI_DISCONNECT,
    CONTROLLER_EVENT_OPT_WIFI_SCAN,
    CONTROLLER_EVENT_OPT_TOGGLE_INFO_PAGE,
    CONTROLLER_EVENT_OPT_ENTER_BLE_CONFIG,
    CONTROLLER_EVENT_OPT_EXIT_BLE_CONFIG,
    CONTROLLER_EVENT_OTA_STATE_UPDATE,
    CONTROLLER_EVENT_WAKE_WORD_UPDATE,
    
    CONTROLLER_EVENT_ALARM_ADD_ITEM,
    CONTROLLER_EVENT_ALARM_DELETE_ITEM,
    CONTROLLER_EVENT_WEATHER_UPDATE,
    
    CONTROLLER_EVENT_TTS_PLAY_START,
    CONTROLLER_EVENT_TTS_PLAY_FINISH,
    CONTROLLER_EVENT_SOUND_PLAY_START,
    CONTROLLER_EVENT_SOUND_PLAY_FINISH,
    
    CONTROLLER_EVENT_CLOUD_GET_ROLES_SUCCESS,
    CONTROLLER_EVENT_CLOUD_GET_ROLES_FAILED,
    
    CONTROLLER_EVENT_MAX_NUMBER,
} assistant_controller_event_id_e;
```

### 6.2 控制器初始化 (`src/controller/assistant_controller.c`)

```c
int assist_controller_init(void)
{
    // 1. 检查事件处理器数量
    if (sizeof(s_ctrl_event_handlers) / sizeof(s_ctrl_event_handlers[0]) != CONTROLLER_EVENT_MAX_NUMBER) {
        LISA_LOGE(TAG, "The number of s_ctrl_event_handlers must be equal to CONTROLLER_EVENT_MAX_NUMBER.");
        return -ENOENT;
    }
    
    // 2. 分配控制器结构
    assist_controller_t *controller = (assist_controller_t *)exram_malloc(4, sizeof(assist_controller_t));
    if (!controller) {
        return -ENOMEM;
    }
    
    memset(controller, 0, sizeof(assist_controller_t));
    controller->status.wifi_scan_retry_count = 0;
    
    // 3. 初始化视图
    controller->view = assistant_view_init(&s_view_cbs);
    if (!controller->view) {
        return -ENOMEM;
    }
    
    // 4. 创建消息队列
    frontend_ctrl_queue = xQueueCreate(FRONTEND_CTRL_QUEUE_NUMBER, sizeof(ctr_event_message_t));
    backend_ctrl_queue = xQueueCreate(BACKEND_CTRL_QUEUE_NUMBER, sizeof(ctr_event_message_t));
    
    // 5. 创建前端和后端任务
    xTaskCreate(frontend_task, "ctr_frontend_task", FRONTEND_TASK_TACK_SIZE, NULL, FRONTEND_TASK_PRIORITY, NULL);
    xTaskCreate(backend_task, "ctr_backend_task", BACKEND_TASK_TACK_SIZE, NULL, BACKEND_TASK_PRIORITY, NULL);
    
    // 6. 创建前端和后端任务
    xTaskCreate(frontend_task, "ctr_frontend_task", FRONTEND_TASK_TACK_SIZE, NULL, FRONTEND_TASK_PRIORITY, NULL);
    xTaskCreate(backend_task, "ctr_backend_task", BACKEND_TASK_TACK_SIZE, NULL, BACKEND_TASK_PRIORITY, NULL);
    
    // 6. 视图设置
    if (controller->view->ops.setup) {
        controller->view->ops.setup();
    }
    
    assist_controller = controller;
    
    return 0;
}
```

### 6.3 事件触发流程

```
assist_controller_trigger_event(event_id, arg, len)
  ├─ 同步处理（如果有同步处理函数）
  │   └─ 调用同步函数
    
  └─ 异步处理
      ├─ 如果有前端处理函数 → 发送到前端队列
      └─ 如果有后端处理函数 → 发送到后端队列
```

### 6.4 前端/后端任务

**前端任务**: 处理 UI 更新（界面、LED、显示等）
**后端任务**: 处理业务逻辑（音频、云端交互、WiFi 等）

---

## 7. 音频服务初始化

### 7.1 音频录制服务 (`src/audio/audio_record.c`)

```c
void audio_record_service_start()
{
    // 1. 同步远程共享
    AadcService_remote_sync();
    
    // 2. 创建录制线程（8KB 栈，高优先级）
    lisa_thread_attr_t attr = {
        .stack_size = 8 * 1024,
        .priority = LISA_OS_PRIORITY_ABOVE_NORMAL,
        .name = (uint8_t *)"recorder",
    };
    
    lisa_thread_create(&attr, audio_recorder_thread, NULL);
}
```

**数据流**:
```
CP 核心采集 (AADC)
  ↓
核间通信 (ICStream，共享内存环缓冲)
  ↓
AP 核心音频接收线程
  ├─ 获取帧数据（2560 bytes，5 通道 PCM）
  └─ handle_algo_record(2560)  [弱函数回调]
       ├─ 并行：发送到 USB Audio 输出（2560 bytes，5 通道）
       ├─ 并行：发送到云端（单通道 512 bytes）
       └─ 并行：本地录制（可选，原始 5 通道 2560 bytes）
```

### 7.2 音频播放服务 (`src/audio/audio_play.c`)

```c
void audio_play_service_start()
{
    // 建立远程共享
    PlayService_remote_sync();
}
```

**数据流**:
```
AP 核心 (AADC) → ICStream → AP 核心 → USB Audio 输出
├─ 2560 bytes/帧，5 通道
```

### 7.3 LisaPlayer 初始化

**位置**: `src/audio/app_player.c`

```c
void app_player_init(void)
{
    // 初始化音頻和音乐播放器
    //  - tone_player: 提示音播放器
    // - audio_player: 音频/音乐播放器
}
```

---

## 8. 重要配置参数

### 8.1 内存配置

| 组件 | 配置 | 说明 |
|------|------|------|
| APP_TASK_STACK | 8192 bytes | 应用任务栈大小 |
| FRONTEND_TASK_TACK_SIZE | 1024 bytes | 前端任务栈大小 |
| BACKEND_TASK_TACK_SIZE | 2048 bytes | 后端任务栈大小 |
| FRONTEND_TASK_PRIORITY | 6 | 前端任务优先级 |
| BACKEND_TASK_PRIORITY | 6 | 后端任务优先级 |

### 8.2 BLE 配置

| 参数 | 值 | 说明 |
|------|-----|------|
| BLE_ADV_START_MAX_RETRIES | 5 | BLE 广播最大重试次数 |
| BLE_ADV_START_RETRY_DELAY_MS | 100 | BLE 广播重试延迟 (ms) |

### 8.3 WiFi 配置

| 参数 | 值 | 说明 |
|------|-----|------|
| ARCS_DAC_USE_LITE_DAC | 1 | 是否使用 Lite DAC |
| CONFIG_LISA_KV_POWEROFF_SAVE | 未定义 | KV 关机时是否保存数据 |
| CONFIG_FLEXIBLE_BUTTON | 未定义 | 是否启用柔性按键 |

---

## 9. 初始化顺序

```
main()
  ↓
├─ 硬件层 (WDT, GPIO, USB, Power)
├─ 内存层 (PSRAM, cJSON, NVS, Config)
├─ 通信层 (IC message)
├─ 系统层 (System, Shell, EVS, KV)
├─ ── 网络层 (WiFi, BLE, Comm service)
├─ ── 音频层 (Audio, Tone, Player, PA, Button, Battery, Mic)
├─ ── 应用层 (Assistant, Video Camera, View, Cloud)
└─ └─ 配置检查 → WiFi/BLE 配置 → [应用就绪]
```

---

## 10. 时间线

| 时间 (ms) | 操作 | 说明 |
|------|--------|------|
| 0 | 系统上电，WDT 开始运行 |
| 0-300 | 配置解析完成 |
| 300-600 | 应用就绪，可以接收用户输入 |

---

## 11. 外部依赖

### 11.1 核心依赖

- CP 核心音频驱动 (AADC)
- WiFi 驱动（wifi_manager）
- BLE 模块（ls_ble）
- 音频硬件驱动（PA, Speaker）

### 11.2 外部库依赖

- cJSON (JSON 解析，使用 PSRAM)
- LisaPlayer (音频播放框架)
- FreeRTOS (任务调度)
- LISA 框架（线程、队列、消息、日志、KV 存储）

### 11.3 模块依赖

| 模块 | 依赖项 |
|------|--------|------|
| 助手控制器 | 视图层 (lisaui_manager) |
| 通信服务 | WiFi, BLE, 云端 (comm_service) |
| 音频服务 | 音频录制/播放 |
| LED 服务 | led.h |
| KV 存储 | lisa_kv |
| OTA | ota_manager |
| 闹钟 | alarm_ring.h, alarm_next.h |

---

## 12. 关键函数总结

| 函数 | 功能 | 位置 |
|------|------|------|
| `main()` | 入口函数，启动应用任务 | main.c:555 |
| `app_task()` | 主应用任务，系统初始化 | main.c:357 |
| `power_init()` | 电源管理初始化 | power_manager.c:50 |
| `power_wait_settle()` | 等待按键长按检测 | power_manager.c:59 |
| `power_shutdown()` | 系统关机 | main.c:167 |
| `shutdown()` | 关机回调 | main.c:167 |
| `enter_ble_config()` | 进入 BLE 配置模式 | main.c:189 |
| `factory_reset()` | 恢复出厂设置 | main.c:70 |
| `network_reset()` | 网络重置 | main.c:100 |

---

## 13. 系统模式

| 模式 | 触发条件 | 功能 | 说明 |
|------|--------|----------|----------|
| 正常模式 | 有 WiFi 配置 | 自动连接 WiFi |
| BLE 配置模式 | 无 WiFi 配置 | 显示网络配置二维码，进入 BLE 配置 |
| USB-MSC 模式 | USB 连接 | 仅文件传输，不运行应用 |

---

## 14. 总结

1. **初始化顺序**：硬件 → 内存 → 通信 → 系统 → 网络/BLE → 音频 → UI → 控制器
2. **启动检测**：电源按键长按 3 秒或 USB 连接
3. **配置检查**：WiFi 配置检查决定正常模式或 BLE 配置模式
4. **事件驱动**：助手控制器使用事件队列解耦前后端
5. **网络重置**：Factory Reset 清除所有数据，Network Reset 仅清除 WiFi
6. **音频分流**：录制 → USB 输出（5 通道），云端发送（1 通道）

---

**文档版本**: v1.0  
**最后更新**: 2025-02-19  
**维护者**: ListenAI Team
