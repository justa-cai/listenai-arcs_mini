# 应用启动流程分析 (src/main.c)

## 目录
- [1. 入口函数 main()](#1-入口函数-main)
- [2. 主应用任务 app_task()](#2-主应用任务-app_task)
- [3. 按键事件处理](#3-按键事件处理)
- [4. 系统重置功能](#4-系统重置功能)
- [5. BLE配置模式](#5-ble配置模式)
- [6. 配置检查与初始化](#6-配置检查与初始化)

---

## 1. 入口函数 main()

### 1.1 硬件初始化阶段

```
main()
├── 关闭看门狗
│   └── WDT_RegDef 寄存器操作 (IP_AP_WDT)
├── GPIO 初始化
│   ├── GPIO_Initialize(GPIOB)
│   └── usb_plug_detect_gpio_init()
├── 电源管理初始化
│   ├── power_config_t 设置 shutdown 回调
│   ├── power_init()
│   └── power_wait_settle() - 等待长按3s启动
└── LED 闪烁提示
    └── app_led_blink(50, 50)
```

**关键点**：
- 如果 `power_wait_settle()` 返回 false，系统直接关机
- 启动需要长按电源键3秒

### 1.2 内存与配置初始化

```
main() 续
├── 外部内存启用
│   └── heap_caps_malloc_extmem_enable(16)
├── cJSON 内存钩子设置
│   ├── cjson_malloc (使用 exram_malloc)
│   └── cjson_free (使用 exram_free)
├── 配置解析
│   ├── config_init(0x300f0000) - 从固定地址读取配置
│   └── config_print() - 打印配置信息
└── 创建应用任务
    └── lisa_thread_create("APP_BOOT_TASK", app_task)
```

**配置地址**：`0x300f0000`

---

## 2. 主应用任务 app_task()

### 2.1 核心服务初始化流程

```
app_task()
├── 核间通信初始化
│   ├── ipc_mem_init(1)
│   └── ipc_master_init(NULL)
├── 系统初始化
│   └── ls_sys_init(TIMEZONE_SHANGHAI)
├── Shell 初始化
│   ├── lisa_shell_init()
│   ├── 暂停系统日志 ("sys.log")
│   └── 重定向到 shell 输出
├── 核间消息初始化
│   └── ic_message_init()
├── 文件系统初始化
│   ├── user_fs_init() (LSFS)
│   └── lisa_kv_init()
├── 事件系统初始化
│   └── evs_utils_init()
├── 设备ID更新
│   └── update_device_id()
└── USB 启动
    └── user_usb_start()
```

### 2.2 USB-MSC 模式检查

```c
if (app_usb_msc_enabled()) {
    // USB-MSC 模式下只支持文件传输，不运行应用程序
    while (1) {
        lisa_thread_delay(1000);
    }
}
```

### 2.3 NVS 与 WiFi 预初始化

```
app_task() 续
├── NVS 初始化
│   └── arcs_nvs_init()
├── WiFi 预初始化
│   └── ls_wifi_pre_init(_main_wifi_stack_init_done_cb)
├── BLE 初始化
│   └── ls_ble_init()
└── 通信服务初始化
    └── comm_service_init()
```

### 2.4 硬件组件初始化

```
app_task() 续
├── LED 服务
│   ├── app_led_init()
│   └── app_led_off()
├── 音频服务
│   ├── audio_play_service_start()
│   └── audio_record_service_start()
├── PA 与扬声器
│   ├── pa_manager_pre_init()
│   ├── pa_manager_init(PA_MGR_NONE)
│   └── listen_spk_init()
└── Tone 初始化
    └── app_tone_default_init()
```

### 2.5 LisaPlayer 与 UI

```
app_task() 续
├── LisaPlayer 初始化
│   └── app_player_init()
├── 欢迎界面
│   └── app_main()
├── 按键初始化
│   └── lisa_btn_init(button_callback_handle)
├── 电池与麦克风
│   ├── battery_adc_sample_init()
│   └── listen_mic_gain_init()
├── 用户数据加载
│   └── assistant_view_userdata_load()
├── 摄像头初始化
│   ├── video_camera_init()
│   └── camera_senor_release()
└── 助手控制器初始化
    └── assist_controller_init()
```

### 2.6 WiFi 配置检查

```c
// 检查是否有已保存的 WiFi 配置
wifi_mgr_sta_config_t *matched_list = NULL;
ret = wifi_mgr_storage_search_ap(&matched_list, SEARCH_ALL, NULL);

if (ret > 0 && matched_list != NULL) {
    // 有配置，正常启动
    has_wifi_config = true;
    free(matched_list);
} else {
    // 无配置，进入 BLE 配置模式
    change_info_page(LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_NETWORK);
    enter_ble_config(true);
}
```

---

## 3. 按键事件处理

### 3.1 按键类型与事件

```
button_callback_handle()
├── LISA_BTN_ID_POWER (仅处理电源键)
└── 事件类型:
    ├── LISA_BTN_PRESS_CLICK (单击)
    ├── LISA_BTN_PRESS_DOUBLE_CLICK (双击)
    ├── LISA_BTN_PRESS_TRIPLE_CLICK (三击)
    ├── LISA_BTN_PRESS_QUADRUPLE_CLICK (四击)
    ├── LISA_BTN_PRESS_QUINTUPLE_CLICK (五击)
    ├── LISA_BTN_PRESS_REPEAT_CLICK (连击)
    └── LISA_BTN_PRESS_LONG_HOLD (长按)
```

### 3.2 按键功能映射

| 按键操作 | 功能 |
|---------|------|
| 单击 (非主页) | 返回主页 |
| 单击 (主页 + 聆听状态) | 进入待唤醒 (app_btn_idle) |
| 单击 (主页) | 执行唤醒 (app_btn_wakeup) |
| 双击 | 拍照识图 (photo_recognition_trigger) |
| 三击 | 切换信息页 (CONTROLLER_EVENT_OPT_TOGGLE_INFO_PAGE) |
| 连击 ≥ 8次 | 恢复出厂设置 (factory_reset) |
| 连击 ≥ 5次 | 网络重置 (network_reset) + BLE配置 |
| 长按 (USB未连接) | 关机 (power_shutdown) |
| 长按 (USB已连接) | 忽略 |

### 3.3 单击流程图

```
单击处理
├── 判断: is_primary_page_active()
│   ├── FALSE → 返回主页
│   │   └── lisaui_manager_group_enter(LAUNCHER, PRIMARY_PAGE)
│   └── TRUE → 判断: get_audio_listen_status()
│       ├── TRUE (聆听中) → app_btn_idle()
│       └── FALSE → app_btn_wakeup()
└── alarm_ring_stop()
```

### 3.4 连击处理流程 (≥8次 恢复出厂)

```
八击以上处理
├── enter_audio_idle()
├── play_factory_reset_audio()
├── enter_ble_config(false) - 不播放音频
├── factory_reset()
│   ├── 删除 WiFi 配置
│   │   ├── lisa_kv_del("wifi-info-count")
│   │   └── lisa_kv_del("wifi-list")
│   └── 删除用户凭据
│       ├── lisa_kv_del("user.pid")
│       ├── lisa_kv_del("user.sid")
│       ├── lisa_kv_del("user.volume")
│       ├── lisa_kv_del("user.mic_gain")
│       └── lisa_kv_del("user.aec_gain")
├── app_cloud_disconnect()
├── wifi_mgr_sta_disconnect(false)
└── change_info_page(CONFIGURE_NETWORK)
```

---

## 4. 系统重置功能

### 4.1 Factory Reset (恢复出厂设置)

```
factory_reset()
├── WiFi 配置清除
│   ├── "wifi-info-count"
│   └── "wifi-list"
└── 用户数据清除
    ├── "user.pid" - 用户ID
    ├── "user.sid" - 会话ID
    ├── "user.volume" - 音量设置
    ├── "user.mic_gain" - 麦克风增益
    └── "user.aec_gain" - AEC增益
```

### 4.2 Network Reset (网络重置)

```
network_reset()
├── WiFi 配置清除
│   ├── "wifi-info-count"
│   └── "wifi-list"
└── 保留用户数据
```

### 4.3 关机流程 (shutdown)

```
shutdown()
├── pa_manager_onoff(0) - 关闭功放
├── lisa_kv_poweroff_save() - 保存KV数据
├── app_led_off() - 关闭LED
├── lisa_display_blanking_on() - 屏幕息屏
├── lisa_display_set_brightness(0) - 亮度设为0
└── vTaskDelay(100) - 延时等待
```

---

## 5. BLE配置模式

### 5.1 enter_ble_config() 流程

```
enter_ble_config(bool play_audio)
├── 如果 play_audio == true
│   └── assist_controller_trigger_event(ENTER_BLE_CONFIG)
├── BLE 广播启动 (最多重试5次)
│   ├── app_ble_adv_start(0, BLE_ADV_GEN)
│   └── 失败重试 (间隔 100ms)
└── 结果判断
    ├── 成功 → "BLE Config Mode Ready"
    └── 失败 → "BLE Config Mode Failed"
```

### 5.2 BLE 配置常量

```c
#define BLE_ADV_START_MAX_RETRIES  5  // 最大重试次数
#define BLE_ADV_START_RETRY_DELAY_MS 100  // 重试间隔
```

### 5.3 触发 BLE 配置的场景

| 场景 | 播放音频 |
|-----|---------|
| 启动时无 WiFi 配置 | 是 |
| 五击以上按键 | 是 |
| 八击以上恢复出厂 | 否 |

---

## 6. 配置检查与初始化

### 6.1 NVS (Non-Volatile Storage) 配置

```c
#define NVDS_FLASH_ADDRESS_OFFSET  (0xFF8000)  // Flash最后32KB
#define NVDS_FLASH_SIZE            (0x8000)     // 32KB
```

### 6.2 NVS 初始化流程

```
arcs_nvs_init()
├── flash_if_init() - Flash接口初始化
├── flash_get_page_info_by_offs() - 获取页信息
├── 设置 sector_size 和 sector_count
└── nvds_init() - NVS初始化
```

### 6.3 DAC 配置

```c
#define ARCS_DAC_USE_LITE_DAC  (1)
```

当 `ARCS_DAC_USE_LITE_DAC = 1` 时：
- 跳过 `pa_manager_pre_init()`
- 跳过 `listen_spk_init()`

---

## 7. WiFi 栈初始化完成回调

```
__handle_wifi_stack_init_done()
└── LISA_LOGI("Wifi Stack Init Done")
    // 当前版本大部分初始化代码已注释
    // 历史上曾包含:
    //   - Flash/EasyFlash 初始化
    //   - PA/Speaker 初始化
    //   - System/Tone/Player 初始化
```

---

## 8. cJSON 内存钩子

```c
// 将 cJSON 的内存操作重定向到外部 RAM
cjson_hooks = {
    .malloc_fn = cjson_malloc,  // exram_malloc(4, sz)
    .free_fn = cjson_free       // exram_free(ptr)
};
```

---

## 9. 启动时序图

```
main()
  │
  ├─ 关闭看门狗 ─────────────────────────────────┐
  ├─ GPIO初始化                                   │
  ├─ 电源初始化                                   │ (约几ms)
  ├─ 等待长按3s ─────────────────────────────────┘
  │
  ├─ 外部内存启用 ─────────────────────────────┐
  ├─ 配置解析 (config_init)                    │
  │                                         │ (配置解析时间)
  └─ 创建 app_task ──────────────────────────┘
        │
        ▼
    app_task()
        │
        ├─ 核间通信 ────────────────────────────────┐
        ├─ 系统初始化                               │
        ├─ Shell 初始化                            │
        ├─ IC消息 ─────────────────────────────────┤
        ├─ 文件系统                                │
        ├─ 事件系统                                │ (核心服务层)
        ├─ USB启动 ────────────────────────────────┤
        │                                         │
        ├─ NVS初始化 ──────────────────────────────┐
        ├─ WiFi预初始化                            │
        ├─ BLE初始化                               │ (网络层)
        ├─ 通信服务 ──────────────────────────────┤
        │                                         │
        ├─ LED服务 ────────────────────────────────┐
        ├─ 音频服务 (播放/录音)                    │
        ├─ PA/扬声器                              │ (硬件层)
        ├─ Tone ──────────────────────────────────┤
        │                                         │
        ├─ LisaPlayer ────────────────────────────┐
        ├─ 欢迎界面                                │
        ├─ 按键服务                                │ (应用层)
        ├─ 电池/麦克风                            │
        ├─ 摄像头 ─────────────────────────────────┤
        │                                         │
        └─ WiFi配置检查 ──────────────────────────┘
              │
              ├── 有配置 → 正常启动
              └── 无配置 → BLE配置模式
```

---

## 10. 关键文件路径

| 文件 | 描述 |
|-----|------|
| `src/main.c` | 主程序入口 |
| 配置地址 | `0x300f0000` |
| NVS Flash偏移 | `0xFF8000` (最后32KB) |

---

## 11. 重要常量定义

```c
// 任务配置
#define APP_TASK_STACK     (4096 * 2)  // 8KB
#define APP_TASK_PRIO      LISA_OS_PRIORITY_NORMAL

// BLE配置
#define BLE_ADV_START_MAX_RETRIES    5
#define BLE_ADV_START_RETRY_DELAY_MS 100

// DAC配置
#define ARCS_DAC_USE_LITE_DAC    1

// NVS配置
#define NVDS_FLASH_ADDRESS_OFFSET  0xFF8000
#define NVDS_FLASH_SIZE           0x8000
```

---

## 12. 外部依赖

### 核心模块
- `lisa_kv.h` - 键值存储
- `lisa_log.h` - 日志系统
- `lisa_thread.h` - 线程管理
- `wifi_manager/wifi_manager.h` - WiFi管理
- `power/power_manager.h` - 电源管理
- `cJSON.h` - JSON解析

### UI模块
- `lisa_display.h` - 显示屏
- `led.h` - LED控制
- `lisaui_manager.h` - UI管理器
- `assistant_view.h` - 助手视图
- `assistant_controller.h` - 助手控制器

### 音频模块
- `pa_manager.h` - 功放管理
- `app_player.h` - 播放器
- `app_tone.h` - 提示音
- `listen_mic_gain.h` - 麦克风增益

### 其他
- `video_camera.h` - 摄像头
- `bt_app_if.h` - 蓝牙接口
- `alarm_ring.h` - 闹钟铃声
- `nvs.h` - 非易失性存储
