# 4G Modem 模块示例

## 功能说明

本示例演示如何使用 LISA Modem 模块（ML307 4G 模块）进行网络通信，包括：
- 4G 模块初始化
- DNS 域名解析
- TCP 连接建立
- TCP 数据发送和接收
- 连接关闭和资源清理

## 硬件连接

本示例使用 ML307 4G 模块通过 UART 与主控芯片通信。

**UART 连接：**
- 默认使用 `uart1` 设备
- 需要根据实际硬件连接修改 `MODEM_UART_DEVICE` 宏

**4G 模块：**
- 确保 4G 模块已正确供电
- 确保 SIM 卡已正确插入且开通数据业务
- 确保天线已正确连接

**串口输出：**
- 串口 TX: PA3
- 波特率：921600

## 示例内容

1. 初始化文件系统和 KV 存储
2. 初始化 4G 模块并等待网络注册
3. 通过 4G 模块解析域名获取 IP 地址
4. 建立 TCP 连接到测试服务器
5. 发送 HTTP GET 请求
6. 接收并显示服务器响应
7. 关闭连接

## 配置修改

使用前请修改 `src/main.c` 中的配置：

```c
// 4G 模块使用的 UART 设备
#define MODEM_UART_DEVICE   "uart1"

// 测试服务器配置
#define TEST_SERVER_HOST    "httpbin.org"
#define TEST_SERVER_PORT    80
```

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

系统启动后，终端将输出以下内容：

```
[I][modem] === 4G Modem Example ===
[I][modem] Initializing 4G modem module...
[I][lisa_modem] 4G module initialized successfully
[I][modem] 4G modem initialized successfully
[I][modem] Waiting for network registration...
[I][modem] === DNS Resolve Test ===
[I][modem] Resolving domain: httpbin.org
[I][modem] Resolved IP: 54.208.105.16
[I][modem] === TCP Communication Test ===
[I][modem] Step 1: Creating TCP socket...
[I][modem] TCP socket created: id=0
[I][modem] Step 2: Connecting to httpbin.org:80...
[I][modem] Connected to server successfully
[I][modem] Step 3: Sending HTTP request...
[I][modem] Sent 58 bytes
[I][modem] Step 4: Receiving response...
[I][modem] Received 512 bytes:
[I][modem] HTTP/1.1 200 OK...
[I][modem] Step 5: Closing connection...
[I][modem] Connection closed
[I][modem] === Example completed ===
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_modem_module_init()` | 初始化 4G 模块 |
| `lisa_modem_dns_resolve()` | DNS 域名解析 |
| `lisa_modem_tcp_socket()` | 创建 TCP socket |
| `lisa_modem_tcp_connect()` | 建立 TCP 连接 |
| `lisa_modem_tcp_send()` | 发送 TCP 数据 |
| `lisa_modem_tcp_recv()` | 接收 TCP 数据 |
| `lisa_modem_tcp_closesocket()` | 关闭 TCP 连接 |
| `lisa_modem_tcp_deinit()` | 释放 TCP socket |

## 关键代码

### 1. 4G 模块初始化

```c
/* 初始化 4G 模块 */
if (!lisa_modem_module_init(MODEM_UART_DEVICE)) {
    LISA_LOGE(LOG_TAG, "Error: Failed to initialize 4G modem");
    return -1;
}

/* 等待网络注册完成 */
vTaskDelay(pdMS_TO_TICKS(3000));
```

### 2. DNS 解析

```c
char ip_addr[64] = {0};

if (!lisa_modem_dns_resolve(domain, ip_addr, sizeof(ip_addr))) {
    LISA_LOGE(LOG_TAG, "Error: DNS resolve failed");
    return -1;
}

LISA_LOGI(LOG_TAG, "Resolved IP: %s", ip_addr);
```

### 3. TCP 通信

```c
/* 创建 TCP socket */
int tcp_id = lisa_modem_tcp_socket(false);

/* 连接到服务器 */
if (!lisa_modem_tcp_connect(tcp_id, host, port, false)) {
    LISA_LOGE(LOG_TAG, "Error: Connect failed");
    return -1;
}

/* 发送数据 */
int sent = lisa_modem_tcp_send(tcp_id, data, data_len, 5000);

/* 接收数据 */
int recv_len = lisa_modem_tcp_recv(tcp_id, buffer, buffer_size, 10000);

/* 关闭连接 */
lisa_modem_tcp_closesocket(tcp_id);
lisa_modem_tcp_deinit(tcp_id);
```

## 配置说明

### prj.conf 关键配置

```kconfig
# 4G Modem 配置（必需）
CONFIG_LISA_MODEM=y

# SAL 网络抽象层
CONFIG_SAL_USING_POSIX=y

# LWIP 网络栈
CONFIG_LWIP=y
CONFIG_LWIP_OPTION_LWIP_DNS=y

# 堆内存配置
CONFIG_HEAP_SIZE=0x5000
CONFIG_PSRAM_HEAP_SIZE=0x100000
```

## 注意事项

1. **SIM 卡**：确保 SIM 卡已正确插入且已开通数据业务，否则无法进行网络通信

2. **网络注册**：4G 模块需要一定时间完成网络注册，示例中等待 3 秒，实际使用时可根据需要调整

3. **UART 配置**：根据实际硬件连接修改 `MODEM_UART_DEVICE` 宏

4. **超时设置**：发送和接收都有超时参数，根据网络状况适当调整

5. **资源释放**：TCP 连接使用完毕后必须调用 `lisa_modem_tcp_closesocket()` 和 `lisa_modem_tcp_deinit()` 释放资源

6. **错误处理**：网络通信可能因各种原因失败，务必检查所有 API 的返回值

## 相关文档

- [LISA Modem 组件文档](../../../components/lisa_modem/README.md) - 4G 模块详细说明
- [NetDev 网络设备抽象层](../../../components/lisa_net/README.md) - 网络设备管理
