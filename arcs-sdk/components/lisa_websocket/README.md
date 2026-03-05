# LISA WebSocket 组件

基于 nopoll 库的 WebSocket 客户端组件，为 ARCS 平台提供统一的 WebSocket 通信接口，支持安全和非安全连接、事件驱动架构和异步数据处理。

## 功能特性

- **连接支持**: 支持 ws:// 和 wss:// 协议，提供安全和非安全连接
- **数据传输**: 支持文本、二进制和 TTS 三种数据类型传输
- **事件驱动**: 完整的事件回调机制，包括连接状态、错误处理和数据接收
- **异步处理**: 独立线程处理网络通信，支持消息队列和重试机制
- **心跳检测**: 可配置的 Ping/Pong 机制，实时监测连接状态
- **线程安全**: 支持多线程环境，内部使用互斥锁保护
- **配置灵活**: 丰富的配置选项，支持超时、重试、队列大小等参数调整
- **内存管理**: 自动内存管理，支持分片传输和大文件处理

## 配置选项

在 `prj.conf` 中启用组件：

```kconfig
CONFIG_LISA_WEBSOCKET=y                                          # 启用 WebSocket 组件
CONFIG_LISA_WEBSOCKET_SEND_RETRY_DELAY_NS=10000                  # 发送重试延迟（纳秒）
CONFIG_LISA_WEBSOCKET_SEND_RETRY_COUNT=10                        # 发送重试次数
CONFIG_LISA_WEBSOCKET_THREAD_STACK_SIZE=12288                    # WebSocket 线程栈大小
CONFIG_LISA_WEBSOCKET_QUEUE_COUNT=150                            # 消息队列大小
CONFIG_LISA_WEBSOCKET_PING_PONG_MSG=y                            # 启用心跳检测
CONFIG_LISA_WEBSOCKET_PING_PONG_TIMEOUT_MS=2000                  # 心跳超时时间（毫秒）
CONFIG_LISA_WEBSOCKET_PING_PONG_RETRY_CNT=5                      # 心跳重试次数
CONFIG_LISA_WEBSOCKET_FRAGMENT_RECV_TIMEOUT_MS=10000             # 分片接收超时时间
CONFIG_LISA_WEBSOCKET_FRAGMENT_SIZE=512                          # 消息分片大小
```

根据应用需求调整相关参数。

## API 接口

### 生命周期管理

```c
lisa_ws_t *lisa_ws_init(lisa_ws_request_t *req);
lisa_ws_err_e lisa_ws_connect(lisa_ws_t *ins);
lisa_ws_err_e lisa_ws_disconnect(lisa_ws_t *ins);
lisa_ws_err_e lisa_ws_cleanup(lisa_ws_t *ins);
```

### 数据传输接口

```c
lisa_ws_err_e lisa_ws_send_text(lisa_ws_t *ins, const uint8_t *text);
lisa_ws_err_e lisa_ws_send_binary(lisa_ws_t *ins, const void *buf, uint32_t len);
```

### 配置管理接口

```c
lisa_ws_err_e lisa_ws_cfg_get(lisa_ws_t *ws, lisa_ws_request_t *req);
lisa_ws_err_e lisa_ws_cfg_set(lisa_ws_t *ws, lisa_ws_request_t *req);
```

### 便利函数

```c
static inline lisa_ws_t *lisa_ws_new(void);
static inline lisa_ws_err_e lisa_ws_delete(lisa_ws_t *ins);
```

## 使用示例

### 基础 WebSocket 连接

```c
#include "lisa_websocket.h"

// WebSocket 事件回调函数
static void on_websocket_event(lisa_ws_event_t *event)
{
    switch (event->what) {
    case LISA_WS_ON_CONNECTED:
        LISA_INFO("WebSocket 连接成功");
        break;
    case LISA_WS_ON_DISCONNECTED:
        LISA_INFO("WebSocket 连接断开");
        break;
    case LISA_WS_ON_ERROR:
        LISA_INFO("WebSocket 发生错误");
        break;
    default:
        break;
    }
}

// WebSocket 数据接收回调函数
static void on_websocket_data(lisa_ws_data_t *data)
{
    switch (data->type) {
    case LISA_WS_TEXT:
        LISA_INFO("收到文本消息: %s", (char *)data->buf);
        break;
    case LISA_WS_BIN:
        LISA_INFO("收到二进制数据，长度: %d", data->len);
        break;
    default:
        break;
    }
}

int websocket_basic_example(void)
{
    // 1. 配置 WebSocket 连接参数
    lisa_ws_request_t req = {
        .scheme = (uint8_t *)"wss",
        .host = (uint8_t *)"echo.websocket.org",
        .path = (uint8_t *)"/",
        .port = (uint8_t *)"443",
        .timeout = 10000,
        .extra_header = NULL,
        .user = NULL,
        .on_event = on_websocket_event,
        .on_data = on_websocket_data,
    };

    // 2. 创建 WebSocket 实例
    lisa_ws_t *ws = lisa_ws_init(&req);
    if (!ws) {
        LISA_ERR("WebSocket 实例创建失败");
        return -1;
    }

    // 3. 连接到 WebSocket 服务器
    lisa_ws_err_e ret = lisa_ws_connect(ws);
    if (ret != LISA_WS_OK) {
        LISA_ERR("WebSocket 连接失败: %d", ret);
        lisa_ws_delete(ws);
        return -1;
    }

    // 4. 发送文本消息
    const char *message = "Hello, WebSocket!";
    ret = lisa_ws_send_text(ws, (const uint8_t *)message);
    if (ret != LISA_WS_OK) {
        LISA_ERR("发送文本消息失败: %d", ret);
    }

    // 5. 发送二进制数据
    uint8_t binary_data[] = {0x01, 0x02, 0x03, 0x04};
    ret = lisa_ws_send_binary(ws, binary_data, sizeof(binary_data));
    if (ret != LISA_WS_OK) {
        LISA_ERR("发送二进制数据失败: %d", ret);
    }

    // 6. 等待一段时间接收服务器的响应
    lisa_thread_delay(5000);  // 等待 5 秒

    // 7. 断开连接并清理资源
    lisa_ws_disconnect(ws);
    lisa_ws_delete(ws);

    return 0;
}
```

