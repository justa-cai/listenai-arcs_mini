# 网络设备抽象层（NetDev）示例

## 功能说明

本示例演示如何使用 LISA 网络设备抽象层（NetDev）管理多种网络接口，包括：
- 初始化网络设备子系统
- 注册和管理多个网络设备（WiFi、4G）
- 设置网络设备优先级实现自动切换
- 监听网络状态变化
- 查询网络设备状态

NetDev 提供了统一的网络设备管理接口，支持多网卡共存和自动切换，应用层无需关心底层使用的是 WiFi 还是 4G 网络。

## 硬件连接

本示例支持 WiFi 和 4G 两种网络接口：

**WiFi（默认启用）：**
- 使用芯片内部 WiFi 外设，无需额外接线

**4G Modem（可选）：**
- 默认使用 `uart1` 设备与 ML307 模块通信
- 需要根据实际硬件连接修改 `MODEM_UART_DEVICE` 宏

**串口输出：**
- 串口 TX: PA3
- 波特率：921600

## 示例内容

1. 初始化网络设备子系统
2. 初始化并注册 WiFi 网络设备（优先级 100）
3. 初始化并注册 4G 网络设备（优先级 500，可选）
4. 设置网络状态变化回调
5. 演示优先级管理（修改和恢复设备优先级）
6. 演示设备状态查询
7. 定期打印已注册的网络设备列表

## 配置修改

使用前请修改 `src/main.c` 中的配置：

```c
// WiFi 配置
#define TARGET_WIFI_SSID    "your_wifi_ssid"
#define TARGET_WIFI_PWD     "your_wifi_pwd"

// 4G Modem 配置（如果启用）
#define MODEM_UART_DEVICE   "uart1"
```

### 启用 4G Modem

如需同时使用 WiFi 和 4G，取消 `prj.conf` 中的注释：

```kconfig
# 4G Modem 配置（可选，取消注释以启用）
CONFIG_LISA_MODEM=y
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
[I][net] === NetDev (Network Device Abstraction) Example ===
[I][net] Initializing NetDev subsystem...
[I][net] NetDev subsystem initialized
[I][net] Initializing WiFi device...
[I][net] WiFi MAC: XX:XX:XX:XX:XX:XX
[I][net] WiFi device initialized and registered
[I][net] Callbacks set for WiFi device
[I][net] Waiting for network devices to be ready...
[I][net] WiFi connection status: 1
[I][net] WiFi connected to AP
[I][net] DHCP Success: IP=192.168.1.100
[I][net] NetDev [wifi0] status changed: LINK_UP
[I][net] NetDev [wifi0] address changed: IP_CHANGED
[I][net] NetDev [wifi0] IP: 192.168.1.100
[I][net] === Priority Management Demo ===
[I][net] === Registered Network Devices ===
[I][net] [0] Name: wifi0, Priority: 100, Flags: 0x01D5
[I][net]     IP: 192.168.1.100
[I][net]     Status: UP=1, LINK=1, INET=1, DHCP=1
[I][net] Default device: wifi0
[I][net] WiFi priority: 100
[I][net] === Device Status Demo ===
[I][net] WiFi device status:
[I][net]   UP: Yes
[I][net]   Link: Up
[I][net]   Internet: Up
[I][net]   DHCP: Enabled
[I][net] First device with LINK_UP: wifi0
[I][net] === Example completed ===
```

## 核心 API

| API | 说明 |
|-----|------|
| `netdev_init()` | 初始化网络设备子系统 |
| `app_netdev_register()` | 按名称注册网络设备 |
| `netdev_unregister_by_name()` | 按名称注销网络设备 |
| `netdev_get_by_name()` | 按名称获取网络设备 |
| `netdev_get_first_by_flags()` | 获取具有指定状态的第一个设备 |
| `netdev_set_priority()` | 设置设备优先级 |
| `netdev_get_priority()` | 获取设备优先级 |
| `netdev_set_status_callback()` | 设置状态变化回调 |
| `netdev_set_addr_callback()` | 设置地址变化回调 |
| `netdev_is_up()` | 检查设备是否启用 |
| `netdev_is_link_up()` | 检查链路是否连接 |
| `netdev_is_internet_up()` | 检查互联网是否可用 |

