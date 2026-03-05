# MQTT over WebSocket Secure (WSS) 示例

本示例演示如何在 ARCS 平台上使用 coreMQTT 库通过 WebSocket Secure 连接到 MQTT broker。

## 功能特性

- 使用 lisa_websocket 组件实现 WebSocket Secure 连接
- 支持 MQTT over WSS (端口 8084)
- TLS/SSL 加密传输
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
#define MQTT_BROKER_PORT   "8084"                 // WebSocket Secure 端口
#define MQTT_BROKER_PATH   "/mqtt"                // MQTT over WSS 路径
#define MQTT_CLIENT_ID     "arcs_mqtt_wss_client"

// WiFi 配置
#define TARGET_WIFI_SSID   "your_wifi_ssid"       // 修改为你的WiFi名称
#define TARGET_WIFI_PWD    "your_wifi_pwd"        // 修改为你的WiFi密码

// MQTT 主题
#define MQTT_TOPIC_PUB     "arcs/test/pub"        // 发布主题
#define MQTT_TOPIC_SUB     "arcs/test/sub"        // 订阅主题
```

## WebSocket Secure 配置

### 连接参数

```c
lisa_ws_request_t ws_req = {
    .scheme = "wss",                   // WebSocket Secure 协议
    .host = "broker.emqx.io",          // Broker 地址
    .port = "8084",                    // WSS 端口
    .path = "/mqtt",                   // MQTT 路径
    .timeout = 15000,                  // 连接超时(毫秒)
    .on_event = on_ws_event,           // 事件回调
    .on_data = on_ws_data              // 数据回调
};
```

### MQTT 子协议

MQTT over WebSocket 需要在 WebSocket 握手时声明 `mqtt` 子协议。`lisa_websocket` 组件会自动检测路径中包含 `/mqtt` 并设置正确的子协议头:

```
Sec-WebSocket-Protocol: mqtt
```

### SSL/TLS 配置

当前配置为**不验证服务器证书**模式(适用于测试):

```c
nopoll_conn_opts_ssl_peer_verify(nopoll_opts, nopoll_false);
```

生产环境建议启用证书验证。

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 运行效果

程序启动后,将依次执行以下步骤:

1. **WiFi 连接**: 自动连接到配置的 WiFi 网络
2. **WebSocket Secure 连接**: 建立到 MQTT broker 的加密 WebSocket 连接
3. **MQTT 连接**: 通过 WSS 通道建立 MQTT 连接
4. **订阅主题**: 订阅指定的 MQTT 主题
5. **发布消息**: 发布测试消息
6. **接收消息**: 持续监听和处理接收的消息

### 预期日志输出

```
[mqtt-wss-test] MQTT over WebSocket Secure Test Starting...
[mqtt-wss-test] WiFi connected and IP obtained
========================================
MQTT WebSocket Secure 功能测试
========================================

[lisa-ws] websocket thread waiting for connect sem
[lisa-ws] Got connect signal
[mqtt-wss-test] Waiting for WebSocket Secure connection... (10/100)
[mqtt-wss-test] Connected to MQTT broker via WebSocket Secure

=== 测试 1: MQTT CONNECT ===
[mqtt-wss-test] MQTT Connected successfully

=== 测试 2: MQTT SUBSCRIBE ===
[mqtt-wss-test] Subscribe request sent for topic: arcs/test/sub

=== 测试 3: MQTT PUBLISH ===
[mqtt-wss-test] Published message to topic: arcs/test/pub

