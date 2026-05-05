# MQTT 交互

## 连接参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| Broker | `192.168.32.205:1883` | 使用前需修改 |
| Client ID | `arcs_mqtt_client` | 使用前需修改 |
| Keep-Alive | 120 秒 | MQTT 心跳周期 |
| 发布 Topic | `arcs/test/pub` | 设备 → Broker |
| 订阅 Topic | `arcs/test/sub` | Broker → 设备 |
| QoS | 0 (At most once) | 不重传 |
| 网络缓冲区 | 1024 字节 | 收发共用 |
| 连接重试 | 最多 3 次，间隔 1 秒 | |

## 消息处理架构

```
  MQTT Broker
      │
  mqtt_event_callback()     ← coreMQTT 回调 (仅入队, 不做耗时处理)
      │
  xQueueSend(g_msg_queue)   ← FreeRTOS 消息队列 (深度 5, 每条最大 128 字节)
      │
  msg_handler_task()         ← 独立任务消费队列
      │
  handle_mqtt_message()      ← 解析并处理命令
      │
  mqtt_publish_response()    ← 加锁发布响应到 arcs/test/pub
```

### 设计要点

1. **回调不做耗时操作**：`mqtt_event_callback()` 仅做 payload 拷贝 + 入队，不阻塞 coreMQTT 内部处理
2. **独立处理任务**：`msg_handler_task` 以独立任务消费队列，与 MQTT 协议处理解耦
3. **互斥保护**：所有 MQTT 发布操作通过 `g_mqtt_mutex` 加锁，防止主循环和消息任务并发写 socket

## 消息协议

### 请求格式

```
<cmd>
<cmd>:<arg>
```

### 响应格式

```
<cmd>:<code>:<message>
```

- `code = 0`：成功
- `code < 0`：失败

### 支持的命令

| 命令 | 响应示例 | 说明 |
|------|---------|------|
| `status` | `status:0:wifi=1,ip=1,uptime=12345` | 返回 WiFi 连接状态、IP 状态和运行时长 (ms) |
| `ping` | `pong:0:ok` | 心跳探测 |
| `reboot` | `reboot:0:rebooting` | 发送响应后延时 1 秒重启 |
| 其他 | `error:-1:unknown cmd` | 未知命令 |

## MQTT 生命周期

### 启动流程

```
mqtt_start()
  ├─ mqtt_prepare_runtime()          初始化运行时结构体
  │    ├─ 设置 socket = -1
  │    ├─ 绑定 transport_recv / transport_send
  │    └─ 绑定 network_buffer (1024B)
  ├─ mqtt_create_runtime_resources()  创建运行时资源
  │    ├─ xSemaphoreCreateMutex()    → g_mqtt_mutex
  │    ├─ xQueueCreate(5, ...)       → g_msg_queue
  │    └─ xTaskCreate(msg_handler)   → 消息处理任务
  ├─ mqtt_connect_session()           建立 MQTT 连接
  │    ├─ connect_to_broker()        TCP 连接 (socket + connect)
  │    │    └─ fcntl(O_NONBLOCK)     设置非阻塞模式
  │    ├─ MQTT_Init()                初始化 coreMQTT 上下文
  │    └─ MQTT_Connect() × 3        最多重试 3 次，间隔 1 秒
  ├─ mqtt_subscribe_default_topic()   订阅 arcs/test/sub
  │    ├─ MQTT_Subscribe(QoS0)
  │    └─ MQTT_ProcessLoop()         等待 SUBACK
  └─ mqtt_publish_boot_message()      发布 "Hello from ARCS!"
```

### 主循环

```c
while (1) {
    mqtt_process_once();     // 加锁 → MQTT_ProcessLoop() → 解锁
    vTaskDelay(300ms);       // 让出 CPU，可能触发 PM 睡眠
}
```

`mqtt_process_once()` 包含多重前置检查：
- `g_wifi_connected` — WiFi 是否连接
- `g_get_ip_success` — 是否已获取 IP
- `g_mqtt_ctx != NULL` — MQTT 上下文是否有效
- `xSemaphoreTake(g_mqtt_mutex)` — 能否获取锁

任何一项不满足则直接返回，不做处理。

### 清理流程

```
mqtt_cleanup()
  ├─ xSemaphoreTake(g_mqtt_mutex)   加锁
  ├─ g_mqtt_ctx = NULL              清除全局指针 (阻止其他任务使用)
  ├─ close(socket)                  关闭 TCP 连接
  ├─ mqtt_prepare_runtime()         重置运行时结构体
  └─ xSemaphoreGive(g_mqtt_mutex)   解锁
```

## Transport 层

MQTT 使用 BSD socket 作为传输层，通过 coreMQTT 的 `TransportInterface_t` 接口适配：

```c
// 接收：非阻塞模式，EAGAIN 返回 0 (无数据), 错误返回 -1
static int32_t transport_recv(NetworkContext_t *ctx, void *buf, size_t len) {
    int ret = recv(ctx->socket, buf, len, 0);
    if (ret < 0 && (errno == EWOULDBLOCK || errno == EAGAIN))
        return 0;
    return ret < 0 ? -1 : ret;
}

// 发送：非阻塞模式，同上
static int32_t transport_send(NetworkContext_t *ctx, const void *buf, size_t len) {
    int ret = send(ctx->socket, buf, len, 0);
    if (ret < 0 && (errno == EWOULDBLOCK || errno == EAGAIN))
        return 0;
    return ret < 0 ? -1 : ret;
}
```

