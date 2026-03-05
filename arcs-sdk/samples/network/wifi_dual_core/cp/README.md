# WiFi 双核运行示例 (CP 核)

## 功能说明

此示例演示如何在双核架构下的 CP (Communication Processor) 核上运行 LWIP 网络栈和 WiFi 应用层。

在双核 WiFi 架构中：
- **AP 核**：运行 WiFi 协议栈底层，负责 RF 校准和硬件驱动
- **CP 核**：运行 LWIP 网络栈、WiFi Manager 和应用层

两个核心通过 IPC 进行通信，实现完整的 WiFi 功能。

### 核心组件

本示例主要涉及两个关键组件：

#### 1. MAC Manager（MAC 地址管理）

负责管理和提供 WiFi MAC 地址：
- 从芯片 ID 生成 MAC 地址
- 或从 Flash/OTP 读取预设的 MAC 地址
- 通过 `custom_mac` 回调提供给 WiFi 底层使用

#### 2. WiFi Manager（WiFi 连接管理）

负责管理 WiFi 连接的生命周期：
- 扫描和连接 AP
- 自动重连机制
- 连接状态管理
- AP 配置的持久化存储

## 硬件连接

本示例使用芯片内部 WiFi 外设，无需额外接线。

**串口输出：**
- 串口 TX: PA21
- 波特率：921600

## 编译运行

### 编译

```{eval-rst}
.. include:: /sample_build.rst
```

### 烧录

```bash
cskburn -C arcs -s /dev/ttyACM0 -b 3000000 0x800000 build/arcs.bin
```

> **注意**：
> - CP 核固件烧录到 0x800000 地址
> - 双核运行时需要配合 AP 核固件一起使用，请参考 [AP 核示例](../ap/README.md)

## 预期输出

系统启动后，CP 核将输出 WiFi 初始化和连接日志：

```
[INFO] app-wifi: EVENT_WIFI_INIT_DONE
...
[INFO] app-wifi: EVENT_WIFI_AP_STARTED
[INFO] app-wifi: EVENT_WIFI_SCAN_DONE
[INFO] app-wifi: EVENT_WIFI_CONNECTED
[INFO] app-wifi: EVENT_WIFI_GOT_IP
```

## 核心代码

### 1. MAC Manager 初始化

初始化 MAC 地址管理器，用于提供 WiFi MAC 地址：

```c
static void user_mac_manager_init(void)
{
    mac_manager_config_t config = {
        .random_mac_if_mac_invalid = false,
    };

    m_mac_manager = mac_manager_init(
        mac_manager_ops_get()->mem_ops, 
        mac_manager_ops_get()->content_ops, 
        &config
    );
}
```

### 2. 自定义 WiFi MAC 地址

通过 MAC Manager 获取 MAC 地址，并提供给 WiFi 使用：

```c
static int8_t user_custom_mac(uint8_t mac_addr[6])
{
    int ret = mac_manager_get(m_mac_manager, mac_addr, 6);
    if (ret != 0) {
        LOGI("[user_wifi]mac_manager_get failed\n");
    }
    LOGI("set mac: %02X:%02X:%02X:%02X:%02X:%02X\n", 
         mac_addr[0], mac_addr[1], mac_addr[2], 
         mac_addr[3], mac_addr[4], mac_addr[5]);
    
    return ret;
}
```

### 3. WiFi Manager 配置

配置 WiFi Manager，设置目标 AP 和自动连接参数：

```c
static void user_wifi_manager_init(void)
{
    wifi_mgr_sta_config_t cfg = {
        .ssid = TARGET_WIFI_SSID,  // "listenai"
        .pwd = TARGET_WIFI_PWD,    // "listenai"
    };

    wifi_mgr_autoconn_config_t autoconn_cfg = {
        .interval_ms = WIFI_MGR_AUTO_CONNECT_INTERVAL_MS,  // 2000ms
    };
    
    wifi_mgr_init(wifi_mgr_ops_get());
    wifi_mgr_sta_enable();
    wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_cb, NULL);

    // 保存 AP 配置并启动自动连接
    wifi_mgr_storage_save_ap(&cfg);
    wifi_mgr_auto_connect_start(&autoconn_cfg);
}
```

### 4. WiFi 初始化完成回调

在 WiFi 初始化完成后，初始化文件系统、KV 存储和 WiFi Manager：

```c
static void cb_wifi_init_done(void)
{
    LOGI("[user_wifi]wifi init done\n");

    // 初始化文件系统和 KV 存储
    user_fs_init();
    lisa_kv_init();
    
    // 初始化 WiFi Manager
    user_wifi_manager_init();
}
```

### 5. 主函数

```c
int main(int argc, char **argv)
{
    // 1. 初始化 MAC Manager
    user_mac_manager_init();

    // 2. 配置 WiFi 回调
    lisa_wifi_ops_t ops = {
        .custom_mac = user_custom_mac,      // 提供 MAC 地址
        .init_done = cb_wifi_init_done,     // WiFi 初始化完成回调
    };

    // 3. 初始化 WiFi
    lisa_wifi_init(&ops);
    
    // 4. 启动 Shell
    user_shell_start();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    return 0;
}
```

## Shell 命令

本示例提供了 `wifi` Shell 命令用于调试和控制：

```bash
# 查看 WiFi 命令帮助
wifi wifi?

# 其他 WiFi 相关命令请参考 Shell 输出
```

## 相关文档

- [LISA WiFi 组件文档](../../../../components/lisa_wifi/README.md) - 了解 WiFi 初始化的详细说明
- [WiFi 双核 AP 核示例](../ap/README.md) - AP 核（WiFi 侧）的示例说明
- [MAC Manager 文档](../../../../modules/mac_manager/README.md) - MAC 地址管理组件
- [WiFi Manager 文档](../../../../modules/wifi_manager/README.md) - WiFi 连接管理组件

## 注意事项

1. **启动顺序**：必须先启动 AP 核，再启动 CP 核
2. **固件地址**：CP 核固件烧录到 0x800000，AP 核固件烧录到 0x0
3. **WiFi 配置**：默认连接 SSID 为 "listenai"，密码为 "listenai"，可在 `main.c` 中修改 `TARGET_WIFI_SSID` 和 `TARGET_WIFI_PWD`
4. **异步初始化**：WiFi 初始化是异步的，需要在 `init_done` 回调中初始化依赖 WiFi 的模块
5. **Shell 调试**：输入 `wifi wifi?` 可查看支持的调试命令。如需手动控制，需注释掉 `wifi_mgr_auto_connect_start`。

