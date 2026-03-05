# HTTP 客户端基础示例

## 功能说明

此示例演示如何使用 LISA HTTP 客户端库进行基本的 HTTP 通信，包括：
- HTTP GET 请求
- HTTP POST 请求（普通表单数据）
- HTTP POST 请求（Chunked 传输编码）

示例通过 WiFi 连接到测试服务器，执行多个 HTTP 请求并接收响应，展示了 HTTP 客户端的基本用法。

## 硬件连接

本示例使用芯片内部 WiFi 外设，无需额外接线。

**串口输出：**
- 串口 TX: PA3
- 波特率：921600

## 测试环境准备

### 1. 启动 HTTP 测试服务器

在与设备**同一局域网**的 Linux/Mac 主机上运行测试服务器：

```bash
cd samples/network/http
python3 http-server.py
```

服务器启动后将显示：
```
Serving at http://0.0.0.0:8000 (IPv4)
```

### 2. 配置 WiFi 和服务器地址

**重要：** 本示例强制要求在编译前配置 WiFi 和服务器信息，否则会编译失败。

在 `src/main.c` 中找到配置区域（第 21-27 行），**取消注释**并填写实际配置：

**修改前（默认状态）：**
```c
// ============================================================
// 重要：使用前请修改以下配置！
// ============================================================
// #define SERVER_URL     // 修改为测试服务器的 URL，例如："http://192.168.1.100:8000"

// #define TARGET_WIFI_SSID   // 修改为目标 WiFi 的 SSID
// #define TARGET_WIFI_PWD     // 修改为目标 WiFi 的密码
```

**修改后（配置示例）：**
```c
// ============================================================
// 重要：使用前请修改以下配置！
// ============================================================
#define SERVER_URL "http://192.168.1.100:8000"  // 测试服务器的 URL

#define TARGET_WIFI_SSID "MyHomeWiFi"      // 你的 WiFi 名称
#define TARGET_WIFI_PWD "mypassword123"    // 你的 WiFi 密码
```

**操作步骤：**
1. 移除 `SERVER_URL` 前的 `//` 注释符
2. 将 `SERVER_URL` 后面修改为实际的服务器 URL（例如："http://192.168.1.100:8000"）
3. 移除 `TARGET_WIFI_SSID` 前的 `//` 注释符
4. 将 `TARGET_WIFI_SSID` 后面修改为实际的 WiFi 名称
5. 移除 `TARGET_WIFI_PWD` 前的 `//` 注释符
6. 将 `TARGET_WIFI_PWD` 后面修改为实际的 WiFi 密码

**获取服务器 IP 地址：**

在运行 `http-server.py` 的主机上执行：

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
[INFO] HTTP Test Starting...
[INFO] WiFi auto-connect started
[INFO] custom_get_wifi_mac: 0, XX:XX:XX:XX:XX:XX
[INFO] WiFi connection status: 1
[INFO] WiFi connected to AP
[INFO] WiFi event: module=1, id=4
[INFO] Got IP address
[INFO] WiFi connected and IP obtained, starting HTTP tests...
```

### 2. HTTP 测试输出

```
========================================
HTTP 功能测试
========================================

=== 测试 1: HTTP GET 请求 ===

http response : <html><p>Done</p></html>

=== 测试 2: HTTP POST 请求 ===

http response : <html><p>Done</p></html>

=== 测试 3: HTTP POST 请求 (Chunked 编码) ===

http response : <html><p>Done</p></html>

========================================
所有 HTTP 测试完成
========================================
```

### 3. 服务器端日志

服务器端将显示接收到的 POST 请求详情：

```
===== 收到 POST 请求 =====
路径: /foobar
头部:
  Host: 192.168.1.100:8000
  Content-Length: 6
Body 数据:
foobar

===== 收到 POST 请求 =====
路径: /chunked-test
头部:
  Host: 192.168.1.100:8000
  Transfer-Encoding: chunked
Body 数据:
6
foobar
7
chunked
4
last
0
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_http_init()` | 初始化 HTTP 客户端 |
| `lisa_http_perform()` | 执行 HTTP 请求 |
| `lisa_http_cleanup()` | 清理并释放 HTTP 客户端资源 |
| `wifi_mgr_init()` | 初始化 WiFi 管理器 |
| `wifi_mgr_auto_connect_start()` | 启动自动连接 |
| `ls_event_register_cb()` | 注册 WiFi 事件回调 |

## 关键代码

### 1. HTTP GET 请求

使用 LISA HTTP 库发送 GET 请求：

```c
lisa_http_request_t req;

memset(&req, 0, sizeof(lisa_http_request_t));
req.method = LISA_HTTP_GET;
req.url = SERVER_URL;
req.timeout = 10;
req.on_data = http_data_cb;
req.body = NULL;
req.body_len = 0;
req.headers = NULL;

http = lisa_http_init(&req);
if (!http) {
    LOGE("[http] init err\n");
    goto ERR;
}

