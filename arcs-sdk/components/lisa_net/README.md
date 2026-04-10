# LISA Net 组件

网络设备抽象层（NetDev）和套接字抽象层（SAL）组件，为 ARCS 平台提供统一的多网卡管理和标准套接字接口，支持 WiFi、4G 等多种网络设备的无缝切换。

## 功能特性

- **多网卡管理**: 支持同时管理多个网络设备（WiFi、4G、以太网等）
- **优先级调度**: 基于优先级的网络设备自动选择和切换
- **统一接口**: 提供标准的 BSD Socket API，应用层无需关心底层网络类型
- **状态监控**: 实时监控网络设备状态变化，支持回调通知
- **DNS 解析**: 支持多网卡环境下的 DNS 域名解析
- **POSIX 兼容**: 支持 POSIX 标准的套接字接口
- **动态注册**: 支持运行时动态注册和注销网络设备
- **lwIP 集成**: 与 lwIP 网络栈深度集成，支持 WiFi 网络
- **AT 设备支持**: 支持基于 AT 命令的 4G/LTE 模块

## 配置选项

在 `prj.conf` 中启用组件：

```kconfig
CONFIG_SAL_USING_POSIX=y    # 启用 SAL 套接字抽象层
CONFIG_LISA_NETWORK=y        # 启用 LISA 网络组件
CONFIG_LWIP=y                # 启用 lwIP 网络栈（WiFi）
CONFIG_LISA_MODEM=y          # 启用 4G Modem 支持（可选）
```

## API 接口

### 网络设备管理

```c
/* 初始化网络设备子系统 */
int netdev_init(void);

/* 注册网络设备 */
int netdev_register(struct netdev *netdev, const char *name,
                    uint8_t priority, void *user_data);
int app_netdev_register(const char *name, uint32_t priority);

/* 注销网络设备 */
int netdev_unregister(struct netdev *netdev);
int netdev_unregister_by_name(const char *name);
```

### 设备查找

```c
/* 按名称获取设备 */
struct netdev *netdev_get_by_name(const char *name);

/* 按 IP 地址获取设备 */
struct netdev *netdev_get_by_ipaddr(netdev_ip_addr_t *ip_addr);

/* 获取具有指定状态的第一个设备 */
struct netdev *netdev_get_first_by_flags(uint16_t flags);
```

### 优先级管理

```c
/* 设置设备优先级 */
int netdev_set_priority(const char *name, uint8_t priority);

/* 获取设备优先级 */
int netdev_get_priority(const char *name);

/* 设置默认网络设备 */
void netdev_set_default(struct netdev *netdev);
```

### 设备状态控制

```c
/* 启用/禁用设备 */
int netdev_set_up(struct netdev *netdev);
int netdev_set_down(struct netdev *netdev);

/* DHCP 控制 */
int netdev_dhcp_enabled(struct netdev *netdev, bool is_enabled);
```

### 状态查询宏

```c
/* 检查设备状态 */
netdev_is_up(netdev)           /* 设备是否启用 */
netdev_is_link_up(netdev)      /* 链路是否连接 */
netdev_is_internet_up(netdev)  /* 互联网是否可用 */
netdev_is_dhcp_enabled(netdev) /* DHCP 是否启用 */
```

### 回调注册

```c
/* 设置状态变化回调 */
void netdev_set_status_callback(struct netdev *netdev,
                                 netdev_callback_fn status_callback);

/* 设置地址变化回调 */
void netdev_set_addr_callback(struct netdev *netdev,
                               netdev_callback_fn addr_callback);
```

## 使用示例

### 基础网络设备管理示例

