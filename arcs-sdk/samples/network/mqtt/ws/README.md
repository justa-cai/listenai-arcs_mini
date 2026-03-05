# MQTT over WebSocket 示例

本示例演示如何在 ARCS 平台上使用 coreMQTT 库通过 WebSocket 连接到 MQTT broker。

## 功能特性

- 使用 lisa_websocket 组件实现 WebSocket 连接
- 支持 MQTT over WebSocket (端口 8083)
- WiFi 自动连接
- MQTT 发布/订阅功能
- 事件驱动的 WebSocket 通信
- 非阻塞 I/O 操作

## 硬件要求

- ARCS 开发板
- SD 卡(用于文件系统)
- WiFi 网络环境

## 配置说明

在使用前,请修改 [src/main.c](src/main.c) 中的以下配置:

```c
// MQTT Broker 配置
#define MQTT_BROKER_HOST   "broker.emqx.io"       // MQTT broker 地址
#define MQTT_BROKER_PORT   "8083"                 // WebSocket 端口
#define MQTT_BROKER_PATH   "/mqtt"                // MQTT over WebSocket 路径
#define MQTT_CLIENT_ID     "arcs_mqtt_ws_client"

// WiFi 配置
#define TARGET_WIFI_SSID   "your_wifi_ssid"       // 修改为你的WiFi名称
#define TARGET_WIFI_PWD    "your_wifi_pwd"        // 修改为你的WiFi密码

// MQTT 主题
#define MQTT_TOPIC_PUB     "arcs/test/pub"        // 发布主题
#define MQTT_TOPIC_SUB     "arcs/test/sub"        // 订阅主题
```

## WebSocket 配置

### 连接参数

```c
lisa_ws_request_t ws_req = {
    .scheme = "ws",                    // WebSocket 协议
    .host = "broker.emqx.io",          // Broker 地址
    .port = "8083",                    // WebSocket 端口
    .path = "/mqtt",                   // MQTT 路径
    .timeout = 15000,                  // 连接超时(毫秒)
    .on_event = on_ws_event,           // 事件回调
    .on_data = on_ws_data              // 数据回调
};
```

### MQTT 子协议

MQTT over WebSocket 需要在 WebSocket 握手时声明 `mqtt` 子协议。`lisa_websocket` 组件会自动检测路径中包含 `/mqtt` 并设置正确的子协议头:

```
\r\nSec-WebSocket-Protocol: mqtt\r\n
```

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 运行效果

程序启动后,将依次执行以下步骤:

1. **WiFi 连接**: 自动连接到配置的 WiFi 网络
2. **WebSocket 连接**: 建立到 MQTT broker 的 WebSocket 连接
3. **MQTT 连接**: 通过 WebSocket 通道建立 MQTT 连接
4. **订阅主题**: 订阅指定的 MQTT 主题
5. **发布消息**: 发布测试消息
6. **接收消息**: 持续监听和处理接收的消息

### 预期日志输出

```
[mqtt-ws-test] MQTT over WebSocket Test Starting...
[mqtt-ws-test] WiFi connected and IP obtained
========================================
MQTT WebSocket 功能测试
========================================

[lisa-ws] websocket thread waiting for connect sem
[lisa-ws] Got connect signal
[mqtt-ws-test] Waiting for WebSocket connection... (10/100)
[mqtt-ws-test] Connected to MQTT broker via WebSocket

=== 测试 1: MQTT CONNECT ===
[mqtt-ws-test] MQTT Connected successfully

=== 测试 2: MQTT SUBSCRIBE ===
[mqtt-ws-test] Subscribe request sent for topic: arcs/test/sub

=== 测试 3: MQTT PUBLISH ===
[mqtt-ws-test] Published message to topic: arcs/test/pub

=== 测试 4: 接收消息 ===
[mqtt-ws-test] Received PUBLISH: topic=arcs/test/sub, payload=...
```

## 主要差异(相比其他传输方式)

| 特性 | TCP 版本 | SSL 版本 | WebSocket 版本 |
|-----|---------|---------|---------------|
| 端口 | 1883 | 8883 | 8083 |
| 加密 | 无 | TLS 1.2+ | 无 |
| 传输层 | TCP | TCP + TLS | TCP + WebSocket |
| 库依赖 | lwIP | mbedTLS | lisa_websocket + nopoll |
| 连接建立 | TCP 三次握手 | TCP + SSL握手 | TCP + WebSocket握手 |
| 数据传输 | socket send/recv | mbedtls_ssl_write/read | lisa_ws_send/on_data回调 |
| 子协议 | 无 | 无 | mqtt |
| 适用场景 | 标准MQTT | 加密MQTT | 穿越HTTP代理 |

## WebSocket 配置 (prj.conf)

```ini
# WebSocket 支持
CONFIG_LISA_WEBSOCKET=y
CONFIG_MODULE_NOPOLL=y

# coreMQTT
CONFIG_SDK_MODULE_COREMQTT=y
```

## 调试技巧

### WebSocket 连接状态

通过事件回调监控 WebSocket 连接状态:

```c
static void on_ws_event(lisa_ws_event_t *event)
{
    switch (event->what) {
    case LISA_WS_ON_CONNECTING:
        LOGI("WebSocket connecting...");
        break;
    case LISA_WS_ON_CONNECTED:
        LOGI("WebSocket connected");
        break;
    case LISA_WS_ON_DISCONNECTED:
        LOGI("WebSocket disconnected");
        break;
    }
}
```

### 常见问题

1. **连接超时**:
   - 检查 broker 是否支持 WebSocket
   - 确认端口号正确 (通常是 8083)
   - 确认路径正确 (通常是 `/mqtt`)

