# MQTT Agent 示例

本示例展示了如何使用 coreMQTT-Agent 库实现线程安全的 MQTT 客户端应用。

## 功能特性

- 基于 coreMQTT-Agent 的线程安全 MQTT 操作
- 使用 FreeRTOS 队列进行线程间消息传递
- 支持异步订阅和发布操作
- 命令完成回调机制
- 多任务环境下的 MQTT 通信

## coreMQTT-Agent 简介

coreMQTT-Agent 是对 coreMQTT 库的线程安全封装,主要特性包括:

### 1. 线程安全

- 所有 MQTT 操作通过命令队列序列化执行
- 避免多线程环境下的竞态条件
- 支持从多个任务中安全调用 MQTT API

### 2. 异步操作模型

- 订阅/发布操作不会阻塞调用者
- 通过回调函数通知操作完成
- 可使用信号量实现同步等待

### 3. 专用 Agent 任务

- 运行 `MQTTAgent_CommandLoop()` 处理所有 MQTT 操作
- 统一管理网络收发和 MQTT 协议处理
- 简化多线程 MQTT 应用开发

## 架构设计

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  Publisher  │     │ Subscriber  │     │  Other Task │
│    Task     │     │    Task     │     │             │
└──────┬──────┘     └──────┬──────┘     └──────┬──────┘
       │                   │                   │
       │   Command Queue   │   Command Queue   │
       ├───────────────────┼───────────────────┤
       │                   │                   │
       ▼                   ▼                   ▼
┌────────────────────────────────────────────────────┐
│           MQTT Agent Task (Command Loop)           │
│  ┌──────────────────────────────────────────────┐  │
│  │  MQTTAgent_CommandLoop()                     │  │
│  │  - 接收命令队列中的命令                       │  │
│  │  - 执行 MQTT 操作 (Subscribe/Publish/etc)    │  │
│  │  - 处理网络事件和消息                         │  │
│  │  - 调用完成回调                               │  │
│  └──────────────────────────────────────────────┘  │
└──────────────────────┬─────────────────────────────┘
                       │
                       ▼
              ┌─────────────────┐
              │  MQTT Broker    │
              └─────────────────┘
```

## 核心 API

### Agent 初始化

```c
MQTTStatus_t MQTTAgent_Init(
    MQTTAgentContext_t * pMqttAgentContext,
    const MQTTAgentMessageInterface_t * pMsgInterface,
    const MQTTFixedBuffer_t * pNetworkBuffer,
    const TransportInterface_t * pTransportInterface,
    MQTTGetCurrentTimeFunc_t getCurrentTimeMs,
    IncomingPubCallback_t incomingCallback,
    void * pIncomingPacketContext
);
```

### Agent 命令循环 (在专用任务中运行)

```c
MQTTStatus_t MQTTAgent_CommandLoop(MQTTAgentContext_t * pMqttAgentContext);
```

### 线程安全的订阅操作

```c
MQTTStatus_t MQTTAgent_Subscribe(
    const MQTTAgentContext_t * pMqttAgentContext,
    MQTTAgentSubscribeArgs_t * pSubscriptionArgs,
    MQTTAgentCommandInfo_t * pCommandInfo
);
```

### 线程安全的发布操作

```c
MQTTStatus_t MQTTAgent_Publish(
    const MQTTAgentContext_t * pMqttAgentContext,
    MQTTPublishInfo_t * pPublishInfo,
    MQTTAgentCommandInfo_t * pCommandInfo
);
```

## 示例代码说明

### 1. 消息接口实现

示例实现了 `MQTTAgentMessageInterface_t` 结构,使用 FreeRTOS 队列:

```c
MQTTAgentMessageInterface_t messageInterface = {
    .pMsgCtx = &g_command_queue_context,
    .send = agent_send_command,        // 发送命令到队列
    .recv = agent_recv_command,        // 从队列接收命令
    .getCommand = agent_get_command,   // 获取空闲命令结构
    .releaseCommand = agent_release_command  // 释放命令结构
};
```

### 2. Agent 任务

运行 `MQTTAgent_CommandLoop()` 的专用任务:

```c
static void mqtt_agent_task(void *arg)
{
    MQTTStatus_t status = MQTTAgent_CommandLoop(&g_agent_context);
    // 命令循环退出表示连接断开或发生错误
}
```

### 3. 异步订阅

从任意任务中调用,不会阻塞:

```c
MQTTAgentSubscribeArgs_t subscribeArgs = {
    .pSubscribeInfo = subscribeInfo,
    .numSubscriptions = 1
};

MQTTAgentCommandInfo_t commandInfo = {
    .cmdCompleteCallback = subscribe_command_callback,
    .pCmdCompleteCallbackContext = &subscribe_sem
};