Socket 在 `connect_to_broker()` 中设置为非阻塞模式 (`O_NONBLOCK`)，确保 `MQTT_ProcessLoop()` 不会长时间阻塞，从而不影响 PM 睡眠触发。

---

## 本地 MQTT Broker 搭建与测试

wifi_pm 使用局域网 TCP MQTT（端口 1883），需要一个本地 MQTT Broker。推荐使用 Mosquitto。

### 安装 Mosquitto

```bash
sudo apt install -y mosquitto mosquitto-clients
```

### 配置局域网访问

默认 Mosquitto 仅监听 localhost，需配置为监听所有网卡并允许匿名访问：

```bash
sudo tee /etc/mosquitto/conf.d/wifi_pm.conf > /dev/null << 'EOF'
listener 1883 0.0.0.0
allow_anonymous true
EOF

sudo systemctl restart mosquitto
```

### 修改设备代码

将 `src/main.c` 中的 `MQTT_BROKER_HOST` 改为 PC 的局域网 IP：

```c
#define MQTT_BROKER_HOST   "192.168.x.x"   // 替换为你的 PC IP
```

查看 PC IP：`ip -4 addr show | grep -oP 'inet \K[0-9.]+' | grep -v '127.0.0.1'`

### 测试脚本

项目中提供了 `mqtt_test.sh` 脚本，简化测试流程：

```bash
# 查看 Broker 状态
./mqtt_test.sh status

# 终端 1: 订阅设备发布的消息
./mqtt_test.sh sub

# 终端 2: 向设备发送命令
./mqtt_test.sh pub ping       # 心跳探测，设备应回复 pong:0:ok
./mqtt_test.sh pub status     # 查询状态，设备回复连接信息和运行时长
./mqtt_test.sh pub reboot     # 远程重启
```

### 手动测试

```bash
# 订阅设备发布的消息 (终端 1)
mosquitto_sub -h <PC_IP> -p 1883 -t "arcs/test/pub" -v

# 向设备发送命令 (终端 2)
mosquitto_pub -h <PC_IP> -p 1883 -t "arcs/test/sub" -m "ping"
mosquitto_pub -h <PC_IP> -p 1883 -t "arcs/test/sub" -m "status"
```

### 预期交互

设备上电后：

```
设备 → Broker:  [arcs/test/pub] "Hello from ARCS!"     (开机消息)

PC 发送:        [arcs/test/sub] "ping"
设备 → Broker:  [arcs/test/pub] "pong:0:ok"

PC 发送:        [arcs/test/sub] "status"
设备 → Broker:  [arcs/test/pub] "status:0:wifi=1,ip=1,uptime=12345"
```

### 完整测试步骤

#### 第 1 步：确认 Broker 运行

```bash
./mqtt_test.sh status
```

预期输出：`状态: 运行中` + `[OK] Broker 可连接`。如未运行，执行 `./mqtt_test.sh setup`。

#### 第 2 步：终端 1 订阅设备消息

```bash
./mqtt_test.sh sub
```

此终端会阻塞等待，设备发的所有消息都会实时打印。

#### 第 3 步：烧录并启动设备

设备上电后依次完成 WiFi 连接 → MQTT 连接 → 发布开机消息。
终端 1 应收到：

```
arcs/test/pub Hello from ARCS!
```

看到此消息说明 **设备 → Broker → PC** 链路已通。

#### 第 4 步：终端 2 向设备发命令

```bash
./mqtt_test.sh pub ping
```

终端 1 应收到设备的响应：

```
arcs/test/pub pong:0:ok
```

继续测试其他命令：

```bash
./mqtt_test.sh pub status    # 终端 1 收到 status:0:wifi=1,ip=1,uptime=xxxxx
./mqtt_test.sh pub reboot    # 终端 1 收到 reboot:0:rebooting，设备随后重启
```

### 交互时序图

```
终端 1 (sub)                    设备                        终端 2 (pub)
    │                            │                              │
    │   (设备上电)                │                              │
    │◄── Hello from ARCS! ──────┤                              │
    │                            │                              │
    │                            │◄──── "ping" ────────────────┤
    │◄── pong:0:ok ─────────────┤                              │
    │                            │                              │
    │                            │◄──── "status" ──────────────┤
    │◄── status:0:wifi=1,... ───┤                              │
    │                            │                              │
    │                            │◄──── "reboot" ──────────────┤
    │◄── reboot:0:rebooting ────┤                              │
    │                            │ (设备重启)                    │
```

### 常见问题

| 问题 | 排查 |
|------|------|
| 终端 1 (`sub`) 无任何输出 | 设备未连上 Broker —— 检查 IP/端口/WiFi 是否同一子网；查看设备串口日志 |
| 收到 `Hello from ARCS!` 但 pub 无响应 | 设备订阅正常但消息处理异常 —— 检查设备串口日志是否有 `CMD:` 打印 |
| `Error: Connection refused` | Broker 未运行 —— `./mqtt_test.sh setup` 重新配置并启动 |
| 设备连不上 Broker | 确认 PC 和设备在同一子网；`mosquitto_pub -h <IP> -t test -m x` 验证 Broker 可达 |
| Broker 启动失败 | `sudo systemctl status mosquitto` 查看日志；检查 1883 端口是否被占用 |
| 设备已连接但收不到命令 | 确认发布到 `arcs/test/sub`（设备订阅的 topic），而非 `arcs/test/pub` |
| 防火墙拦截 | `sudo ufw allow 1883/tcp` 或 `sudo iptables -A INPUT -p tcp --dport 1883 -j ACCEPT` |
