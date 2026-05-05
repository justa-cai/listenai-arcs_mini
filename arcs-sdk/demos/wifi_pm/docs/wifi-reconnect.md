# WiFi 重连机制

## 整体设计

独立的 `wifi_reconnect_task` 负责断线后的全链路恢复，通过 FreeRTOS Task Notification 触发，采用三阶段串行恢复策略。

## 触发条件

以下事件会触发重连：

| 事件 | 来源 | 触发方式 |
|------|------|---------|
| WiFi 断连 | `EVENT_WIFI_DISCONNECT` 回调 | `schedule_wifi_reconnect()` |
| WiFi 连接失败 | `EVENT_WIFI_STA_CONNECT_FAIL` 回调 | `schedule_wifi_reconnect()` |
| DHCP 失败 | `dhcp_status_callback(success=false)` | 间接 (等待超时后重试) |

```c
static void schedule_wifi_reconnect(void) {
    g_wifi_reconnect_requested = true;
    if (g_wifi_reconnect_task != NULL) {
        xTaskNotifyGive(g_wifi_reconnect_task);
    }
}
```

## 三阶段恢复流程

```
  WiFi 断连事件 / 连接失败 / DHCP 失败
           │
  schedule_wifi_reconnect()
           │
  xTaskNotifyGive(g_wifi_reconnect_task)
           │
  ┌────────┴──────────────────────────────────┐
  │  阶段 1: WiFi 连接                        │
  │  ├─ 检查 wifi_is_link_busy() 防并发连接    │
  │  ├─ 调用 user_wifi_direct_connect()       │
  │  ├─ 每 3 秒 (WIFI_RECONNECT_DELAY_MS) 重试│
  │  └─ 直到 g_wifi_connected == true         │
  ├───────────────────────────────────────────┤
  │  阶段 2: 等待 IP                          │
  │  ├─ 等待 DHCP 回调设置 g_get_ip_success    │
  │  └─ 每 1 秒检查一次                       │
  ├───────────────────────────────────────────┤
  │  阶段 3: MQTT 重连                        │
  │  ├─ mqtt_cleanup() 清理旧连接              │
  │  ├─ mqtt_connect_session() 建立新连接      │
  │  ├─ mqtt_subscribe_default_topic() 重新订阅 │
  │  ├─ mqtt_publish_boot_message() 发布上线消息│
  │  └─ 每 2 秒 (MQTT_RECONNECT_DELAY_MS) 重试│
  ├───────────────────────────────────────────┤
  │  全部成功:                                │
  │  ├─ set_low_power_enabled(true)           │
  │  ├─ g_wifi_reconnect_requested = false    │
  │  └─ wifi_attempt 计数归零                  │
  └───────────────────────────────────────────┘
```

## 断连时的清理操作

WiFi 断连事件处理 (`app_wifi_event_handler`)：

```
EVENT_WIFI_DISCONNECT / EVENT_WIFI_STA_CONNECT_FAIL
  │
  ├─ g_wifi_connected = false
  ├─ g_get_ip_success = false
  ├─ set_low_power_enabled(false)      ← 立即退出低功耗
  ├─ ls_dhcpc_stop()                   ← 停止 DHCP 客户端
  ├─ net_if_down()                     ← 关闭网络接口
  ├─ mqtt_cleanup()                    ← 清理 MQTT 连接
  └─ schedule_wifi_reconnect()         ← 触发重连
```

## WiFi 初始化与连接

### 初始化回调

WiFi 初始化完成后通过回调配置：

```c
static void cb_lisa_wifi_init_done(void) {
    ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, app_wifi_event_handler, NULL);
    wifi_sta_mode_enable();                      // 使能 STA 模式
    wifi_sta_auto_reconnect_enable();            // 使能驱动层自动重连
    wifi_sta_set_listen_itv(WIFI_PS_LISTEN_INTERVAL);  // Listen interval=10
    user_wifi_direct_connect();                  // 发起首次连接
}
```

### 连接参数

```c
wifi_connect_cfg_t cfg = {
    .ssid      = TARGET_WIFI_SSID,
    .key       = TARGET_WIFI_PWD,
    .dhcp_mode = DHCP_CLIENT,       // 使用 DHCP
    .sec       = WIFI_SEC_AUTO,     // 自动检测加密方式
};
wifi_sta_connect(&cfg);
```

### DHCP 回调

```c
static void dhcp_status_callback(int vif_idx, bool success,
                                  uint32_t ip_addr, ...) {
    if (success) {
        g_get_ip_success = true;
        // 如果正在重连，通知重连任务进入下一阶段
        if (g_wifi_reconnect_requested && g_wifi_reconnect_task)
            xTaskNotifyGive(g_wifi_reconnect_task);
    } else {
        g_get_ip_success = false;
    }
}
```

## 并发保护

| 机制 | 实例 | 用途 |
|------|------|------|
| Mutex | `g_mqtt_mutex` | 保护 MQTT 上下文的并发访问 (主循环 / 消息任务 / 重连任务) |
| Task Notification | `xTaskNotifyGive/Take` | 唤醒重连任务、通知 DHCP 结果 |
| Volatile 标志 | `g_wifi_connected`, `g_get_ip_success` | 跨任务共享连接状态 |
| 消息队列 | `g_msg_queue` | 解耦 MQTT 回调与消息处理 |
| 连接状态检查 | `wifi_is_link_busy()` | 防止在连接中/已连接时发起重复连接 |

### `wifi_is_link_busy()` 防重复连接

重连任务在发起连接前会检查 WiFi 链路状态，避免在 "正在连接" 或 "已连接" 状态下重复调用 `wifi_sta_connect()`：

```c
static bool wifi_is_link_busy(void) {
    wifi_link_status_t link_status = {0};
    if (wifi_get_link_status(&link_status) != LS_OK) return false;
    return (link_status.state == STA_IN_CONNECTING) ||
           (link_status.state == STA_CONNECTED);
}
```

## 重连与低功耗的协调

```
  正常运行 (低功耗开启)
         │
    WiFi 断连
         │
  set_low_power_enabled(false)    ← 立即退出低功耗
         │                           (WiFi PS OFF + PM_MODE_ACTIVE)
    三阶段重连
         │
    全部成功
         │
  set_low_power_enabled(true)     ← 重新进入低功耗
                                     (WiFi DTIM PS + PM Light Sleep)
```

这一设计确保重连过程中 CPU 始终活跃（不会意外进入睡眠导致重连中断），重连成功后再恢复低功耗。
