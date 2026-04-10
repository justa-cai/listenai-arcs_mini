# LISA Modem 组件

ML307 4G/LTE 模块驱动组件，为 ARCS 平台提供基于 AT 命令的蜂窝网络通信能力，支持 TCP/UDP/SSL 连接、DNS 解析，并与 NetDev 抽象层集成实现多网卡管理。

## 功能特性

- **AT 命令通信**: 完整的 AT 命令发送与响应解析框架
- **URC 处理**: 自动检测和分发未经请求的结果码（Unsolicited Result Code）
- **TCP/SSL 连接**: 支持 TCP 和 SSL/TLS 安全连接
- **UDP 通信**: 支持 UDP 数据报收发
- **DNS 解析**: 通过 4G 模块进行域名解析
- **多连接支持**: 同时支持 6 个 TCP/UDP 连接（ID 0-5）
- **网络状态监控**: 实时监控网络注册状态和 IP 地址获取
- **电源管理**: 支持休眠模式和模块重启
- **NetDev 集成**: 与 SAL 层集成，提供标准 Socket API
- **线程安全**: 所有接口支持多任务并发访问

## 配置选项

在 `prj.conf` 中启用组件：

```kconfig
CONFIG_LISA_MODEM=y              # 启用 LISA Modem 组件
CONFIG_SAL_USING_POSIX=y         # 启用 SAL 套接字抽象层（可选）
CONFIG_LISA_NETWORK=y            # 启用 LISA 网络组件（可选，用于 NetDev 集成）
```

## API 接口

### 模块初始化

```c
/* 初始化 4G 模块（阻塞式，等待网络就绪） */
int lisa_modem_module_init(void);
```

### TCP 连接

```c
/* 创建 TCP 连接 */
ml307_tcp_t *ml307_tcp_create(at_uart_t *uart, int id);

/* 连接服务器 */
int ml307_tcp_connect(ml307_tcp_t *tcp, const char *host, uint16_t port);

/* 发送数据 */
int ml307_tcp_send(ml307_tcp_t *tcp, const char *data, size_t len);

/* 断开连接 */
int ml307_tcp_disconnect(ml307_tcp_t *tcp);

/* 销毁连接 */
void ml307_tcp_destroy(ml307_tcp_t *tcp);

/* 设置数据接收回调 */
void ml307_tcp_on_stream(ml307_tcp_t *tcp, ml307_on_stream_cb cb, void *user_data);

/* 设置断开连接回调 */
void ml307_tcp_on_disconnected(ml307_tcp_t *tcp, ml307_on_disconnected_cb cb, void *user_data);
```

### UDP 通信

```c
/* 创建 UDP 连接 */
ml307_udp_t *ml307_udp_create(at_uart_t *uart, int id);

/* 连接远程地址 */
int ml307_udp_connect(ml307_udp_t *udp, const char *host, uint16_t port);

/* 发送数据 */
int ml307_udp_send(ml307_udp_t *udp, const char *data, size_t len);

/* 关闭连接 */
int ml307_udp_close(ml307_udp_t *udp);

/* 销毁连接 */
void ml307_udp_destroy(ml307_udp_t *udp);
```

### SSL/TLS 连接

```c
/* 创建 SSL 连接 */
ml307_tcp_t *ml307_tcp_create_ssl(at_uart_t *uart, int id);

/* 连接服务器（使用 SSL） */
int ml307_tcp_connect(ml307_tcp_t *tcp, const char *host, uint16_t port);
```

### DNS 解析

```c
/* 域名解析 */
int ml307_dns_resolve(const char *hostname, char *ip_buf, size_t buf_len);
```

### 网络状态

```c
/* 获取网络状态 */
network_status_t ml307_wrapper_get_network_status(ml307_wrapper_t *wrapper);

/* 检查网络是否就绪 */
bool ml307_wrapper_is_network_ready(ml307_wrapper_t *wrapper);

/* 等待网络就绪 */
int ml307_network_check(ml307_wrapper_t *wrapper, uint32_t timeout_ms);
```