=== 测试 4: 接收消息 ===
[mqtt-wss-test] Received PUBLISH: topic=arcs/test/sub, payload=...
```

## 主要差异(相比其他传输方式)

| 特性 | TCP | SSL | WebSocket | WSS |
|-----|-----|-----|-----------|-----|
| 端口 | 1883 | 8883 | 8083 | 8084 |
| 加密 | 无 | TLS 1.2+ | 无 | TLS 1.2+ |
| 传输层 | TCP | TCP + TLS | TCP + WS | TCP + WS + TLS |
| 库依赖 | lwIP | mbedTLS | nopoll | nopoll + mbedTLS |
| 连接建立 | TCP 握手 | TCP + SSL握手 | TCP + WS握手 | TCP + WS + SSL握手 |
| 数据传输 | socket | mbedtls_ssl | lisa_ws | lisa_ws (加密) |
| 子协议 | 无 | 无 | mqtt | mqtt |
| 适用场景 | 标准MQTT | 加密MQTT | 穿越代理 | 加密+穿越代理 |

## 配置文件 (prj.conf)

```ini
# WebSocket Secure 支持
CONFIG_LISA_WEBSOCKET=y
CONFIG_MODULE_NOPOLL=y

# mbedTLS (SSL/TLS支持 - WSS需要)
CONFIG_MBEDTLS=y
CONFIG_MBEDTLS_BUILTIN=y
CONFIG_MBEDTLS_ENABLE_HEAP=y
CONFIG_MBEDTLS_HEAP_SIZE=0x8000

# mbedTLS 功能配置
CONFIG_MBEDTLS_SSL_PROTO_TLS1_2=y
CONFIG_MBEDTLS_KEY_EXCHANGE_RSA_ENABLED=y
CONFIG_MBEDTLS_SSL_CLI_C=y
CONFIG_MBEDTLS_SSL_TLS_C=y
CONFIG_MBEDTLS_X509_CRT_PARSE_C=y
CONFIG_MBEDTLS_PEM_PARSE_C=y

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

1. **SSL 握手失败**:
   - 检查 mbedTLS 堆大小 (CONFIG_MBEDTLS_HEAP_SIZE)
   - 确认系统时间正确 (影响证书验证)
   - 检查是否禁用了证书验证

2. **连接超时**:
   - 检查 broker 是否支持 WSS
   - 确认端口号正确 (通常是 8084)
   - 确认路径正确 (通常是 `/mqtt`)

3. **数据传输异常**:
   - 检查 `recv_buffer` 大小是否足够
   - 确认事件回调正确设置
   - 检查 SSL 加密是否正常

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
mqttx sub -h broker.emqx.io -p 8084 -t "arcs/test/pub" -l wss --path "/mqtt"
```

**预期结果:**
```
✔ Connected
✔ Subscribed to arcs/test/pub
topic: arcs/test/pub, qos: 0, size: 38B
Hello from ARCS via WebSocket Secure!
```

设备会定期发布消息到 `arcs/test/pub` 主题,MQTTX 客户端会接收并显示这些消息。

#### 测试场景 2: 向设备发布消息

在电脑上运行以下命令,向设备订阅的主题发送消息:

```bash
mqttx pub -h broker.emqx.io -p 8084 -t "arcs/test/sub" -m "Hello ARCS WSS" -l wss --path "/mqtt"
```

**预期结果:**

MQTTX 客户端输出:
```
✔ Connected
✔ Message published
```

设备串口日志输出:
```
I/mqtt-wss-test [1034:43:49.966 1 main] Received PUBLISH: topic=arcs/test/sub, payload=Hello ARCS WSS
```

#### MQTTX 参数说明

| 参数 | 说明 | 示例值 |
|-----|------|--------|
| `-h` | MQTT broker 地址 | `broker.emqx.io` |
| `-p` | 端口号 | `8084` (WSS端口) |
| `-t` | 主题名称 | `arcs/test/pub` |
| `-m` | 消息内容 | `"Hello ARCS WSS"` |
| `--protocol` | 协议类型 | `wss` (WebSocket Secure) |
| `--path` | WebSocket 路径 | `/mqtt` |
| `-q` | QoS 级别 (可选) | `0`, `1`, `2` |
| `-u` | 用户名 (可选) | `username` |
| `-P` | 密码 (可选) | `password` |

#### 完整测试流程

1. **启动设备**: 编译并运行 WSS MQTT 示例程序
2. **订阅测试**: 在电脑上运行 MQTTX 订阅命令,验证能收到设备消息
3. **发布测试**: 在电脑上运行 MQTTX 发布命令,验证设备能收到消息
4. **双向通信**: 同时开启订阅和发布,观察双向消息传输

### 使用浏览器 WebSocket 测试

WebSocket Secure 也可以在浏览器中测试:

```javascript
// 在浏览器控制台运行
const ws = new WebSocket('wss://broker.emqx.io:8084/mqtt', 'mqtt');
ws.binaryType = 'arraybuffer';