if (lisa_http_perform(http) != 0) {
    LOGE("http get request info err..\n");
    goto ERR;
}

lisa_http_cleanup(http);
```

### 2. HTTP POST 请求（普通表单数据）

发送普通 POST 数据：

```c
memset(&req, 0, sizeof(lisa_http_request_t));
req.method = LISA_HTTP_POST;
req.url = SERVER_URL "/foobar";
req.timeout = 10;
req.on_data = http_data_cb;
req.body = "foobar";
req.body_len = strlen(req.body);
req.headers = NULL;

http = lisa_http_init(&req);
lisa_http_perform(http);
lisa_http_cleanup(http);
```

### 4. HTTP 响应回调

处理服务器响应数据：

```c
static void http_data_cb(lisa_http_data_t *data)
{
    if (!data || !data->buf) {
        LOGE("http data is null");
        return;
    } else if (data->len <= 0) {
        LOGE("http data len is 0");
        return;
    } else {
        LOGI("\nhttp response : %s\n", data->buf);
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
            LOGI("WiFi connected and IP obtained, starting HTTP tests...");
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        timeout--;
    }
    return -1;
}
```

## 配置说明

### HTTP 请求参数

`lisa_http_request_t` 结构体包含以下主要字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| `method` | `lisa_http_method_t` | HTTP 方法（GET/POST/PUT/DELETE 等） |
| `url` | `const char *` | 完整的 HTTP URL |
| `timeout` | `int` | 请求超时时间（秒） |
| `on_data` | `lisa_http_data_cb_t` | 响应数据回调函数 |
| `body` | `const char *` | 请求 Body 数据（POST/PUT） |
| `body_len` | `size_t` | Body 数据长度 |
| `headers` | `void *` | 自定义 HTTP 头部（可选） |

### prj.conf 关键配置

```kconfig
# LWIP 网络栈
CONFIG_LWIP=y
CONFIG_LWIP_OPTION_LWIP_DNS=y
CONFIG_LWIP_OPTION_SO_REUSE=y

# WiFi 管理器
CONFIG_WIFI_MANAGER=y

# HTTP 客户端
CONFIG_MODULE_HTTPCLIENT=y

# LISA 组件
CONFIG_LISA_OS=y
CONFIG_LISA_KV=y
CONFIG_LISA_NETWORK=y
CONFIG_LISA_HTTP=y

# 堆内存配置
CONFIG_HEAP_SIZE=0x3C00         # 主堆: 15KB
CONFIG_PSRAM_HEAP_SIZE=0x5b0000 # PSRAM 堆: 5.6MB
```

## HTTP 方法说明

LISA HTTP 库支持以下 HTTP 方法：

| 方法 | 枚举值 | 用途 |
|------|--------|------|
| GET | `LISA_HTTP_GET` | 获取资源 |
| POST | `LISA_HTTP_POST` | 提交数据 |
| PUT | `LISA_HTTP_PUT` | 更新资源 |
| DELETE | `LISA_HTTP_DELETE` | 删除资源 |
| HEAD | `LISA_HTTP_HEAD` | 获取响应头 |

## 注意事项

1. **编译前配置（重要）**：首次编译前**必须**取消注释并修改 `src/main.c` 中的 WiFi 和服务器配置，否则会触发编译错误：
   ```
   main.c:31:2: error: #error "请修改 SERVER_URL 为实际的测试服务器 URL（例如：http://192.168.1.100:8000）"
   main.c:35:2: error: #error "请修改 TARGET_WIFI_SSID 为实际的 WiFi SSID"
   main.c:39:2: error: #error "请修改 TARGET_WIFI_PWD 为实际的 WiFi 密码"
   ```
   这是为了防止使用默认配置导致连接失败。配置方法请参考上方的"配置 WiFi 和服务器地址"章节。

2. **网络环境**：确保设备与测试服务器在同一局域网内，避免跨网段访问

3. **服务器地址**：`SERVER_URL` 必须包含完整的 URL（协议 + IP + 端口），例如："http://192.168.1.100:8000"

4. **WiFi 配置**：将 `TARGET_WIFI_SSID` 和 `TARGET_WIFI_PWD` 修改为实际的 WiFi SSID 和密码

5. **超时设置**：HTTP 超时设置为 10 秒，如果网络较慢可适当增加 `req.timeout` 值

6. **响应处理**：`on_data` 回调函数会在接收到响应数据时被调用，可能会被多次调用（分块接收）

7. **资源清理**：每次 HTTP 请求完成后必须调用 `lisa_http_cleanup()` 释放资源，避免内存泄漏

8. **Python 版本**：测试服务器需要 Python 3.6 或更高版本

## 相关文档

- [LISA WiFi 组件文档](../../../components/lisa_wifi/README.md) - WiFi 初始化和连接
- [WiFi Manager 文档](../../../modules/wifi_manager/README.md) - WiFi 连接管理
- [LISA HTTP 组件文档](../../../components/lisa_http/README.md) - HTTP 客户端详细说明
