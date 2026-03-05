# HTTP 下载性能测试示例

## 功能说明

此示例演示如何测试 HTTP 下载性能，通过不同的缓冲区大小和文件大小组合，评估网络下载的最佳配置。

示例使用 XRADIO HTTP 客户端库进行流式下载，实时统计下载速度、数据块分布等性能指标，并生成详细的性能测试报告。

### 测试方案

**测试维度：**
- **文件大小测试**：10MB、50MB、100MB 三种文件大小
- **缓冲区大小测试**：2KB、4KB、8KB、16KB、32KB、64KB、128KB 多种缓冲区配置

**性能指标：**
- 平均下载速度（KB/s 和 Mbps）
- 总下载时间
- 数据块统计（数量、最小/最大/平均大小）
- 不同文件大小下的最佳缓冲区配置

## 硬件连接

本示例使用芯片内部 WiFi 外设，无需额外接线。

**串口输出：**
- 串口 TX: PA3
- 波特率：921600

## 测试环境准备

### 1. 启动 HTTP 测试服务器

在与设备**同一局域网**的 Linux/Mac 主机上运行测试服务器：

```bash
cd samples/network/http_download_perf
python3 http-server.py
```

服务器启动后将显示：
```
========================================
  ARCS HTTP 测试服务器
========================================
监听地址: http://0.0.0.0:8080
默认文件大小: 5MB
最大文件大小: 100MB

使用示例:
  下载默认大小: http://0.0.0.0:8080/random
  下载 10MB:   http://0.0.0.0:8080/random?size=10
  下载 50MB:   http://0.0.0.0:8080/random?size=50
========================================
```

### 2. 配置 WiFi 和服务器地址

**重要：** 本示例强制要求在编译前配置 WiFi 和服务器信息，否则会编译失败。

在 `src/main.c` 中找到配置区域（第 21-28 行），**取消注释**并填写实际配置：

**修改前（默认状态）：**
```c
// ============================================================
// 重要：使用前请修改以下配置！
// ============================================================
// #define SERVER_HOST      // 修改为测试服务器的 IP 地址，例如："192.168.1.100"
#define SERVER_PORT 8080  // 修改为测试服务器的端口号，默认 8080

// #define TARGET_WIFI_SSID   // 修改为目标 WiFi 的 SSID
// #define TARGET_WIFI_PWD     // 修改为目标 WiFi 的密码
```

**修改后（配置示例）：**
```c
// ============================================================
// 重要：使用前请修改以下配置！
// ============================================================
#define SERVER_HOST "192.168.1.100"        // 测试服务器的局域网 IP
#define SERVER_PORT 8080                    // 测试服务器端口

#define TARGET_WIFI_SSID "MyHomeWiFi"      // 你的 WiFi 名称
#define TARGET_WIFI_PWD "mypassword123"    // 你的 WiFi 密码
```

**操作步骤：**
1. 移除 `SERVER_HOST` 前的 `//` 注释符
2. 将 `SERVER_HOST` 后面修改为实际的服务器 IP（例如："192.168.1.100"）
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
[INFO] HTTP Performance Test Starting...
[INFO] WiFi auto-connect started
[INFO] custom_get_wifi_mac: 0, XX:XX:XX:XX:XX:XX
[INFO] WiFi connection status: 1
[INFO] WiFi connected to AP
[INFO] WiFi event: module=1, id=4
[INFO] Got IP address
[INFO] WiFi connected and IP obtained, starting HTTP tests...
```

### 2. 性能测试进度

每个测试显示实时下载进度：

```
========================================
10MB - 2KB 缓冲区
缓冲区大小: 2.00 KB (2048 bytes)
========================================
URL: http://192.168.1.100:8080/random?size=10
========== 开始下载 ==========
注意: 数据以小块方式接收，不会占用大量内存
下载中: 1.23 MB (634.25 KB/s) | 已接收 634 个数据块
下载中: 2.56 MB (681.19 KB/s) | 已接收 1319 个数据块
下载中: 4.02 MB (749.88 KB/s) | 已接收 2067 个数据块
...
========== 下载完成 ==========
总大小: 10.00 MB (10485760 bytes)
总时间: 14235 ms (14.24 秒)
平均速度: 719.67 KB/s (5.62 Mbps)
--- 数据块统计 ---
总数据块数: 5120
数据块大小: 最小=1.46 KB, 最大=2.00 KB, 平均=2.00 KB
================================
```

### 3. 性能测试汇总报告

所有测试完成后生成汇总报告：

```
╔════════════════════════════════════════════════════════════════╗
║          HTTP 下载性能测试汇总报告                            ║
╚════════════════════════════════════════════════════════════════╝