ws.onopen = () => {
    console.log('WSS connected');
    // 这里可以发送 MQTT 协议数据包
};

ws.onmessage = (event) => {
    console.log('Received:', event.data);
};
```

### 测试 Checklist

- [ ] WiFi 连接成功并获取 IP 地址
- [ ] SSL/TLS 握手成功完成
- [ ] WebSocket 握手成功完成
- [ ] MQTT 子协议正确声明
- [ ] MQTT 连接建立成功
- [ ] 主题订阅成功
- [ ] 设备能够成功发布消息
- [ ] 外部客户端能够接收到设备发布的消息
- [ ] 设备能够接收外部客户端发布的消息
- [ ] 消息内容正确无误
- [ ] 加密连接稳定,无异常断线

## 测试建议

1. **先测试 WS 版本**: 确保 WebSocket 基本功能正常
2. **检查内存配置**: WSS 需要更多内存 (WebSocket + SSL)
3. **验证网络连接**: 确保可以访问目标 broker 的 8084 端口
4. **使用公共 broker 测试**: 如 `broker.emqx.io`
5. **使用专业工具**: 推荐使用 MQTTX 或浏览器开发者工具进行测试

## 公共 MQTT Broker (WebSocket Secure)

| Broker | 地址 | WSS端口 | 路径 | 认证 |
|--------|------|---------|------|------|
| EMQX | broker.emqx.io | 8084 | /mqtt | 可选 |
| HiveMQ | broker.hivemq.com | 8884 | /mqtt | 可选 |

## 故障排查

### SSL/TLS 握手失败

1. 检查 mbedTLS 堆大小 (CONFIG_MBEDTLS_HEAP_SIZE)
2. 确认系统时间正确
3. 检查是否正确禁用证书验证 (测试模式)
4. 查看 SSL 错误码

### WebSocket 连接超时

1. 检查 broker 是否支持 WSS (通常端口 8084)
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

1. 增加 `CONFIG_MBEDTLS_HEAP_SIZE` (SSL 需要)
2. 增加 `CONFIG_PSRAM_HEAP_SIZE`
3. 减少 `NETWORK_BUFFER_SIZE`
4. 检查 WebSocket 队列大小配置

## WSS 的优势

相比其他传输方式,WSS 提供:

1. **双重安全**: WebSocket 协议 + TLS 加密
2. **防火墙友好**: 使用 443 或 8084 端口,易于穿越企业防火墙
3. **代理支持**: 可通过 HTTP 代理建立连接
4. **浏览器兼容**: 可直接在 Web 应用中使用
5. **完整加密**: 所有数据都经过 TLS 加密传输

## 相关示例对比

| 示例 | 传输方式 | 加密 | 端口 | 适用场景 |
|------|---------|------|------|---------|
| [tcp](../tcp/) | TCP | 无 | 1883 | 内网/测试 |
| [ssl](../ssl/) | TCP + TLS | 是 | 8883 | 加密通信 |
| [ws](../ws/) | WebSocket | 无 | 8083 | 穿越代理 |
| **wss** | WebSocket + TLS | 是 | 8084 | **生产环境推荐** |

## 相关文档

- [coreMQTT 库文档](https://www.freertos.org/mqtt/index.html)
- [WebSocket 协议规范](https://datatracker.ietf.org/doc/html/rfc6455)
- [MQTT over WebSocket 规范](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718127)
- [nopoll 库文档](http://www.aspl.es/nopoll/)
- [mbedTLS 文档](https://mbed-tls.readthedocs.io/)

## 许可证

Copyright (c) 2025, LISTENAI
SPDX-License-Identifier: Apache-2.0