```c
#include "netdev.h"
#include "lisa_log.h"

#define LOG_TAG "net_example"

/* 网络状态变化回调 */
static void on_status_change(struct netdev *netdev, enum netdev_cb_type type)
{
    switch (type) {
    case NETDEV_CB_STATUS_LINK_UP:
        LISA_LOGI(LOG_TAG, "Device %s link up", netdev->name);
        break;
    case NETDEV_CB_STATUS_LINK_DOWN:
        LISA_LOGI(LOG_TAG, "Device %s link down", netdev->name);
        break;
    case NETDEV_CB_STATUS_INTERNET_UP:
        LISA_LOGI(LOG_TAG, "Device %s internet available", netdev->name);
        break;
    case NETDEV_CB_STATUS_INTERNET_DOWN:
        LISA_LOGI(LOG_TAG, "Device %s internet unavailable", netdev->name);
        break;
    default:
        break;
    }
}

/* 地址变化回调 */
static void on_addr_change(struct netdev *netdev, enum netdev_cb_type type)
{
    if (type == NETDEV_CB_ADDR_IP) {
        LISA_LOGI(LOG_TAG, "Device %s IP: %d.%d.%d.%d",
                  netdev->name,
                  netdev->ip_addr.addr & 0xFF,
                  (netdev->ip_addr.addr >> 8) & 0xFF,
                  (netdev->ip_addr.addr >> 16) & 0xFF,
                  (netdev->ip_addr.addr >> 24) & 0xFF);
    }
}

int netdev_example(void)
{
    /* 1. 初始化网络设备子系统 */
    if (netdev_init() != 0) {
        LISA_LOGE(LOG_TAG, "Failed to initialize netdev subsystem");
        return -1;
    }

    /* 2. 注册 WiFi 网络设备（优先级 100） */
    if (app_netdev_register("wifi0", 100) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to register WiFi device");
        return -1;
    }

    /* 3. 设置回调函数 */
    struct netdev *wifi_dev = netdev_get_by_name("wifi0");
    if (wifi_dev) {
        netdev_set_status_callback(wifi_dev, on_status_change);
        netdev_set_addr_callback(wifi_dev, on_addr_change);
    }

    /* 4. 查询设备状态 */
    if (netdev_is_link_up(wifi_dev)) {
        LISA_LOGI(LOG_TAG, "WiFi link is up");
    }

    return 0;
}
```

### 多网卡优先级管理示例

```c
#include "netdev.h"
#include "lisa_log.h"

#define LOG_TAG "priority_example"

/* 打印所有已注册的网络设备 */
static void print_all_devices(void)
{
    struct netdev *dev = netdev_list;
    int index = 0;

    LISA_LOGI(LOG_TAG, "=== Registered Network Devices ===");

    while (dev != NULL) {
        LISA_LOGI(LOG_TAG, "[%d] %s: priority=%d, flags=0x%04X",
                  index++, dev->name, dev->priority, dev->flags);
        dev = dev->next;
    }

    if (netdev_default) {
        LISA_LOGI(LOG_TAG, "Default device: %s", netdev_default->name);
    }
}

int priority_management_example(void)
{
    /* 1. 注册多个网络设备 */
    app_netdev_register("wifi0", 100);   /* WiFi: 高优先级 */
    app_netdev_register("ml307", 500);   /* 4G: 低优先级 */

    /* 2. 打印当前设备列表 */
    print_all_devices();

    /* 3. 获取设备优先级 */
    int wifi_priority = netdev_get_priority("wifi0");
    int modem_priority = netdev_get_priority("ml307");

    LISA_LOGI(LOG_TAG, "WiFi priority: %d", wifi_priority);
    LISA_LOGI(LOG_TAG, "4G modem priority: %d", modem_priority);

    /* 4. 动态调整优先级（提升 4G 优先级） */
    LISA_LOGI(LOG_TAG, "Raising 4G priority...");
    netdev_set_priority("ml307", 50);  /* 现在 4G 优先级更高 */

    /* 5. 再次打印设备列表 */
    print_all_devices();

    /* 6. 恢复原优先级 */
    netdev_set_priority("ml307", 500);

    return 0;
}
```

### 使用标准 Socket API 示例

```c
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "lisa_log.h"

#define LOG_TAG "socket_example"

int socket_example(void)
{
    int sockfd;
    struct sockaddr_in server_addr;
    struct hostent *host;
    const char *request = "GET / HTTP/1.1\r\nHost: httpbin.org\r\n\r\n";
    char buffer[1024];
    int bytes_received;

    /* 1. DNS 解析 */
    host = gethostbyname("httpbin.org");
    if (!host) {
        LISA_LOGE(LOG_TAG, "DNS resolve failed");
        return -1;
    }

    LISA_LOGI(LOG_TAG, "Resolved IP: %s",
              inet_ntoa(*(struct in_addr *)host->h_addr));

    /* 2. 创建 socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LISA_LOGE(LOG_TAG, "Socket creation failed");
        return -1;
    }

    /* 3. 设置服务器地址 */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    memcpy(&server_addr.sin_addr, host->h_addr, host->h_length);

    /* 4. 连接服务器 */
    if (connect(sockfd, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        LISA_LOGE(LOG_TAG, "Connection failed");
        close(sockfd);
        return -1;
    }

    LISA_LOGI(LOG_TAG, "Connected to server");

    /* 5. 发送 HTTP 请求 */
    if (send(sockfd, request, strlen(request), 0) < 0) {
        LISA_LOGE(LOG_TAG, "Send failed");
        close(sockfd);
        return -1;
    }

    /* 6. 接收响应 */
    bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        LISA_LOGI(LOG_TAG, "Received %d bytes", bytes_received);
    }

    /* 7. 关闭连接 */
    close(sockfd);

    return 0;
}
```

