# WebSocket 客户端示例

## 功能说明

此示例演示如何使用 LISA WebSocket 客户端库进行实时双向通信，包括：
- WebSocket 连接建立
- 文本消息发送与接收
- WebSocket 事件处理（连接/断开）
- 实时双向通信演示

示例通过 WiFi 连接到 WebSocket 服务器，建立 WebSocket 连接后循环发送测试消息并接收服务器响应，展示了 WebSocket 客户端的基本用法。

## 硬件连接

本示例使用芯片内部 WiFi 外设，无需额外接线。

**串口输出：**
- 串口 TX: PA3
- 波特率：921600

## 测试环境准备

### 1. 安装 Python 依赖

本测试服务器需要安装 `websocket-server` 库。

**Python 版本要求：** Python 3.6 或更高版本

**安装依赖：**

```bash
cd samples/network/websocket
pip3 install -r requirements.txt
```

### 2. 启动 WebSocket 测试服务器

在与设备**同一局域网**的 Linux/Mac 主机上运行测试服务器：

```bash
cd samples/network/websocket
python3 websocket-server.py
```

服务器启动后将显示：
```
WebSocket 服务器启动在 ws://0.0.0.0:9001/
等待客户端连接...
```

### 3. 配置 WiFi 和服务器地址

**重要：** 本示例强制要求在编译前配置 WiFi 和服务器信息，否则会编译失败。

在 `src/main.c` 中找到配置区域（第 22-31 行），**取消注释**并填写实际配置：

**修改前（默认状态）：**
```c
// ============================================================
// 重要：使用前请修改以下配置！
// ============================================================
// #define WS_TEST_SCHEME    // WebSocket 协议，通常为 "ws" 或 "wss"
// #define WS_TEST_HOST      // WebSocket 服务器 IP，例如："192.168.1.100"
// #define WS_TEST_PORT      // WebSocket 服务器端口，例如："9001"
// #define WS_TEST_PATH      // WebSocket 路径，例如："/"

// #define TARGET_WIFI_SSID   // 修改为目标 WiFi 的 SSID
// #define TARGET_WIFI_PWD     // 修改为目标 WiFi 的密码
```

**修改后（配置示例）：**
```c
// ============================================================
// 重要：使用前请修改以下配置！
// ============================================================
#define WS_TEST_SCHEME "ws"                  // WebSocket 协议
#define WS_TEST_HOST "192.168.1.100"         // WebSocket 服务器 IP
#define WS_TEST_PORT "9001"                  // WebSocket 服务器端口
#define WS_TEST_PATH "/"                     // WebSocket 路径

#define TARGET_WIFI_SSID "MyHomeWiFi"        // 你的 WiFi 名称
#define TARGET_WIFI_PWD "mypassword123"      // 你的 WiFi 密码
```

**操作步骤：**
1. 移除 `WS_TEST_SCHEME` 前的 `//` 注释符，填写为 "ws" 或 "wss"
2. 移除 `WS_TEST_HOST` 前的 `//` 注释符，填写实际服务器 IP
3. 移除 `WS_TEST_PORT` 前的 `//` 注释符，填写实际端口（通常为 "9001"）
4. 移除 `WS_TEST_PATH` 前的 `//` 注释符，填写 WebSocket 路径（通常为 "/"）
5. 移除 `TARGET_WIFI_SSID` 前的 `//` 注释符，填写实际 WiFi 名称
6. 移除 `TARGET_WIFI_PWD` 前的 `//` 注释符，填写实际 WiFi 密码

**获取服务器 IP 地址：**

在运行 `websocket-server.py` 的主机上执行：

```bash
# Linux/Mac
ifconfig | grep "inet "

# 或者
ip addr show | grep "inet "
```

查找类似 `192.168.x.x` 的局域网地址。

## 编译运行

### 编译

```{eval-rst}
.. include:: /sample_build.rst
```

### 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

系统启动后，终端将输出以下内容：

### 1. WiFi 连接日志

```
[INFO] WebSocket Test Starting...
[INFO] WiFi auto-connect started
[INFO] custom_get_wifi_mac: 0, XX:XX:XX:XX:XX:XX
[INFO] WiFi connection status: 1
[INFO] WiFi connected to AP
[INFO] WiFi event: module=1, id=4
[INFO] Got IP address
[INFO] WiFi connected and IP obtained, starting WebSocket test...
```

### 2. WebSocket 连接和数据交互

```
========================================
WebSocket 功能测试
========================================

[INFO] Connecting to WebSocket server: ws://192.168.1.100:9001/
[INFO] lisa ws connect

WebSocket 连接成功，开始发送测试数据...

[INFO] 发送消息 1/10: lisa_test
[INFO] ws recv data: Echo: lisa_test
[INFO] 发送消息 2/10: lisa_test
[INFO] ws recv data: Echo: lisa_test
[INFO] 发送消息 3/10: lisa_test
[INFO] ws recv data: Echo: lisa_test
...
[INFO] 发送消息 10/10: lisa_test
[INFO] ws recv data: Echo: lisa_test

========================================
WebSocket 测试完成
发送消息数: 10
========================================
```

