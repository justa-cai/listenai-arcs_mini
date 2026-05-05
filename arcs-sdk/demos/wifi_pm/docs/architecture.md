# 整体架构

## 双核 AMP 架构

本示例运行在 ARCS 双核 AMP (Asymmetric Multi-Processing) 架构上：

```
┌─────────────────────────────────────────────────────┐
│                   AP 核 (HART0)                      │
│  FreeRTOS + App + PM Framework                       │
│                                                      │
│  ┌─────────────┐ ┌──────────┐ ┌──────────────────┐  │
│  │ main loop   │ │ reconnect│ │ msg_handler_task  │  │
│  │ (MQTT处理)  │ │ task     │ │ (消息处理)         │  │
│  └─────────────┘ └──────────┘ └──────────────────┘  │
│                                                      │
│  ┌───────────────────────────────────────────────┐   │
│  │ PM Framework (pm_impl)                        │   │
│  │ 系统层低功耗: PM_MODE_LIGHT_SLEEP             │   │
│  │ (CPU 断电, RAM 保留, AON 域存活)               │   │
│  └───────────────────────────────────────────────┘   │
├──────────────── IPC/MRPC 邮箱 ───────────────────────┤
│                   CP 核 (HART1)                      │
│  WiFi 协议�� + RF 控制                               │
│                                                      │
│  ┌───────────────────────────────────────────────┐   │
│  │ WiFi PS (wifi_ps_hw)                          │   │
│  │ WiFi 层低功耗: WIFI_PS_MODE_DTIM              │   │
│  │ (仅在 DTIM beacon 时唤醒射频接收)              │   │
│  └───────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

- **AP 核 (HART0)**：运行 FreeRTOS、应用逻辑、PM 框架，负责 MQTT 通信和系统级睡眠控制
- **CP 核 (HART1)**：运行 WiFi 协议栈和射频控制，通过 IPC 邮箱接收 AP 核的指令

两个核之间通过 MRPC (Mailbox RPC) 进行通信，WiFi PS 命令和状态同步均走此通道。

## 任务模型

AP 核上运行以下 FreeRTOS 任务：

| 任务 | 栈大小 | 优先级 | 职责 |
|------|--------|--------|------|
| main (默认任务) | — | — | WiFi 初始化 → MQTT 启动 → 主循环 `mqtt_process_once()` |
| `wifi_reconnect` | 1024 words | 4 | WiFi/MQTT 断线重连 (Task Notification 触发) |
| `msg_handler` | 2048 words | 3 | 消费 MQTT 消息队列，解析并执行命令 |
| FreeRTOS idle | — | 0 | 触发 PM 睡眠 (`vPortSuppressTicksAndSleep`) |

## 内存布局

定义在 `memap.h` 中：

| 区域 | 起始地址 | 大小 | 用途 |
|------|---------|------|------|
| ILM | `0x00280000` | 16KB | 指令紧耦合存储器 |
| DLM | `0x00300000` | 8KB | 数据紧耦合存储器 |
| SRAM | `0x20000000` | 320KB | 主 RAM (代码/数据/堆/栈) |
| BTRAM | `0x200C0000` | 32KB | BT 堆 |
| LUNA_RAM | `0x200B0000` | 24KB | 双核共享存储 |
| Flash | `0x30004000` | 8MB - 16KB | 代码/只读数据 (前 16KB 留给 Boot) |
| PSRAM | `0x28000000` | 8MB | 外部 PSRAM |

堆配置：SRAM 堆 32KB + PSRAM 堆 128KB。WiFi 校准数据预留 16KB，中断栈 4KB。

### Boot 与 App 的 Flash 分布

```
  Flash 起始
  0x30000000 ┬──────────────────┐
             │  Boot (16KB)     │  boot.bin
  0x30004000 ├──────────────────┤
             │  App             │  app.bin
             │  (8MB - 16KB)    │  代码 + 只读数据
             │                  │
  0x30800000 └──────────────────┘
```

Boot 通过 `boot.cmake` 脚本将 `boot.bin` 和 `app.bin` 合并为最终固件。

## 主流程

```
main()
  │
  ├─ vrtc_init() + pm_init()              初始化 PM 和虚拟 RTC
  ├─ user_mac_manager_init()              初始化 MAC 地址管理器
  ├─ net_dhcp_register_status_callback()  注册 DHCP 回调
  ├─ xTaskCreate(wifi_reconnect_task)     创建重连任务
  ├─ lisa_wifi_init()                     初始化 WiFi (异步)
  │    └─ cb_lisa_wifi_init_done()
  │         ├─ 注册 WiFi 事件回调
  │         ├─ 使能 STA 模式 + 自动重连
  │         ├─ 设置 listen interval = 10
  │         └─ user_wifi_direct_connect()
  ├─ wait_for_wifi_connection()           等待 WiFi + IP 就绪 (30s 超时)
  ├─ mqtt_start()                         MQTT 连接 + 订阅 + 发布
  ├─ set_low_power_enabled(true)          开启低功耗
  │
  └─ while (1)                            主循环
       ├─ mqtt_process_once()             处理 MQTT 收发
       └─ vTaskDelay(300ms)               让出 CPU → 可能触发 PM 睡眠
```