## 关键代码

### 1. 初始化和注册网络设备

```c
/* 初始化网络设备子系统 */
if (netdev_init() != 0) {
    LISA_LOGE(LOG_TAG, "Error: Failed to initialize NetDev");
    return -1;
}

/* 注册 WiFi 网络设备（优先级 100） */
if (app_netdev_register("wifi0", 100) != 0) {
    LISA_LOGE(LOG_TAG, "Error: Failed to register WiFi device");
    return -1;
}
```

### 2. 设置状态变化回调

```c
static void netdev_status_callback(struct netdev *netdev, enum netdev_cb_type type)
{
    switch (type) {
    case NETDEV_CB_STATUS_LINK_UP:
        LISA_LOGI(LOG_TAG, "Device %s link up", netdev->name);
        break;
    case NETDEV_CB_STATUS_LINK_DOWN:
        LISA_LOGI(LOG_TAG, "Device %s link down", netdev->name);
        break;
    /* ... */
    }
}

/* 设置回调 */
struct netdev *netdev = netdev_get_by_name("wifi0");
netdev_set_status_callback(netdev, netdev_status_callback);
```

### 3. 优先级管理

```c
/* 获取设备优先级 */
int priority = netdev_get_priority("wifi0");

/* 设置设备优先级（数值越小优先级越高） */
netdev_set_priority("ml307", 50);  /* 提升 4G 优先级 */
```

### 4. 设备状态查询

```c
struct netdev *netdev = netdev_get_by_name("wifi0");

if (netdev_is_up(netdev)) {
    LISA_LOGI(LOG_TAG, "Device is up");
}

if (netdev_is_link_up(netdev)) {
    LISA_LOGI(LOG_TAG, "Link is connected");
}

if (netdev_is_internet_up(netdev)) {
    LISA_LOGI(LOG_TAG, "Internet is available");
}
```

## 配置说明

### prj.conf 关键配置

```kconfig
# SAL 网络抽象层
CONFIG_SAL_USING_POSIX=y

# LISA 网络组件
CONFIG_LISA_NETWORK=y

# WiFi 配置
CONFIG_WIFI=y
CONFIG_LISA_WIFI=y
CONFIG_WIFI_MANAGER=y

# LWIP 网络栈
CONFIG_LWIP=y
CONFIG_LWIP_OPTION_LWIP_DNS=y

# 4G Modem（可选）
# CONFIG_LISA_MODEM=y
```

## 优先级说明

NetDev 使用优先级来决定默认网络设备：
- **数值越小，优先级越高**
- WiFi 默认优先级：100
- 4G 默认优先级：500

当多个网络设备都可用时，系统自动选择优先级最高（数值最小）的设备作为默认设备。

## 注意事项

1. **优先级设置**：优先级值范围为 0-255，数值越小优先级越高，0 会被设置为默认值 100

2. **设备名称规范**：
   - WiFi 设备：`wifi0`、`wlan0` 等
   - 4G 设备：`ml307`、`4g0`、`lte0` 等

3. **多网卡共存**：同时启用 WiFi 和 4G 时，系统根据优先级自动选择默认设备

4. **回调线程安全**：状态回调可能在不同线程上下文中被调用，注意线程安全

5. **资源释放**：程序退出前应调用 `netdev_unregister_by_name()` 注销设备

## 相关文档

- [NetDev 网络设备抽象层](../../../components/lisa_net/README.md) - 详细 API 说明
- [4G Modem 示例](../modem/README.md) - 4G 模块使用示例
- [HTTP 示例](../http/README.md) - HTTP 网络通信示例
