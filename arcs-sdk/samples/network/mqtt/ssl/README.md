# MQTT SSL/TLS 示例

本示例演示如何在 ARCS 平台上使用 coreMQTT 库通过 SSL/TLS 加密连接到 MQTT broker。

## 功能特性

- 使用 mbedTLS 实现 SSL/TLS 加密连接
- 支持安全的 MQTT 通信(端口 8883)
- WiFi 自动连接
- MQTT 发布/订阅功能
- 完整的 SSL/TLS 握手流程
- 非阻塞 I/O 操作

## 硬件要求

- ARCS 开发板
- SD 卡(用于文件系统)
- WiFi 网络环境

## 配置说明

在使用前,请修改 [src/main.c](src/main.c) 中的以下配置:

```c
// MQTT Broker 配置
#define MQTT_BROKER_HOST   "broker.emqx.io"  // MQTT broker 地址
#define MQTT_BROKER_PORT   8883              // SSL 端口(标准为 8883)
#define MQTT_CLIENT_ID     "arcs_mqtt_ssl_client"

// WiFi 配置
#define TARGET_WIFI_SSID   "your_wifi_ssid"  // 修改为你的WiFi名称
#define TARGET_WIFI_PWD    "your_wifi_pwd"   // 修改为你的WiFi密码

// MQTT 主题
#define MQTT_TOPIC_PUB     "arcs/test/pub"   // 发布主题
#define MQTT_TOPIC_SUB     "arcs/test/sub"   // 订阅主题
```

## SSL/TLS 配置

### 证书验证模式

当前配置为**不验证服务器证书**模式(适用于测试):

```c
mbedtls_ssl_conf_authmode(&pNetworkContext->conf, MBEDTLS_SSL_VERIFY_NONE);
```

### 生产环境配置

在生产环境中,**强烈建议启用证书验证**:

1. 修改验证模式:
```c
mbedtls_ssl_conf_authmode(&pNetworkContext->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
```

2. 加载 CA 证书:
```c
/* 加载根证书 */
const char *ca_cert_pem = "-----BEGIN CERTIFICATE-----\n"
                          "...\n"
                          "-----END CERTIFICATE-----\n";

ret = mbedtls_x509_crt_parse(&pNetworkContext->cacert,
                              (const unsigned char *)ca_cert_pem,
                              strlen(ca_cert_pem) + 1);

mbedtls_ssl_conf_ca_chain(&pNetworkContext->conf, &pNetworkContext->cacert, NULL);
```

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 运行效果

程序启动后,将依次执行以下步骤:

1. **WiFi 连接**: 自动连接到配置的 WiFi 网络
2. **SSL/TLS 连接**: 建立到 MQTT broker 的加密连接
3. **MQTT 连接**: 通过 SSL 通道建立 MQTT 连接
4. **订阅主题**: 订阅指定的 MQTT 主题
5. **发布消息**: 发布测试消息
6. **接收消息**: 持续监听和处理接收的消息

### 预期日志输出

```
[mqtt-ssl-test] MQTT SSL/TLS Test Starting...
[mqtt-ssl-test] WiFi connected and IP obtained
========================================
MQTT SSL/TLS 功能测试
========================================

[mqtt-ssl-test] Initializing SSL/TLS connection to broker.emqx.io:8883...
[mqtt-ssl-test] TCP connection established
[mqtt-ssl-test] Performing SSL/TLS handshake...
[mqtt-ssl-test] SSL/TLS handshake completed successfully
[mqtt-ssl-test] Connected to MQTT broker broker.emqx.io:8883 via SSL/TLS

=== 测试 1: MQTT CONNECT ===
[mqtt-ssl-test] MQTT Connected successfully

=== 测试 2: MQTT SUBSCRIBE ===
[mqtt-ssl-test] Subscribe request sent for topic: arcs/test/sub

=== 测试 3: MQTT PUBLISH ===
[mqtt-ssl-test] Published message to topic: arcs/test/pub

=== 测试 4: 接收消息 ===
[mqtt-ssl-test] Received PUBLISH: topic=arcs/test/sub, payload=...
```

## 主要差异(相比 TCP 版本)

| 特性 | TCP 版本 | SSL 版本 |
|-----|---------|---------|
| 端口 | 1883 | 8883 |
| 加密 | 无 | TLS 1.2+ |
| 库依赖 | lwIP sockets | mbedTLS |
| 连接建立 | 直接 TCP | TCP + SSL握手 |
| 数据传输 | socket send/recv | mbedtls_ssl_write/read |
| 缓冲区大小 | 1024 字节 | 2048 字节(SSL开销) |

## mbedTLS 配置 (prj.conf)

```ini
# mbedTLS SSL/TLS支持
CONFIG_MBEDTLS=y
CONFIG_MBEDTLS_BUILTIN=y
CONFIG_MBEDTLS_ENABLE_HEAP=y
CONFIG_MBEDTLS_HEAP_SIZE=0x8000

# mbedTLS功能配置
CONFIG_MBEDTLS_SSL_PROTO_TLS1_2=y
CONFIG_MBEDTLS_KEY_EXCHANGE_RSA_ENABLED=y
CONFIG_MBEDTLS_SSL_CLI_C=y
CONFIG_MBEDTLS_SSL_TLS_C=y
CONFIG_MBEDTLS_X509_CRT_PARSE_C=y
CONFIG_MBEDTLS_PEM_PARSE_C=y
```

