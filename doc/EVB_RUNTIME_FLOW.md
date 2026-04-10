# EVB 工程运行流程（arcs-evb）

## 1. 上电启动概览

运行主链路：

- 上电/复位
- 启动代码完成内存初始化、RTOS 启动
- `SYS_INIT` 分级初始化（消息、平台、WiFi、云端、播放器等）
- 进入 `apps/arcs-evb/main.c` 的 `main()`
- `main()` 初始化各 `service_*`，订阅关键消息
- 进入主循环：喂狗 + 事件驱动（语音/网络/云端/UI）

---

## 2. 关键入口文件

- `apps/arcs-evb/main.c`
- `apps/arcs-evb/voice_msg.h` / `apps/arcs-evb/voice_msg.c`
- `src/framework/voice_msg.h` / `src/framework/voice_msg.c`
- `apps/arcs-evb/services/service_*.c`
- `apps/arcs-evb/voice_player.c`
- `src/server/lschat_server/`（云端通信集成）

---

## 3. SYS_INIT 初始化阶段（运行前置）

工程使用分级初始化机制：

- 在各模块里通过 `SYS_INIT(xxx_init, SYS_INIT_LEVEL_PRE_APPLICATION, prio)` 注册
- 消息系统通常在应用 `main()` 之前就已准备好，确保 `voice_msg_sub()` 可用

示例（播放器模块）：

- `apps/arcs-evb/voice_player.c`
  - `SYS_INIT(voice_player_init, SYS_INIT_LEVEL_PRE_APPLICATION, 50);`
  - 在 `VOICE_MSG_PLATFORM_READY` 到来后，订阅 TTS、WiFi、唤醒、MCP 等消息

---

## 4. main() 启动流程（apps/arcs-evb/main.c）

### 4.1 启动时日志与基础动作

- 打印版本号
- 喂狗：`boot_watchdog_feed()`

### 4.2 订阅关键消息

- `VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS`
  - 回调：`voice_system_network_probe_success()`
  - 当前实现：触发 `service_alarm_init()`

- `VOICE_MSG_BUTTON_CHANGE`
  - 回调：`button_changed()`
  - 示例逻辑：
    - K3 重复点击触发 `factory_reset()`
    - 离线模式 K3 单击进入 BLE 配网（播放提示音 + `app_ble_adv_start()`）

### 4.3 初始化应用服务（services）

`main()` 中按顺序初始化：

- `service_led_init()`
- `service_volume_init()`
- `service_brightness_init()`
- `service_button_init()`
- `service_camera_init()`（受 `CONFIG_4G_MODULE` 控制）

### 4.4 UI 初始化（可选）

- 受 `CONFIG_APPLICATION_UI` 控制
- 调用 `lisa_ui_init()`

### 4.5 主循环

- `while (1)`：
  - `boot_watchdog_feed()`
  - `vTaskDelay(pdMS_TO_TICKS(100))`

---

## 5. 消息系统驱动的运行方式（voice_msg/ebus）

工程核心是发布订阅：

- 发布：`voice_msg_pub(msg_id, data, len)`
- 订阅：`voice_msg_sub(msg_id, cb, user_data)`

典型运行时序：

- WiFi 连接完成 -> 发布 IP 获取消息
- 网络探测成功 -> 发布 `VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS`
- 云端连接/会话/tts/iat -> 发布对应 `VOICE_MSG_CLOUD_*`
- 播放器、按键、闹钟等模块分别订阅并处理

---

## 6. WiFi -> 云端会话 -> 播放（主业务链路）

参考 `apps/arcs-evb/docs/02-message-system.md`：

- `VOICE_MSG_WIFI_IP_GOT`
- `network_probe_start()`
- `VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS`
- `voice_cloud_connect()`
- `VOICE_MSG_CLOUD_CONNECTED`
- 后续 IAT/TTS/finish 按 `VOICE_MSG_CLOUD_*` 分发

播放器侧：

- `apps/arcs-evb/voice_player.c` 在平台 ready 后订阅：
  - `VOICE_MSG_CLOUD_TTS_URL`
  - `VOICE_MSG_CLOUD_AUDIO_ITEM`
  - `VOICE_MSG_WIFI_DISCONNECTED`
  - `VOICE_MSG_WAKEUP_KEYWORD` / `VOICE_MSG_WAKEUP_BUTTON_START`
  - 等

---

## 7. 工程运行流程速查（按事件）

- **上电后**：SYS_INIT 完成基础模块就绪 -> `main()` 初始化 services
- **按键**：`VOICE_MSG_BUTTON_CHANGE` -> 进入配网/恢复出厂等
- **联网**：WiFi 事件 -> 网络探测 -> 云端连接
- **对话**：唤醒/按键触发录音上行 -> 云端下发 iat/tts/finish -> 播放器播放 -> 回到待机
- **UI**：若启用 UI，由 `lisa_ui_init()` 初始化并随消息更新显示