### 动态配置示例

```c
#include "lisa_websocket.h"

int dynamic_config_example(void)
{
    // 1. 使用默认配置创建实例
    lisa_ws_t *ws = lisa_ws_new();
    if (!ws) {
        return -1;
    }

    // 2. 动态设置配置
    lisa_ws_request_t new_config = {
        .scheme = (uint8_t *)"wss",
        .host = (uint8_t *)"api.example.com",
        .path = (uint8_t *)"/websocket",
        .port = (uint8_t *)"443",
        .timeout = 5000,
        .extra_header = "Authorization: Bearer token123\r\n",
        .user = NULL,
        .on_event = your_event_handler,
        .on_data = your_data_handler,
    };

    lisa_ws_err_e ret = lisa_ws_cfg_set(ws, &new_config);
    if (ret != LISA_WS_OK) {
        LISA_ERR("设置配置失败");
        lisa_ws_delete(ws);
        return -1;
    }

    // 3. 连接并使用
    ret = lisa_ws_connect(ws);
    if (ret == LISA_WS_OK) {
        // 发送消息
        lisa_ws_send_text(ws, (const uint8_t *)"Hello from dynamic config");

        // 获取当前配置（用于验证或调试）
        lisa_ws_request_t current_config;
        if (lisa_ws_cfg_get(ws, &current_config) == LISA_WS_OK) {
            LISA_INFO("当前主机: %s", current_config.host);
        }

        // 断开连接
        lisa_ws_disconnect(ws);
    }

    // 4. 清理
    lisa_ws_delete(ws);
    return 0;
}
```

## 事件类型说明

### WebSocket 连接事件

| 事件类型 | 说明 | 触发时机 |
|---------|------|---------|
| `LISA_WS_ON_ERROR` | 发生错误 | 网络错误、协议错误等 |
| `LISA_WS_ON_HEADER` | 收到头部信息 | WebSocket 握手期间（可能多次） |
| `LISA_WS_ON_CONNECTED` | 连接建立成功 | WebSocket 握手完成 |
| `LISA_WS_ON_CONNECTING` | 连接进行中 | 开始建立连接 |
| `LISA_WS_ON_DISCONNECTED` | 连接断开 | 连接关闭或超时 |

### 数据传输类型

| 数据类型 | 说明 | 编码格式 |
|---------|------|---------|
| `LISA_WS_TEXT` | 文本数据 | UTF-8 字符串 |
| `LISA_WS_BIN` | 二进制数据 | 原始字节数据 |

## 错误代码说明

| 错误代码 | 说明 | 处理建议 |
|---------|------|---------|
| `LISA_WS_OK` | 操作成功 | - |
| `LISA_WS_COMMON_ERR` | 一般错误 | 检查参数和网络状态 |
| `LISA_WS_DISCONNECT` | 连接断开 | 重新建立连接 |

## 注意事项

1. **必须设置回调函数**: `on_event` 和 `on_data` 回调函数必须设置，否则无法处理连接事件和接收数据
2. **连接超时设置**: 合理设置 `timeout` 参数，避免连接时间过长影响应用响应
3. **内存管理**: 发送的数据在函数返回后即可释放，内部会自动管理内存复制
4. **心跳检测**: 启用 Ping/Pong 机制可以及时检测连接状态，但会增加网络开销
5. **队列大小**: 根据应用消息频率调整 `QUEUE_COUNT`，避免消息丢失
6. **分片大小**: `FRAGMENT_SIZE` 应根据网络环境调整，建议在 512-1024 字节之间
7. **线程安全**: WebSocket 组件内部使用独立线程处理网络通信，API 调用是线程安全的
8. **错误处理**: 所有 API 调用都应检查返回值，并进行相应的错误处理
9. **资源清理**: 应用退出前必须调用 `lisa_ws_delete()` 清理资源，避免内存泄漏
10. **重连机制**: 连接断开后需要应用层实现重连逻辑，组件不提供自动重连功能
11. **大文件传输**: 大于分片大小的消息会自动分片发送，确保网络稳定性
12. **SSL/TLS**: 使用 wss:// 协议时，确保设备时间正确，避免证书验证失败

## 文件说明

- `lisa_websocket.h` - WebSocket 组件头文件，包含 API 接口和数据结构定义
- `lisa_websocket.c` - WebSocket 组件实现文件，基于 nopoll 库
- `Kconfig` - 组件配置选项定义文件
- `CMakeLists.txt` - 组件构建配置文件