## 调试技巧

### 启用 mbedTLS 调试输出

代码中已包含调试回调函数:

```c
static void my_debug(void *ctx, int level, const char *file, int line, const char *str)
{
    LOGI("%s:%04d: %s", file, line, str);
}

mbedtls_ssl_conf_dbg(&pNetworkContext->conf, my_debug, NULL);
```

### 常见错误码

- `-0x7780`: SSL - Memory allocation failed
- `-0x7200`: SSL - Fatal handshake failure
- `-0x7100`: SSL - Bad input data
- `-0x6800`: SSL - Certificate verification failed

使用 `mbedtls_strerror()` 可以获取详细错误信息。

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
mqttx sub -h broker.emqx.io -p 8883 -t "arcs/test/pub" -l mqtts
```

**预期结果:**
```
✔ Connected
✔ Subscribed to arcs/test/pub
topic: arcs/test/pub, qos: 0, size: 24B
Hello from ARCS via SSL!
```

设备会定期发布消息到 `arcs/test/pub` 主题,MQTTX 客户端会接收并显示这些消息。

#### 测试场景 2: 向设备发布消息

在电脑上运行以下命令,向设备订阅的主题发送消息:

```bash
mqttx pub -h broker.emqx.io -p 8883 -t "arcs/test/sub" -m "Hello ARCS Girl" -l mqtts
```

**预期结果:**

MQTTX 客户端输出:
```
✔ Connected
✔ Message published
```

设备串口日志输出:
```
I/mqtt-ssl-test [1034:43:49.966 1 main] Received PUBLISH: topic=arcs/test/sub, payload=Hello ARCS Girl
```

#### MQTTX 参数说明

| 参数 | 说明 | 示例值 |
|-----|------|--------|
| `-h` | MQTT broker 地址 | `broker.emqx.io` |
| `-p` | 端口号 | `8883` (SSL端口) |
| `-t` | 主题名称 | `arcs/test/pub` |
| `-m` | 消息内容 | `"Hello ARCS Girl"` |
| `-l` | 协议类型 | `mqtts` (MQTT over SSL/TLS) |
| `-q` | QoS 级别 (可选) | `0`, `1`, `2` |
| `-u` | 用户名 (可选) | `username` |
| `-P` | 密码 (可选) | `password` |

#### 完整测试流程

1. **启动设备**: 编译并运行 SSL MQTT 示例程序
2. **订阅测试**: 在电脑上运行 MQTTX 订阅命令,验证能收到设备消息
3. **发布测试**: 在电脑上运行 MQTTX 发布命令,验证设备能收到消息
4. **双向通信**: 同时开启订阅和发布,观察双向消息传输

### 使用 Mosquitto 客户端测试

如果你已经安装了 Mosquitto 客户端工具:

```bash
# 订阅设备发布的消息
mosquitto_sub -h broker.emqx.io -p 8883 -t "arcs/test/pub" --capath /etc/ssl/certs/

# 向设备发布消息
mosquitto_pub -h broker.emqx.io -p 8883 -t "arcs/test/sub" -m "Hello from Mosquitto" --capath /etc/ssl/certs/
```

### 测试 Checklist

- [ ] WiFi 连接成功并获取 IP 地址
- [ ] SSL/TLS 握手成功完成
- [ ] MQTT 连接建立成功
- [ ] 主题订阅成功
- [ ] 设备能够成功发布消息
- [ ] 外部客户端能够接收到设备发布的消息
- [ ] 设备能够接收外部客户端发布的消息
- [ ] 消息内容正确无误
- [ ] 程序稳定运行,无异常断线

## 测试建议

1. **先测试 TCP 版本**: 确保基本的 MQTT 功能正常
2. **检查内存配置**: SSL/TLS 需要更多内存(已配置 32KB mbedTLS堆)
3. **验证网络连接**: 确保可以访问目标 broker 的 8883 端口
4. **使用公共 broker 测试**: 如 `broker.emqx.io`, `test.mosquitto.org`
5. **使用专业工具**: 推荐使用 MQTTX 或 Mosquitto 客户端进行测试

## 公共 MQTT Broker

| Broker | 地址 | SSL端口 | 证书验证 |
|--------|------|---------|---------|
| EMQX | broker.emqx.io | 8883 | 可选 |
| Eclipse | mqtt.eclipseprojects.io | 8883 | 需要 |
| HiveMQ | broker.hivemq.com | 8883 | 可选 |
| Mosquitto | test.mosquitto.org | 8883 | 需要 |

## 故障排查

### SSL 握手失败

1. 检查 broker 是否支持 TLS 1.2
2. 检查系统时间是否正确(影响证书验证)
3. 增加 mbedTLS 堆大小

### 内存不足

1. 增加 `CONFIG_MBEDTLS_HEAP_SIZE`
2. 增加 `CONFIG_PSRAM_HEAP_SIZE`
3. 减少 `NETWORK_BUFFER_SIZE`

### 连接超时

1. 检查防火墙是否阻止 8883 端口
2. 增加超时时间
3. 检查 DNS 解析是否正常

## 相关文档

- [coreMQTT 库文档](https://www.freertos.org/mqtt/index.html)
- [mbedTLS 文档](https://mbed-tls.readthedocs.io/)
- [MQTT 规范](https://mqtt.org/mqtt-specification/)

## 许可证

Copyright (c) 2025, LISTENAI
SPDX-License-Identifier: Apache-2.0