2. **握手失败**:
   - 检查是否正确设置了 MQTT 子协议
   - 查看 WebSocket 握手日志

3. **数据传输异常**:
   - 检查 `recv_buffer` 大小是否足够
   - 确认事件回调正确设置

## 功能测试

### 使用 MQTTX 客户端测试

[MQTTX](https://mqttx.app/) 是一个优秀的跨平台 MQTT 客户端工具,可用于测试设备的发布和订阅功能。

#### 安装 MQTTX CLI

```bash
# Linux
curl -LO https://www.emqx.com/en/downloads/MQTTX/v1.12.1/mqttx-cli-linux-x64
sudo install ./mqttx-cli-linux-x64 /usr/local/bin/mqttx
```

#### 测试场景 1: 订阅设备发布的消息

在电脑上运行以下命令,订阅设备发布的主题:

```bash
mqttx sub -h broker.emqx.io -p 8083 -t "arcs/test/pub" -l ws --path "/mqtt"
```

**预期结果:**
```
✔ Connected
✔ Subscribed to arcs/test/pub
topic: arcs/test/pub, qos: 0, size: 28B
Hello from ARCS via WebSocket!
```

设备会定期发布消息到 `arcs/test/pub` 主题,MQTTX 客户端会接收并显示这些消息。

#### 测试场景 2: 向设备发布消息

在电脑上运行以下命令,向设备订阅的主题发送消息:

```bash
mqttx pub -h broker.emqx.io -p 8083 -t "arcs/test/sub" -m "Hello ARCS WebSocket" -l ws --path "/mqtt"
```

**预期结果:**

MQTTX 客户端输出:
```
✔ Connected
✔ Message published
```

设备串口日志输出:
```
I/mqtt-ws-test [1034:43:49.966 1 main] Received PUBLISH: topic=arcs/test/sub, payload=Hello ARCS WebSocket
```

#### MQTTX 参数说明

| 参数 | 说明 | 示例值 |
|-----|------|--------|
| `-h` | MQTT broker 地址 | `broker.emqx.io` |
| `-p` | 端口号 | `8083` (WebSocket端口) |
| `-t` | 主题名称 | `arcs/test/pub` |
| `-m` | 消息内容 | `"Hello ARCS WebSocket"` |
| `--protocol` | 协议类型 | `ws` (WebSocket) |
| `--path` | WebSocket 路径 | `/mqtt` |
| `-q` | QoS 级别 (可选) | `0`, `1`, `2` |
| `-u` | 用户名 (可选) | `username` |
| `-P` | 密码 (可选) | `password` |

#### 完整测试流程

1. **启动设备**: 编译并运行 WebSocket MQTT 示例程序
2. **订阅测试**: 在电脑上运行 MQTTX 订阅命令,验证能收到设备消息
3. **发布测试**: 在电脑上运行 MQTTX 发布命令,验证设备能收到消息
4. **双向通信**: 同时开启订阅和发布,观察双向消息传输


### 测试 Checklist

- [ ] WiFi 连接成功并获取 IP 地址
- [ ] WebSocket 握手成功完成
- [ ] MQTT 子协议正确声明
- [ ] MQTT 连接建立成功
- [ ] 主题订阅成功
- [ ] 设备能够成功发布消息
- [ ] 外部客户端能够接收到设备发布的消息
- [ ] 设备能够接收外部客户端发布的消息
- [ ] 消息内容正确无误
- [ ] 程序稳定运行,无异常断线

## 测试建议

1. **先测试 TCP 版本**: 确保基本的 MQTT 功能正常
2. **检查内存配置**: WebSocket 需要额外的缓冲区
3. **验证网络连接**: 确保可以访问目标 broker 的 8083 端口
4. **使用公共 broker 测试**: 如 `broker.emqx.io`
5. **使用专业工具**: 推荐使用 MQTTX 或浏览器开发者工具进行测试

## 公共 MQTT Broker (WebSocket)

| Broker | 地址 | WS端口 | 路径 | 认证 |
|--------|------|--------|------|------|
| EMQX | broker.emqx.io | 8083 | /mqtt | 可选 |
| HiveMQ | broker.hivemq.com | 8000 | /mqtt | 可选 |

## 故障排查

### WebSocket 连接超时

1. 检查 broker 是否支持 WebSocket (通常端口 8083)
2. 确认路径正确 (通常是 `/mqtt`)
3. 检查 MQTT 子协议是否正确声明
4. 增加 `timeout` 参数值

### MQTT 握手失败

1. 检查 WebSocket 连接是否已建立 (`ws_connected` 标志)
2. 确认在 WebSocket 连接成功后才发起 MQTT 连接
3. 检查 MQTT client ID 是否重复

### 数据接收异常

1. 检查 `recv_buffer` 大小 (默认 2048 字节)
2. 确认 `on_data` 回调正确实现
3. 检查数据类型 (MQTT 使用二进制帧)

### 内存不足

1. 增加 `CONFIG_PSRAM_HEAP_SIZE`
2. 减少 `NETWORK_BUFFER_SIZE`
3. 检查 WebSocket 队列大小配置

## WebSocket vs WSS

本示例使用非加密的 WebSocket (ws://)。如果需要加密连接,请使用 [wss](../wss/) 示例,它提供:

- TLS 加密传输
- 端口 8084
- 完整的证书验证支持

## 相关文档

- [coreMQTT 库文档](https://www.freertos.org/mqtt/index.html)
- [WebSocket 协议规范](https://datatracker.ietf.org/doc/html/rfc6455)
- [MQTT over WebSocket 规范](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718127)
- [nopoll 库文档](http://www.aspl.es/nopoll/)

## 许可证

Copyright (c) 2025, LISTENAI
SPDX-License-Identifier: Apache-2.0
