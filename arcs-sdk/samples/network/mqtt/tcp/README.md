# MQTT 客户端示例

基于 coreMQTT 库的 MQTT 客户端示例。

## 功能

- 连接到 MQTT Broker
- 订阅主题
- 发布消息
- 接收消息

## 配置

使用前请修改 `src/main.c` 中的以下配置：

```c
#define MQTT_BROKER_HOST   "broker.emqx.io"  // MQTT Broker 地址
#define MQTT_BROKER_PORT   1883              // MQTT Broker 端口
#define MQTT_CLIENT_ID     "arcs_mqtt_client"

#define TARGET_WIFI_SSID   "your_wifi_ssid"  // WiFi SSID
#define TARGET_WIFI_PWD    "your_wifi_pwd"   // WiFi 密码

#define MQTT_TOPIC_PUB     "arcs/test/pub"   // 发布主题
#define MQTT_TOPIC_SUB     "arcs/test/sub"   // 订阅主题
```


## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 运行

烧录固件后，设备会：
1. 连接 WiFi
2. 连接 MQTT Broker
3. 订阅 `arcs/test/sub` 主题
4. 向 `arcs/test/pub` 主题发布消息
5. 等待接收消息
6. 断开连接

## 测试方法

### 使用公共 MQTT Broker

推荐使用以下公共 MQTT Broker 进行测试：
- broker.emqx.io:1883 （推荐）
- test.mosquitto.org:1883

### 使用 MQTTX 工具测试

[MQTTX](https://mqttx.app/) 是一个跨平台的 MQTT 客户端工具，可以方便地进行测试。

#### 1. 安装 MQTTX CLI

```bash
curl -LO https://www.emqx.com/en/downloads/MQTTX/v1.12.1/mqttx-cli-linux-x64
sudo install ./mqttx-cli-linux-x64 /usr/local/bin/mqttx
```

#### 2. 订阅设备发布的主题

在电脑上运行以下命令，订阅设备将要发布消息的主题：

```bash
mqttx sub -h broker.emqx.io -p 1883 -t "arcs/test/pub"
```

#### 3. 运行设备固件

烧录并运行固件后，MQTTX 会收到设备发布的消息：

```
topic: arcs/test/pub, qos: 0, size: 16B
Hello from ARCS!
```

#### 4. 向设备发送消息

在另一个终端运行以下命令，向设备订阅的主题发送消息：

```bash
mqttx pub -h broker.emqx.io -p 1883 -t "arcs/test/sub" -m "Hello ARCS Girl"
```

设备端会打印接收到的消息：

```
Received PUBLISH: topic=arcs/test/sub, payload=Hello ARCS Girl
```