### 3. 服务器端日志

服务器端将显示接收到的消息：

```
WebSocket 服务器启动在 ws://0.0.0.0:9001/
等待客户端连接...

客户端已连接
接收到消息: lisa_test
回显消息: Echo: lisa_test
接收到消息: lisa_test
回显消息: Echo: lisa_test
...
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_ws_init()` | 初始化 WebSocket 客户端 |
| `lisa_ws_connect()` | 连接到 WebSocket 服务器 |
| `lisa_ws_send_text()` | 发送文本消息 |
| `lisa_ws_disconnect()` | 断开 WebSocket 连接 |
| `lisa_ws_cleanup()` | 清理并释放 WebSocket 客户端资源 |
| `wifi_mgr_init()` | 初始化 WiFi 管理器 |
| `wifi_mgr_auto_connect_start()` | 启动自动连接 |
| `ls_event_register_cb()` | 注册 WiFi 事件回调 |

## 关键代码

### 1. WebSocket 连接初始化

使用 LISA WebSocket 库建立连接：

```c
// 配置 WebSocket 请求参数
lisa_ws_request_t lisa_ws_req;
lisa_ws_req.scheme = WS_TEST_SCHEME;   // "ws" 或 "wss"
lisa_ws_req.host = WS_TEST_HOST;       // 服务器 IP
lisa_ws_req.port = WS_TEST_PORT;       // 服务器端口
lisa_ws_req.path = WS_TEST_PATH;       // 路径
lisa_ws_req.timeout = WS_SEND_RETRY_DELAY;
lisa_ws_req.on_data = get_ws_data_cb;
lisa_ws_req.on_event = get_ws_event_cb;
lisa_ws_req.user = NULL;
lisa_ws_req.extra_header = NULL;

// 初始化 WebSocket 客户端
lisa_ws_t* lisa_ws_ins = lisa_ws_init(&lisa_ws_req);
if (lisa_ws_ins == NULL) {
    LOGE("lisa ws init failed");
    return -1;
}

// 连接到服务器
if (lisa_ws_connect(lisa_ws_ins) != 0) {
    LOGE("lisa evs websocket connect error");
    goto ERR;
}
```

### 2. WebSocket 事件回调

处理 WebSocket 连接状态变化：

```c
static void get_ws_event_cb(lisa_ws_event_t *event)
{
    g_event = event->what;
    if (event->what == LISA_WS_ON_CONNECTED) {
        LOGI("lisa ws connect");
    } else if (event->what == LISA_WS_ON_DISCONNECTED) {
        LOGI("lisa ws disconnect");
    } else {
        LOGI("event %d", event->what);
    }
}
```

### 3. WebSocket 数据接收回调

处理接收到的 WebSocket 消息：

```c
static void get_ws_data_cb(lisa_ws_data_t *data)
{
    LOGI("ws recv data: %s", data->buf);
}
```

### 4. WebSocket 消息发送

发送文本消息到服务器：

```c
// 等待 WebSocket 连接成功
int check_conn_count = 0;
while (g_event == LISA_WS_ON_DISCONNECTED) {
    lisa_thread_mdelay(WS_SEND_RETRY_SLEEP_TIME);
    check_conn_count++;
    if (check_conn_count >= (WS_SEND_RETRY_DELAY / WS_SEND_RETRY_SLEEP_TIME)) {
        LOGE("lisa ws connect timeout");
        goto ERR;
    }
}

// 连接成功后发送消息
int ws_test_count = 0;
while (1) {
    LOGI("发送消息 %d/%d: %s", ws_test_count + 1, WS_TEST_MAX_COUNT, WS_TEST_CONTENT);
    lisa_ws_send_text(lisa_ws_ins, WS_TEST_CONTENT);
    lisa_thread_mdelay(1000);
    ws_test_count++;
    if (ws_test_count >= WS_TEST_MAX_COUNT) {
        break;
    }
}
```

### 5. WiFi 连接和 IP 获取

等待 WiFi 连接成功并获取 IP 地址：

```c
// 注册 ls_event 回调监听 GOT_IP 事件
ls_event_init();
ls_event_register_cb(EVENT_WIFI, EVENT_WIFI_GOT_IP, app_wifi_event_cb, NULL);

// WiFi 事件回调
static int app_wifi_event_cb(void *arg, event_module_t event_module,
                             int event_id, void *event_data)
{
    switch (event_id) {
    case EVENT_WIFI_GOT_IP:
        LOGI("Got IP address");
        g_get_ip_success = true;
        break;
    }
    return 0;
}

// 等待 WiFi 连接和 IP 获取
static int wait_for_wifi_connection(void)
{
    while (timeout > 0) {
        if (g_wifi_connected && g_get_ip_success) {
            LOGI("WiFi connected and IP obtained, starting WebSocket test...");
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        timeout--;
    }
    return -1;
}
```

### 6. 资源清理

正确清理 WebSocket 资源：