序号 | 文件大小 | 缓冲区   | 平均速度      | 总时间    | 数据块数
-----|----------|----------|---------------|-----------|----------
  1  |   10MB   | 2KB      |  719.67 KB/s |   14.24s | 5120
  2  |   10MB   | 8KB      |  856.34 KB/s |   11.95s | 1280
  3  |   10MB   | 16KB     |  923.45 KB/s |   11.09s | 640
  4  |   10MB   | 32KB     |  981.23 KB/s |   10.43s | 320
  5  |   10MB   | 64KB     | 1024.56 KB/s |   10.00s | 160
  6  |   10MB   | 128KB    | 1067.89 KB/s |    9.59s | 80
  7  |   50MB   | 2KB      |  734.21 KB/s |   69.87s | 25600
  8  |   50MB   | 16KB     |  945.67 KB/s |   54.23s | 3200
  9  |   50MB   | 64KB     | 1089.34 KB/s |   47.08s | 800
 10  |   50MB   | 128KB    | 1124.78 KB/s |   45.58s | 400
 11  |  100MB   | 16KB     |  967.45 KB/s |  105.92s | 6400
 12  |  100MB   | 64KB     | 1102.34 KB/s |   93.02s | 1600
 13  |  100MB   | 128KB    | 1145.89 KB/s |   89.46s | 800

═══════════════════════════════════════════════════════════════
                       性能分析
═══════════════════════════════════════════════════════════════
• 10MB 文件: 最佳缓冲区 = 128KB, 速度 = 1067.89 KB/s (8.34 Mbps)
• 50MB 文件: 最佳缓冲区 = 128KB, 速度 = 1124.78 KB/s (8.79 Mbps)
• 100MB 文件: 最佳缓冲区 = 128KB, 速度 = 1145.89 KB/s (8.95 Mbps)

═══════════════════════════════════════════════════════════════

========================================
所有测试完成
========================================
```

## 核心 API

| API | 说明 |
|-----|------|
| `HTTPC_open()` | 打开 HTTP 连接 |
| `HTTPC_request()` | 发送 HTTP 请求 |
| `HTTPC_read()` | 流式读取响应数据 |
| `HTTPC_close()` | 关闭 HTTP 连接 |
| `wifi_mgr_init()` | 初始化 WiFi 管理器 |
| `wifi_mgr_auto_connect_start()` | 启动自动连接 |
| `ls_event_register_cb()` | 注册 WiFi 事件回调 |

## 关键代码

### 1. 流式 HTTP 下载

使用流式读取，避免大文件占用过多内存：

```c
// 分配响应缓冲区
response_buf = (CHAR *)malloc(buffer_size);

// 初始化 HTTP 参数
memset(&http_params, 0, sizeof(HTTPParameters));
snprintf(http_params.Uri, HTTP_CLIENT_MAX_URL_LENGTH,
         "http://%s:%d%s", SERVER_HOST, SERVER_PORT, uri);
http_params.HttpVerb = VerbGet;
http_params.nTimeout = 120;

// 打开连接并发送请求
HTTPC_open(&http_params);
HTTPC_request(&http_params, NULL);

// 流式读取数据
while (1) {
    ret = HTTPC_read(&http_params, response_buf, buffer_size, &recv_size);
    if (ret != 0 || recv_size <= 0) {
        break;  // 读取完成或出错
    }

    // 更新下载统计
    update_download_stats(recv_size);
}

