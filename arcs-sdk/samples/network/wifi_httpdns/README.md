# WiFi + HTTPDNS 示例

## 功能说明

此示例演示如何在 WiFi 连接成功后使用 HTTPDNS 进行域名解析。

HTTPDNS 是基于 HTTP 协议的 DNS 解析服务，可以绕过运营商 Local DNS ，有效避免 DNS 劫持和 DNS 污染问题。

### 工作原理

1. WiFi 连接成功后，通过 DHCP 获取 IP 地址
2. 调用标准 socket 接口（如 `gethostbyname`）进行域名解析
3. lwip 会自动调用 HTTPDNS 解析域名
4. HTTPDNS 向阿里云 HTTPDNS 服务器发送解析请求
5. 解析结果缓存到本地，供后续使用

## 硬件连接

本示例使用芯片内部 WiFi 外设，无需额外接线。

**串口输出：**
- 波特率：921600

## 示例内容

1. 初始化 WiFi 并连接到 AP
2. 等待 WiFi 连接成功并获取 IP 地址
3. 使用 HTTPDNS 进行域名解析
4. 建立 TCP 连接到解析后的 IP 地址
5. 发送 HTTP 请求并接收响应

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 准备工作

本示例使用阿里云 HTTPDNS 服务进行域名解析，使用前需要先开通服务。

### 开通 HTTPDNS 服务