### AT 命令接口

```c
/* 发送 AT 命令 */
at_response_t *at_uart_send_command(const char *cmd, uint32_t timeout_ms, bool wait_ok);

/* 设置 URC 回调 */
void at_uart_register_urc_callback(const char *prefix, at_urc_callback_t cb, void *user_data);

/* 启用调试输出 */
void at_uart_set_debug(bool enable);
```

## 使用示例

### 基础初始化示例

```c
#include "lisa_modem.h"
#include "lisa_log.h"

#define LOG_TAG "modem_example"

int modem_init_example(void)
{
    LISA_LOGI(LOG_TAG, "Initializing 4G modem...");

    /* 初始化 4G 模块（阻塞等待网络就绪） */
    int ret = lisa_modem_module_init();
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Modem init failed: %d", ret);
        return -1;
    }

    LISA_LOGI(LOG_TAG, "4G modem initialized successfully");
    return 0;
}
```

### TCP 通信示例

```c
#include "ml307_tcp.h"
#include "lisa_log.h"

#define LOG_TAG "tcp_example"

/* 数据接收回调 */
static void on_data_received(const char *data, size_t len, void *user_data)
{
    LISA_LOGI(LOG_TAG, "Received %d bytes: %.*s", (int)len, (int)len, data);
}

/* 连接断开回调 */
static void on_disconnected(void *user_data)
{
    LISA_LOGI(LOG_TAG, "Connection closed");
}

int tcp_example(void)
{
    /* 1. 创建 TCP 连接实例 */
    ml307_tcp_t *tcp = ml307_tcp_create(NULL, 0);
    if (!tcp) {
        LISA_LOGE(LOG_TAG, "Failed to create TCP instance");
        return -1;
    }

    /* 2. 设置回调（连接前设置） */
    ml307_tcp_on_stream(tcp, on_data_received, NULL);
    ml307_tcp_on_disconnected(tcp, on_disconnected, NULL);

    /* 3. 连接服务器 */
    if (ml307_tcp_connect(tcp, "httpbin.org", 80) != 0) {
        LISA_LOGE(LOG_TAG, "Connect failed");
        ml307_tcp_destroy(tcp);
        return -1;
    }

    LISA_LOGI(LOG_TAG, "Connected to server");

    /* 4. 发送 HTTP 请求 */
    const char *request = "GET /get HTTP/1.1\r\n"
                          "Host: httpbin.org\r\n"
                          "Connection: close\r\n\r\n";

    if (ml307_tcp_send(tcp, request, strlen(request)) < 0) {
        LISA_LOGE(LOG_TAG, "Send failed");
    }

    /* 5. 等待响应（回调中处理） */
    lisa_os_thread_sleep(5000);

    /* 6. 断开连接并清理 */
    ml307_tcp_disconnect(tcp);
    ml307_tcp_destroy(tcp);

    return 0;
}
```

### UDP 通信示例

```c
#include "ml307_udp.h"
#include "lisa_log.h"

#define LOG_TAG "udp_example"

/* UDP 数据接收回调 */
static void on_udp_data(const char *data, size_t len, void *user_data)
{
    LISA_LOGI(LOG_TAG, "UDP received: %.*s", (int)len, data);
}

int udp_example(void)
{
    /* 1. 创建 UDP 实例 */
    ml307_udp_t *udp = ml307_udp_create(NULL, 1);
    if (!udp) {
        return -1;
    }

    /* 2. 设置回调 */
    ml307_udp_on_data(udp, on_udp_data, NULL);

    /* 3. 连接远程地址 */
    if (ml307_udp_connect(udp, "time.nist.gov", 37) != 0) {
        ml307_udp_destroy(udp);
        return -1;
    }

    /* 4. 发送数据 */
    ml307_udp_send(udp, "hello", 5);

    /* 5. 等待响应 */
    lisa_os_thread_sleep(3000);

    /* 6. 清理 */
    ml307_udp_close(udp);
    ml307_udp_destroy(udp);

    return 0;
}
```