## 网络设备优先级说明

NetDev 使用优先级来决定默认网络设备的选择：

| 优先级值 | 说明 | 典型使用场景 |
|---------|------|-------------|
| 0-50 | 最高优先级 | 有线以太网、专用网络 |
| 50-100 | 高优先级 | WiFi 网络（默认） |
| 100-200 | 中等优先级 | 备用 WiFi 网络 |
| 200-500 | 低优先级 | 4G/LTE 网络（默认） |
| 500+ | 最低优先级 | 备用蜂窝网络 |

**注意**: 数值越小，优先级越高。当有多个网络设备可用时，系统自动选择优先级最高（数值最小）的设备。

## 设备状态标志

| 标志 | 值 | 说明 |
|------|-----|------|
| `NETDEV_FLAG_UP` | 0x01 | 设备已启用 |
| `NETDEV_FLAG_LINK_UP` | 0x04 | 链路已连接 |
| `NETDEV_FLAG_ETHARP` | 0x08 | ARP 已启用 |
| `NETDEV_FLAG_INTERNET_UP` | 0x80 | 互联网可用 |
| `NETDEV_FLAG_DHCP` | 0x100 | DHCP 已启用 |

## 回调事件类型

| 事件类型 | 说明 |
|---------|------|
| `NETDEV_CB_ADDR_IP` | IP 地址变化 |
| `NETDEV_CB_ADDR_NETMASK` | 子网掩码变化 |
| `NETDEV_CB_ADDR_GATEWAY` | 网关地址变化 |
| `NETDEV_CB_ADDR_DNS_SERVER` | DNS 服务器变化 |
| `NETDEV_CB_STATUS_UP` | 设备启用 |
| `NETDEV_CB_STATUS_DOWN` | 设备禁用 |
| `NETDEV_CB_STATUS_LINK_UP` | 链路连接 |
| `NETDEV_CB_STATUS_LINK_DOWN` | 链路断开 |
| `NETDEV_CB_STATUS_INTERNET_UP` | 互联网可用 |
| `NETDEV_CB_STATUS_INTERNET_DOWN` | 互联网不可用 |

## 使用注意事项

1. **初始化顺序**: 必须先调用 `netdev_init()` 初始化子系统，再注册网络设备
2. **设备命名规范**:
   - WiFi 设备: `wifi0`、`wlan0` 等
   - 4G 设备: `ml307`、`4g0`、`lte0` 等
   - 以太网: `eth0` 等
3. **优先级设置**: 优先级值为 0 时，自动使用默认值 100
4. **回调线程安全**: 状态回调可能在不同线程上下文中被调用，注意线程安全
5. **资源释放**: 程序退出前应调用 `netdev_unregister_by_name()` 注销设备
6. **多网卡切换**: 当高优先级网络断开时，系统自动切换到下一个可用的网络设备
7. **DNS 解析**: 在多网卡环境下，DNS 解析会使用当前默认网络设备
8. **lwIP 集成**: WiFi 网络使用 lwIP 协议栈，4G 网络使用 AT 命令接口
9. **SAL 层**: SAL 层提供统一的套接字接口，自动路由到正确的网络设备
10. **并发访问**: NetDev 接口是线程安全的，可以在多任务环境中使用

## 文件说明

- `include/netdev.h` - 网络设备抽象层头文件，包含 API 接口和数据结构定义
- `include/sal_socket.h` - SAL 套接字接口头文件
- `include/sal_netdb.h` - SAL DNS 解析接口头文件
- `sal/src/netdev.c` - 网络设备管理实现
- `sal/src/sal_socket.c` - SAL 套接字实现
- `sal/impl/netdev_lwip.c` - lwIP 网络设备实现（WiFi）
- `sal/impl/netdev_at.c` - AT 网络设备实现（4G Modem）
- `Kconfig` - 组件配置选项定义文件
- `CMakeLists.txt` - 组件构建配置文件