// 关闭连接
HTTPC_close(&http_params);
free(response_buf);
```

### 2. WiFi 连接和 IP 获取

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

### 3. 性能统计

实时统计下载速度和数据块信息：

```c
static void update_download_stats(uint32_t chunk_size)
{
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // 首次接收，初始化统计
    if (g_download_stats.start_time == 0) {
        g_download_stats.start_time = current_time;
        g_download_stats.last_print_time = current_time;
    }

    // 统计数据块信息
    g_download_stats.chunk_count++;
    if (chunk_size < g_download_stats.min_chunk_size) {
        g_download_stats.min_chunk_size = chunk_size;
    }
    if (chunk_size > g_download_stats.max_chunk_size) {
        g_download_stats.max_chunk_size = chunk_size;
    }

    // 累加接收字节数
    g_download_stats.total_bytes += chunk_size;

    // 每隔 1 秒打印一次速度
    if (current_time - g_download_stats.last_print_time >= 1000) {
        uint32_t interval = current_time - g_download_stats.last_print_time;
        uint32_t bytes_in_interval = g_download_stats.total_bytes - g_download_stats.last_bytes;
        float speed_kbps = (float)bytes_in_interval * 1000.0f / interval / 1024.0f;

        LOGI("下载中: %.2f MB (%.2f KB/s) | 已接收 %u 个数据块",
             (float)g_download_stats.total_bytes / (1024.0f * 1024.0f),
             speed_kbps,
             g_download_stats.chunk_count);

        g_download_stats.last_print_time = current_time;
        g_download_stats.last_bytes = g_download_stats.total_bytes;
    }
}
```

## 配置说明

### 缓冲区大小配置

在 `main.c` 中定义了多种缓冲区大小供测试：

```c
#define BUF_SIZE_2KB   (2 * 1024)    // 2KB
#define BUF_SIZE_4KB   (4 * 1024)    // 4KB
#define BUF_SIZE_8KB   (8 * 1024)    // 8KB
#define BUF_SIZE_16KB  (16 * 1024)   // 16KB
#define BUF_SIZE_32KB  (32 * 1024)   // 32KB
#define BUF_SIZE_64KB  (64 * 1024)   // 64KB
#define BUF_SIZE_128KB (128 * 1024)  // 128KB (最大)
```

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

## 性能优化建议

### 1. 缓冲区大小选择

根据测试结果：
- **小文件（<10MB）**：推荐使用 64KB-128KB 缓冲区
- **中等文件（10-50MB）**：推荐使用 128KB 缓冲区
- **大文件（>50MB）**：推荐使用 128KB 缓冲区

### 2. 内存占用

- 流式下载**不会**一次性将整个文件加载到内存
- 内存占用 = 缓冲区大小（2KB-128KB）
- 即使下载 100MB 文件，实际内存占用也仅为缓冲区大小

### 3. 网络性能

影响下载速度的因素：
- WiFi 信号强度
- 网络带宽
- 服务器性能
- TCP 窗口大小
- 缓冲区大小

## 注意事项

1. **编译前配置（重要）**：首次编译前**必须**取消注释并修改 `src/main.c` 中的 WiFi 和服务器配置，否则会触发编译错误：
   ```
   main.c:32:2: error: #error "请修改 SERVER_HOST 为实际的测试服务器 IP 地址（例如：192.168.1.100）"
   main.c:36:2: error: #error "请修改 TARGET_WIFI_SSID 为实际的 WiFi SSID"
   main.c:40:2: error: #error "请修改 TARGET_WIFI_PWD 为实际的 WiFi 密码"
   ```
   这是为了防止使用默认配置导致连接失败。配置方法请参考上方的"配置 WiFi 和服务器地址"章节。

2. **网络环境**：确保设备与测试服务器在同一局域网内，避免跨网段访问影响测试结果

3. **服务器地址**：将 `SERVER_HOST` 修改为运行 `http-server.py` 的主机的局域网 IP（例如：192.168.1.100）

4. **WiFi 配置**：将 `TARGET_WIFI_SSID` 和 `TARGET_WIFI_PWD` 修改为实际的 WiFi SSID 和密码

5. **测试时间**：完整测试需要较长时间（约 10-20 分钟），请耐心等待

6. **内存限制**：缓冲区大小受 PSRAM 堆大小限制，最大建议不超过 128KB

7. **超时设置**：HTTP 超时设置为 120 秒，适合大文件下载，可根据需要调整

8. **Python 版本**：测试服务器需要 Python 3.6 或更高版本

## 相关文档

- [LISA WiFi 组件文档](../../../components/lisa_wifi/README.md) - WiFi 初始化和连接
- [WiFi Manager 文档](../../../modules/wifi_manager/README.md) - WiFi 连接管理
- [LWIP 配置说明](../../../docs/lwip.md) - 网络栈配置