MQTTAgent_Subscribe(&g_agent_context, &subscribeArgs, &commandInfo);
```

### 4. 异步发布

支持 QoS 0/1/2,使用回调通知完成:

```c
MQTTPublishInfo_t publishInfo = {
    .qos = MQTTQoS1,
    .pTopicName = "/test/topic",
    .topicNameLength = strlen("/test/topic"),
    .pPayload = "Hello from Agent!",
    .payloadLength = strlen("Hello from Agent!")
};

MQTTAgentCommandInfo_t commandInfo = {
    .cmdCompleteCallback = publish_command_callback,
    .pCmdCompleteCallbackContext = &publish_sem
};

MQTTAgent_Publish(&g_agent_context, &publishInfo, &commandInfo);
```

### 5. 命令完成回调

操作完成后在 Agent 任务上下文中调用:

```c
static void subscribe_command_callback(MQTTAgentCommandContext_t *pCommandContext,
                                       MQTTAgentReturnInfo_t *pReturnInfo)
{
    SemaphoreHandle_t sem = (SemaphoreHandle_t)pCommandContext;

    if (pReturnInfo->returnCode == MQTTSuccess) {
        CLOG("Subscribe command succeeded");
    } else {
        CLOG("Subscribe command failed: %d", pReturnInfo->returnCode);
    }

    xSemaphoreGive(sem);  // 通知等待的任务
}
```

## 配置说明

### prj.conf 配置

```ini
# 启用 coreMQTT 基础库
CONFIG_SDK_MODULE_COREMQTT=y

# 启用 coreMQTT-Agent (线程安全封装)
CONFIG_SDK_MODULE_COREMQTT_AGENT=y
```

### 网络配置

修改 `main.c` 中的连接参数:

```c
#define MQTT_BROKER_ADDR   "broker.emqx.io"  // MQTT Broker 地址
#define MQTT_BROKER_PORT   "1883"            // MQTT Broker 端口
#define MQTT_CLIENT_ID     "arcs_mqtt_agent_client"  // 客户端 ID
```

### 编译

```{eval-rst}
.. include:: /sample_build.rst
```

### 配置 WiFi

在设备串口终端中执行:

```bash
# 配置 WiFi SSID
kv set wifi_ssid "your_wifi_name"

# 配置 WiFi 密码
kv set wifi_passwd "your_wifi_password"

# 重启设备
reboot
```

### 运行效果

设备启动后会自动:

1. 初始化文件系统
2. 连接 WiFi 网络
3. 连接到 MQTT Broker
4. 启动 Agent 任务
5. 订阅 `/test/topic` 主题
6. 定期发布消息到 `/test/topic`

## 与标准 coreMQTT 的对比

| 特性 | coreMQTT | coreMQTT-Agent |
|------|----------|----------------|
| 线程安全 | 否,需要手动加锁 | 是,内置线程安全 |
| 阻塞模式 | 同步阻塞 | 异步非阻塞 |
| 多任务支持 | 需要手动同步 | 原生支持 |
| 复杂度 | 较低 | 中等 |
| 适用场景 | 单线程应用 | 多线程 RTOS 应用 |
| 性能开销 | 较小 | 略高(队列和回调) |

## 测试方法

### 使用 MQTTX 订阅消息

```bash
# 订阅设备发布的消息
mqttx sub -h broker.emqx.io -p 1883 -t "arcs/test/pub"
```

### 使用 MQTTX 发布消息

```bash
# 向设备发布消息
mqttx pub -h broker.emqx.io -p 1883 -t "arcs/test/sub" -m "Hello ARCS Mqtt Agent"
```

设备会收到消息并在串口打印。

## 注意事项

1. **任务优先级**: Agent 任务应有足够高的优先级,确保及时处理网络事件
2. **栈大小**: Agent 任务需要较大的栈空间(建议 ≥ 4KB)
3. **队列深度**: 命令队列深度根据并发命令数量调整
4. **回调上下文**: 回调函数在 Agent 任务中执行,避免长时间阻塞操作
5. **命令池**: 示例使用静态命令池,可根据需要调整大小

## 故障排除

### 编译错误: 找不到 core_mqtt_agent.h

确保 `prj.conf` 中启用了:
```ini
CONFIG_SDK_MODULE_COREMQTT_AGENT=y
```

### Agent 命令循环退出

检查:
- 网络连接是否稳定
- Broker 地址和端口是否正确
- Keep-alive 超时设置是否合理

### 订阅/发布超时

- 检查命令队列是否已满
- 确认 Agent 任务正常运行
- 验证网络连接状态

## 参考资料

- [coreMQTT-Agent GitHub](https://github.com/FreeRTOS/coreMQTT-Agent)
- [coreMQTT GitHub](https://github.com/FreeRTOS/coreMQTT)
- [MQTT 协议规范](https://mqtt.org/mqtt-specification/)