1. 访问 [阿里云 HTTPDNS 产品页面](https://www.aliyun.com/product/httpdns)
2. 登录阿里云账号并开通 HTTPDNS 服务
3. 在控制台获取您的 Account ID（账户 ID）

> **注意**：目前代码仅支持阿里云 HTTPDNS 服务。

## 配置说明

在 `prj.conf` 中配置 HTTPDNS：

```c
# HTTPDNS 配置
CONFIG_HTTPDNS=y
CONFIG_HTTPDNS_USE_ALIYUN=y
CONFIG_HTTPDNS_ALIYUN_ACCOUNT_ID="your_aliyun_account_id"
```

**说明：**
- `CONFIG_HTTPDNS`：使能 HTTPDNS 功能
- `CONFIG_HTTPDNS_USE_ALIYUN`：使用阿里云 HTTPDNS 服务
- `CONFIG_HTTPDNS_ALIYUN_ACCOUNT_ID`：阿里云 HTTPDNS 账户 ID（替换为您在阿里云控制台获取的 ID）

## 预期输出

WiFi 连接成功后，系统将自动进行 HTTPDNS 域名解析测试：

```
I/app-wifi-httpdns [1034:42:51.360 1 main] resolving host: www.aliyun.com ...
I/NO_TAG          [1034:42:51.360 1 main] httpdns hook called: name=www.aliyun.com
I/NO_TAG          [1034:42:51.360 1 main] httpdns resolve: www.aliyun.com
I/NO_TAG          [1034:42:51.360 1 main] httpdns request: 203.107.1.1/{hide}/d?host=www.aliyun.com
...
I/NO_TAG          [1034:42:54.818 1 main] httpdns response body: {"host":"www.aliyun.com","ips":["59.37.80.15","14.215.57.64","202.104.186.207","121.14.134.66","14.215.172.18","183.60.240.237","183.3.204.203","218.16.122.68","121.14.134.67","183.3.204.198","113.96.128.29","183.60.240.244"],"ttl":60,"origin_ttl":60,"client_ip":"113.118.105.114"}
I/NO_TAG          [1034:42:54.820 1 main] dns cache add: www.aliyun.com -> 59.37.80.15 (ttl=60)
I/NO_TAG          [1034:42:54.820 1 main] httpdns resolved www.aliyun.com -> 59.37.80.15 (ttl=60)
I/app-wifi-httpdns [1034:42:54.820 1 main] resolved www.aliyun.com -> 59.37.80.15
I/app-wifi-httpdns [1034:42:54.820 1 main] connecting to www.aliyun.com:80 ...
I/app-wifi-httpdns [1034:42:54.840 1 main] connect success
I/app-wifi-httpdns [1034:42:54.841 1 main] sent 58 bytes
```

**日志解析（HTTPDNS 解析成功）：**

1. `httpdns hook called` - lwip 调用 HTTPDNS 钩子函数
2. `httpdns request` - 向阿里云 HTTPDNS 服务器发送解析请求
3. `httpdns response body` - HTTPDNS 服务器返回的 JSON 响应，包含多个 IP 地址
4. `dns cache add` - 解析结果添加到本地缓存，TTL 为 60 秒
5. `httpdns resolved` - 域名解析成功，得到 IP 地址

### HTTPDNS 失败回退到本地 DNS

如果 HTTPDNS 解析失败（例如 Account ID 无效、网络超时等），系统会自动回退到传统的本地 DNS 解析：

```
I/app-wifi-httpdns [1034:43:02.360 1 main] resolving host: www.aliyun.com ...
I/NO_TAG          [1034:43:02.360 1 main] httpdns hook called: name=www.aliyun.com
I/NO_TAG          [1034:43:02.360 1 main] httpdns resolve: www.aliyun.com
I/NO_TAG          [1034:43:02.361 1 main] httpdns request: 203.107.1.1/123456/d?host=www.aliyun.com
I/NO_TAG          [1034:43:02.551 1 main] httpdns response body: {"code":"InvalidAccount"}
E/NO_TAG          [1034:43:02.551 1 main] httpdns no ips in response
W/NO_TAG          [1034:43:02.551 1 main] httpdns failed for www.aliyun.com, fallback to lwip dns
I/app-wifi-httpdns [1034:43:02.565 1 main] resolved www.aliyun.com -> 202.104.186.219
```

**日志解析（HTTPDNS 失败回退）：**

1. `httpdns response body: {"code":"InvalidAccount"}` - HTTPDNS 服务器返回错误响应
2. `httpdns no ips in response` - 解析结果中没有 IP 地址
3. `httpdns failed for www.aliyun.com, fallback to lwip dns` - HTTPDNS 解析失败，自动回退到本地 DNS
4. `resolved www.aliyun.com -> 202.104.186.219` - 本地 DNS 解析成功

## 核心代码

### 1. HTTPDNS 测试函数

```c
static void httpdns_test(void)
{
    int sock = -1;
    struct hostent *host;
    struct sockaddr_in server_addr;
    char recv_buf[128];

    LOGI("httpdns test start");

    /* 创建 socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LOGE("socket create failed");
        return;
    }

    /* 通过域名获取IP地址 - lwip会自动调用httpdns解析 */
    LOGI("resolving host: %s ...", TEST_HOST);
    host = gethostbyname(TEST_HOST);
    if (host == NULL) {
        LOGE("gethostbyname failed");
        close(sock);
        return;
    }

    LOGI("resolved %s -> %s", TEST_HOST, inet_ntoa(*(struct in_addr *)host->h_addr));

    /* 设置服务器地址并连接 */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TEST_PORT);
    memcpy(&server_addr.sin_addr, host->h_addr, host->h_length);

    LOGI("connecting to %s:%d ...", TEST_HOST, TEST_PORT);
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOGE("connect failed");
        close(sock);
        return;
    }

    LOGI("connect success");

    /* 发送HTTP请求 */
    const char *http_req = "GET / HTTP/1.1\r\nHost: www.aliyun.com\r\nConnection: close\r\n\r\n";
    int sent = send(sock, http_req, strlen(http_req), 0);
    LOGI("sent %d bytes", sent);

    /* 接收响应 */
    int recv_len = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (recv_len > 0) {
        recv_buf[recv_len] = '\0';
        LOGI("received %d bytes", recv_len);
    }

    close(sock);
    LOGI("httpdns test done");
}
```

### 2. WiFi 连接后触发测试

在主循环中检测 WiFi 连接状态，连接成功后触发 HTTPDNS 测试：

```c
while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* If WiFi is connected, run httpdns test once */
    if (s_wifi_connected) {
        s_wifi_connected = false;  /* 防止重复执行 */
        vTaskDelay(pdMS_TO_TICKS(2000));  /* 等待网络稳定 */
        httpdns_test();
    }
}
```

## 相关文档

- [HTTPDNS 组件文档](../../../components/httpdns/README.md) - HTTPDNS 组件详细说明

## 注意事项

1. **阿里云 HTTPDNS 服务**：请先在 [阿里云控制台](https://www.aliyun.com/product/httpdns) 开通服务并获取 Account ID
2. **测试域名**：默认测试域名为 `www.aliyun.com`，可在 `main.c` 中修改 `TEST_HOST` 和 `TEST_PORT`
3. **网络要求**：设备需要能够访问互联网，且能访问阿里云 HTTPDNS 服务器（203.107.1.1）
4. **缓存机制**：HTTPDNS 解析结果会缓存 60 秒（可配置），在缓存有效期内重复解析不会发起 HTTP 请求
5. **服务支持**：当前仅支持阿里云 HTTPDNS 服务