### SSL/TLS 安全连接示例

```c
#include "ml307_tcp.h"
#include "lisa_log.h"

#define LOG_TAG "ssl_example"

int ssl_example(void)
{
    /* 1. 创建 SSL 连接实例 */
    ml307_tcp_t *ssl = ml307_tcp_create_ssl(NULL, 2);
    if (!ssl) {
        return -1;
    }

    /* 2. 连接 HTTPS 服务器 */
    if (ml307_tcp_connect(ssl, "httpbin.org", 443) != 0) {
        ml307_tcp_destroy(ssl);
        return -1;
    }

    LISA_LOGI(LOG_TAG, "SSL connected");

    /* 3. 发送 HTTPS 请求 */
    const char *request = "GET /get HTTP/1.1\r\n"
                          "Host: httpbin.org\r\n"
                          "Connection: close\r\n\r\n";

    ml307_tcp_send(ssl, request, strlen(request));

    /* 4. 等待响应 */
    lisa_os_thread_sleep(5000);

    /* 5. 清理 */
    ml307_tcp_disconnect(ssl);
    ml307_tcp_destroy(ssl);

    return 0;
}
```

### DNS 解析示例

```c
#include "ml307_dns.h"
#include "lisa_log.h"

#define LOG_TAG "dns_example"

int dns_example(void)
{
    char ip_buf[16];

    /* 解析域名 */
    if (ml307_dns_resolve("www.baidu.com", ip_buf, sizeof(ip_buf)) == 0) {
        LISA_LOGI(LOG_TAG, "Resolved IP: %s", ip_buf);
    } else {
        LISA_LOGE(LOG_TAG, "DNS resolve failed");
        return -1;
    }

    return 0;
}
```

### 与 NetDev 集成示例

```c
#include "netdev.h"
#include "lisa_modem.h"
#include "lisa_log.h"

#define LOG_TAG "netdev_modem"

/* 网络状态回调 */
static void on_modem_status(struct netdev *netdev, enum netdev_cb_type type)
{
    switch (type) {
    case NETDEV_CB_STATUS_LINK_UP:
        LISA_LOGI(LOG_TAG, "4G link up");
        break;
    case NETDEV_CB_STATUS_LINK_DOWN:
        LISA_LOGI(LOG_TAG, "4G link down");
        break;
    case NETDEV_CB_STATUS_INTERNET_UP:
        LISA_LOGI(LOG_TAG, "4G internet available");
        break;
    default:
        break;
    }
}

int netdev_modem_example(void)
{
    /* 1. 初始化网络设备子系统 */
    netdev_init();

    /* 2. 初始化 4G 模块 */
    lisa_modem_module_init();

    /* 3. 注册 4G 网络设备（优先级 500，低于 WiFi） */
    app_netdev_register("ml307", 500);

    /* 4. 设置状态回调 */
    struct netdev *modem_dev = netdev_get_by_name("ml307");
    if (modem_dev) {
        netdev_set_status_callback(modem_dev, on_modem_status);
    }

    /* 5. 现在可以使用标准 Socket API */
    /* SAL 层会自动选择可用的网络设备 */

    return 0;
}
```

## 架构说明

```
┌─────────────────────────────────────────────────────────┐
│                   应用层 (Application)                   │
│     HTTP Client / MQTT Client / Custom Protocols        │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│              ml307_wrapper.h/c (高级管理层)              │
│   - 模块初始化与重启                                     │
│   - 网络注册监控 (+CREG URC)                            │
│   - PDP 上下文管理 (+MIPCALL URC)                       │
│   - 连接工厂 (TCP/SSL)                                  │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│         ml307_tcp.h/c  &  ml307_udp.h/c (连接层)        │
│   - 连接生命周期管理 (connect/disconnect)               │
│   - 数据传输（自动 Hex 编码）                           │
│   - URC 处理 (MIPOPEN, MIPCLOSE, MIPSEND, MIPURC)      │
│   - 流数据与断开回调                                    │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│                at_uart.h/c (AT 命令层)                   │
│   - 命令/响应同步 (EventGroups)                         │
│   - URC 解析与分发                                      │
│   - 参数解析 (string/int/double)                        │
│   - 动态缓冲区管理                                      │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│              lisa_uart (ARCS SDK UART 驱动)              │
└─────────────────────────────────────────────────────────┘
```

