# WiFi 单核运行示例

## 功能说明

此示例演示如何在单核模式下运行完整的 WiFi 功能。

在单核 WiFi 架构中，WiFi 协议栈和 LWIP 网络栈运行在同一个核心上。

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

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

系统启动后，终端将输出 WiFi 初始化和连接日志：

```
[INFO] app-wifi: custom_get_wifi_mac: 0, XX:XX:XX:XX:XX:XX
[INFO] app-wifi: mgr connection: 1
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
static int8_t custom_get_wifi_mac(uint8_t mac_addr[6])
{
    int8_t ret = 0;

    if (!mac_addr)
        return -1;

    ret = mac_manager_get(m_mac_manager, mac_addr, 6);
    LOGI("custom_get_wifi_mac: %d, %02X:%02X:%02X:%02X:%02X:%02X", 
         ret, mac_addr[0], mac_addr[1], mac_addr[2], 
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

    /* 清除旧的 AP 配置 */
    int count = wifi_mgr_storage_search_ap(list, 10, SEARCH_ALL, NULL);
    for (int i = 0; i < count; i++) {
        LOGI("Deleting saved AP: ssid=%s", list[i].ssid);
        wifi_mgr_storage_delete_ap(&list[i]);
    }

    // 保存 AP 配置并启动自动连接
    wifi_mgr_storage_save_ap(&cfg);
    wifi_mgr_auto_connect_start(&autoconn_cfg);
}
```

### 4. 主函数

```c
int main(int argc, char **argv)
{
    int ret = 0;

    // 1. 初始化 Shell
    ret = lisa_shell_init();
    if (ret != 0) {
        LOGI("Failed to initialize shell (error: %d)\n", ret);
    }

    // 2. 初始化 MAC Manager
    user_mac_manager_init();

    // 3. 配置 WiFi 回调并初始化
    lisa_wifi_ops_t ops = {
        .custom_mac = custom_get_wifi_mac,
    };
    lisa_wifi_init(&ops);

    // 4. 初始化文件系统、KV 存储和 WiFi Manager
    user_fs_init();
    lisa_kv_init();
    user_wifi_manager_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
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

- [LISA WiFi 组件文档](../../../components/lisa_wifi/README.md) - 了解 WiFi 初始化的详细说明
- [MAC Manager 文档](../../../modules/mac_manager/README.md) - MAC 地址管理组件
- [WiFi Manager 文档](../../../modules/wifi_manager/README.md) - WiFi 连接管理组件

## 注意事项

1. **WiFi 配置**：默认连接 SSID 为 "listenai"，密码为 "listenai"，可在 `main.c` 中修改 `TARGET_WIFI_SSID` 和 `TARGET_WIFI_PWD`
2. **Shell 命令**：输入 `wifi wifi?` 可查看支持的 WiFi 调试命令。如需要从 Shell 中控制，请先注释 `wifi_mgr_auto_connect_start`，否则 wifi_manager 会自动连接目标 AP
3. **同步初始化**：单核模式下 WiFi 初始化是同步的，可以在 `lisa_wifi_init()` 返回后直接初始化依赖模块