```c
ERR:
    if (lisa_ws_ins != NULL) {
        lisa_ws_disconnect(lisa_ws_ins);
        lisa_ws_cleanup(lisa_ws_ins);
        lisa_ws_ins = NULL;
    }
    return ret;
```

## 配置说明

### WebSocket 请求参数

`lisa_ws_request_t` 结构体包含以下主要字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| `scheme` | `const char *` | WebSocket 协议（"ws" 或 "wss"） |
| `host` | `const char *` | 服务器主机地址 |
| `port` | `const char *` | 服务器端口号 |
| `path` | `const char *` | WebSocket 路径 |
| `timeout` | `int` | 连接超时时间（毫秒） |
| `on_data` | `lisa_ws_data_cb_t` | 数据接收回调函数 |
| `on_event` | `lisa_ws_event_cb_t` | 事件通知回调函数 |
| `extra_header` | `const char *` | 额外的 HTTP 头部（可选） |

### WebSocket 事件类型

| 事件 | 说明 |
|------|------|
| `LISA_WS_ON_CONNECTED` | WebSocket 连接已建立 |
| `LISA_WS_ON_DISCONNECTED` | WebSocket 连接已断开 |

### prj.conf 关键配置

```kconfig
# LWIP 网络栈
CONFIG_LWIP=y
CONFIG_LWIP_OPTION_LWIP_DNS=y
CONFIG_LWIP_OPTION_SO_REUSE=y

# WiFi 管理器
CONFIG_WIFI_MANAGER=y

# WebSocket 客户端
CONFIG_MODULE_NOPOLL=y
CONFIG_LISA_WEBSOCKET=y

# LISA 组件
CONFIG_LISA_OS=y
CONFIG_LISA_KV=y
CONFIG_LISA_NETWORK=y

# 堆内存配置
CONFIG_HEAP_SIZE=0x3C00         # 主堆: 15KB
CONFIG_PSRAM_HEAP_SIZE=0x5b0000 # PSRAM 堆: 5.6MB
```

## WebSocket 协议说明

### ws vs wss

- **ws://** - 未加密的 WebSocket 连接（类似 HTTP）
- **wss://** - 加密的 WebSocket 连接（类似 HTTPS，需要 TLS/SSL）

本示例默认使用 `ws://` 协议。如需使用 `wss://`，需要：
1. 修改 `WS_TEST_SCHEME` 为 "wss"
2. 确保服务器支持 TLS/SSL
3. 可能需要配置证书（根据服务器要求）

## 注意事项

1. **编译前配置（重要）**：首次编译前**必须**取消注释并修改 `src/main.c` 中的 WiFi 和服务器配置，否则会触发编译错误：
   ```
   main.c:35:2: error: #error "请修改 WS_TEST_SCHEME 为 WebSocket 协议（例如：ws 或 wss）"
   main.c:39:2: error: #error "请修改 WS_TEST_HOST 为实际的 WebSocket 服务器 IP（例如：192.168.1.100）"
   main.c:43:2: error: #error "请修改 WS_TEST_PORT 为实际的 WebSocket 服务器端口（例如：9001）"
   main.c:47:2: error: #error "请修改 WS_TEST_PATH 为 WebSocket 路径（例如：/）"
   main.c:51:2: error: #error "请修改 TARGET_WIFI_SSID 为实际的 WiFi SSID"
   main.c:55:2: error: #error "请修改 TARGET_WIFI_PWD 为实际的 WiFi 密码"
   ```
   这是为了防止使用默认配置导致连接失败。配置方法请参考上方的"配置 WiFi 和服务器地址"章节。

2. **网络环境**：确保设备与 WebSocket 服务器在同一局域网内，避免跨网段访问

3. **端口配置**：WebSocket 服务器默认使用 9001 端口，需填写为字符串格式："9001"

4. **路径配置**：WebSocket 路径通常为 "/"，根据实际服务器配置填写

5. **WiFi 配置**：将 `TARGET_WIFI_SSID` 和 `TARGET_WIFI_PWD` 修改为实际的 WiFi SSID 和密码

6. **连接超时**：WebSocket 连接超时设置为 10 秒，如果网络较慢可适当增加 `timeout` 值

7. **事件处理**：`on_event` 和 `on_data` 回调函数在 WebSocket 事件发生时被调用，需要正确处理

8. **资源清理**：使用完毕后必须调用 `lisa_ws_disconnect()` 和 `lisa_ws_cleanup()` 释放资源

9. **Python 版本**：测试服务器需要 Python 3.6 或更高版本，并安装 `websocket-server` 库

10. **消息格式**：当前示例仅发送文本消息，LISA WebSocket 也支持二进制数据传输

## 相关文档

- [LISA WiFi 组件文档](../../../components/lisa_wifi/README.md) - WiFi 初始化和连接
- [WiFi Manager 文档](../../../modules/wifi_manager/README.md) - WiFi 连接管理
- [LISA WebSocket 组件文档](../../../components/lisa_websocket/README.md) - WebSocket 客户端详细说明