## 支持的 AT 命令

| 命令 | 说明 |
|------|------|
| `AT+CREG` | 网络注册状态查询 |
| `AT+MIPCALL` | PDP 上下文激活/查询 |
| `AT+MIPOPEN` | 打开 TCP/UDP 连接 |
| `AT+MIPCLOSE` | 关闭连接 |
| `AT+MIPSEND` | 发送数据 |
| `AT+MDNSGIP` | DNS 域名解析 |
| `AT+CSQ` | 信号强度查询 |
| `AT+CPIN` | SIM 卡状态查询 |
| `AT+COPS` | 运营商查询 |

## 性能参数

| 参数 | 数值 |
|------|------|
| 模块初始化时间 | 3-5 秒 |
| 网络注册时间 | 5-30 秒（取决于信号） |
| TCP 连接时间 | 1-10 秒 |
| 数据发送延迟 | 100-500ms |
| 数据接收延迟 | 100-500ms |
| 最大连接数 | 6 个（ID 0-5） |
| 单包最大数据 | 730 字节 |

## 内存占用

| 模块 | 内存 |
|------|------|
| at_uart | ~8KB（缓冲区 + 实例） |
| ml307_tcp | ~2KB / 连接 |
| ml307_wrapper | ~1KB |
| **总计** | ~11KB + (2KB × 连接数) |

## 使用注意事项

1. **初始化顺序**: 必须先调用 `lisa_modem_module_init()` 初始化模块
2. **回调设置**: 必须在 `connect` 之前设置数据接收回调
3. **连接 ID**: 使用 0-5 的连接 ID，不同连接使用不同 ID
4. **数据分包**: 大数据自动分包（730 字节/包），无需手动处理
5. **Hex 编码**: 数据传输自动进行 Hex 编码/解码
6. **回调上下文**: URC 回调在 UART 任务上下文中执行，回调函数应尽快返回
7. **网络状态**: 发起连接前应确认网络已就绪
8. **资源释放**: 使用完毕后调用 `destroy` 函数释放资源
9. **调试模式**: 调用 `at_uart_set_debug(true)` 可查看 AT 命令交互日志
10. **线程安全**: 所有接口线程安全，可在多任务环境中使用

## 故障排查

### 模块无响应

```c
/* 启用调试查看 AT 交互 */
at_uart_set_debug(true);

/* 尝试重启模块 */
ml307_wrapper_reboot(modem);
lisa_os_thread_sleep(5000);
```

### 网络注册失败

```c
/* 检查 SIM 卡 */
at_uart_send_command("AT+CPIN?", 1000, true);

/* 检查信号强度 */
at_uart_send_command("AT+CSQ", 1000, true);

/* 检查运营商 */
at_uart_send_command("AT+COPS?", 1000, true);
```

### TCP 连接失败

- 确认网络已就绪：`ml307_wrapper_is_network_ready()`
- 检查连接 ID 是否冲突（0-5）
- 验证服务器地址和端口
- 信号弱时增加超时时间

## 文件说明

- `include/lisa_modem.h` - 模块主头文件
- `include/ml307_tcp.h` - TCP/SSL 连接接口
- `include/ml307_udp.h` - UDP 连接接口
- `include/ml307_wrapper.h` - 高级管理接口
- `include/at_uart.h` - AT 命令接口
- `src/ml307_tcp.c` - TCP/SSL 实现
- `src/ml307_udp.c` - UDP 实现
- `src/ml307_wrapper.c` - 模块管理实现
- `src/at_uart.c` - AT 命令实现
- `sal/netdev_at.c` - NetDev 适配层实现
- `Kconfig` - 组件配置选项
- `CMakeLists.txt` - 构建配置
