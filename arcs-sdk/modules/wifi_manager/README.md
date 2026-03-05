# WiFi Manager 组件

## 简介

WiFi Manager 是一个 WiFi 连接管理组件，用于管理 WiFi 连接的生命周期。它提供了 WiFi 扫描、连接、断开、自动重连等功能，并支持 AP 配置的持久化存储。

## 主要特性

- **STA 模式管理**：支持 WiFi Station 模式的启用和禁用
- **连接管理**：提供 WiFi 连接、断开、状态查询等功能
- **扫描功能**：支持扫描周围的 AP 设备
- **事件回调**：提供连接状态变化和扫描完成的事件通知
- **持久化存储**：支持 AP 配置的保存、删除和搜索
- **自动重连**：支持自动连接到已保存的 AP，并具有智能重试机制
- **黑名单机制**：自动记录连接失败的 AP，避免重复尝试
- **灵活的后端**：通过 ops 接口支持不同的 WiFi 驱动、操作系统和存储实现

## 开发流程

TDD 开发流程说明见：`develop.md`

## 架构说明

WiFi Manager 采用分层架构设计：

```
┌─────────────────────────────┐
│       应用层                │
├─────────────────────────────┤
│     WiFi Manager            │
│   (wifi_manager.c)          │
├─────────────────────────────┤
│  WiFi Ops | OS Ops | Mem Ops│
│  (WiFi驱动 | 操作系统 | 内存)│
├─────────────────────────────┤
│    Storage (NVS/KV)         │
└─────────────────────────────┘
```

### WiFi Ops（WiFi 操作）

负责与底层 WiFi 驱动交互，包括扫描、连接、断开等操作。

### OS Ops（操作系统操作）

提供线程、互斥锁、队列等操作系统抽象接口。

### Mem Ops（内存操作）

负责内存的分配和释放。

### Storage（存储）

负责 AP 配置的持久化存储，使用 LISA KV 存储。

## API 参考

### 主要 API

| 函数 | 说明 |
|------|------|
| `wifi_mgr_init()` | 初始化 WiFi Manager |
| `wifi_mgr_deinit()` | 反初始化 WiFi Manager |
| `wifi_mgr_sta_enable()` | 启用 STA 模式 |
| `wifi_mgr_sta_disable()` | 禁用 STA 模式 |
| `wifi_mgr_sta_connect()` | 连接到指定 AP |
| `wifi_mgr_sta_disconnect()` | 断开连接 |
| `wifi_mgr_sta_get_status()` | 获取连接状态 |
| `wifi_mgr_sta_get_connected_info()` | 获取当前连接的 AP 信息 |
| `wifi_mgr_scan_ap()` | 扫描周围的 AP |
| `wifi_mgr_sta_add_connection_cb()` | 添加连接状态变化回调 |
| `wifi_mgr_add_scan_done_cb()` | 添加扫描完成回调 |
| `wifi_mgr_storage_save_ap()` | 保存 AP 配置 |
| `wifi_mgr_storage_delete_ap()` | 删除 AP 配置 |
| `wifi_mgr_storage_search_ap()` | 搜索 AP 配置 |
| `wifi_mgr_auto_connect_start()` | 启动自动连接 |
| `wifi_mgr_auto_connect_stop()` | 停止自动连接 |
| `wifi_mgr_ops_get()` | 获取默认的 ops 配置 |

详细的 API 说明请参考头文件 `wifi_manager/wifi_manager.h`。

## 使用示例

### 基本使用

```c
#include "wifi_manager/wifi_manager.h"

// 连接状态变化回调
static void wifi_mgr_connection_cb(wifi_mgr_connection_info_t *connection_info, void *arg)
{
    printf("WiFi connection status: %d\n", connection_info->status);
}

void init_wifi_manager(void)
{
    // 初始化 WiFi Manager
    wifi_mgr_init(wifi_mgr_ops_get());
    
    // 启用 STA 模式
    wifi_mgr_sta_enable();
    
    // 添加连接状态回调
    wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_cb, NULL);
}
```

### 连接到指定 AP

```c
void connect_to_ap(void)
{
    wifi_mgr_sta_config_t cfg = {
        .ssid = "MyWiFi",
        .pwd = "MyPassword",
    };
    
    int ret = wifi_mgr_sta_connect(&cfg, false);
    if (ret == 0) {
        printf("Connecting to AP...\n");
    } else {
        printf("Failed to connect: %d\n", ret);
    }
}
```

### 扫描 AP

```c
void scan_aps(void)
{
    wifi_mgr_scan_info_t ap_list[10];
    
    int ap_count = wifi_mgr_scan_ap(ap_list, 10, false);
    if (ap_count > 0) {
        printf("Found %d APs:\n", ap_count);
        for (int i = 0; i < ap_count; i++) {
            printf("  SSID: %s, RSSI: %d\n", 
                   ap_list[i].ssid, ap_list[i].rssi);
        }
    }
}
```

### 保存和搜索 AP 配置

```c
void save_and_search_ap(void)
{
    // 保存 AP 配置
    wifi_mgr_sta_config_t cfg = {
        .ssid = "MyWiFi",
        .pwd = "MyPassword",
    };
    wifi_mgr_storage_save_ap(&cfg);
    
    // 搜索所有保存的 AP
    wifi_mgr_sta_config_t list[10];
    int count = wifi_mgr_storage_search_ap(list, 10, SEARCH_ALL, NULL);
    printf("Found %d saved APs\n", count);
    
    // 按 SSID 搜索
    char *target_ssid = "MyWiFi";
    count = wifi_mgr_storage_search_ap(list, 10, SEARCH_BY_SSID, target_ssid);
    printf("Found %d APs with SSID '%s'\n", count, target_ssid);
}
```

### 自动连接

```c
void start_auto_connect(void)
{
    // 保存 AP 配置
    wifi_mgr_sta_config_t cfg = {
        .ssid = "MyWiFi",
        .pwd = "MyPassword",
    };
    wifi_mgr_storage_save_ap(&cfg);
    
    // 启动自动连接
    wifi_mgr_autoconn_config_t autoconn_cfg = {
        .interval_ms = 2000,  // 2 秒重试间隔
    };
    wifi_mgr_auto_connect_start(&autoconn_cfg);
}
```

### 清除所有保存的 AP

```c
void clear_all_saved_aps(void)
{
    wifi_mgr_sta_config_t list[10];
    
    // 搜索所有保存的 AP
    int count = wifi_mgr_storage_search_ap(list, 10, SEARCH_ALL, NULL);
    
    // 删除所有 AP
    for (int i = 0; i < count; i++) {
        printf("Deleting saved AP: ssid=%s\n", list[i].ssid);
        wifi_mgr_storage_delete_ap(&list[i]);
    }
}
```

## 配置说明

### Kconfig 配置

在 `prj.conf` 中启用 WiFi Manager：

```ini
CONFIG_WIFI_MANAGER=y
```

### 线程配置

配置 WiFi Manager 内部线程的参数：

```ini
# 线程栈大小（字节）
CONFIG_WIFI_MGR_THREAD_STACK=4096

# 线程优先级
CONFIG_WIFI_MGR_THREAD_PRIORITY=5
```

### 连接超时

配置连接超时时间：

```ini
# 连接超时时间（毫秒）
CONFIG_WIFI_MGR_CONNECT_TIMEOUT=15000
```

### 存储配置

WiFi Manager 使用 LISA KV 作为存储后端：

```ini
CONFIG_WIFI_MANAGER=y
CONFIG_LISA_KV=y
```

## 自动连接机制

WiFi Manager 的自动连接功能具有以下特性：

### 连接策略

1. **优先级顺序**：按照存储中 AP 的保存顺序尝试连接
2. **智能重试**：连接失败后等待指定间隔再次尝试
3. **黑名单机制**：连续失败 3 次的 AP 会被加入黑名单，不再尝试连接
4. **记忆功能**：记住上次尝试的 AP，避免重复尝试同一个 AP

### 失败处理

- 连接失败时，自动尝试下一个保存的 AP
- 所有 AP 都尝试失败后，等待重试间隔再重新开始
- 黑名单中的 AP 会被跳过

### 成功处理

- 连接成功后，重置该 AP 的失败计数
- 停止自动连接流程
- 触发连接成功回调

## 注意事项

1. **初始化顺序**：必须先调用 `wifi_mgr_init()` 再使用其他功能
2. **STA 模式**：大部分功能需要先调用 `wifi_mgr_sta_enable()` 启用 STA 模式
3. **异步模式**：当前版本的 `asynchronous` 参数必须为 `false`
4. **线程安全**：WiFi Manager 内部使用互斥锁保护，可以在多线程环境中使用
5. **存储限制**：存储的 AP 数量受限于存储后端的容量
6. **回调上下文**：事件回调在 WiFi Manager 的内部线程中执行，不要在回调中执行耗时操作
7. **自动连接**：启用自动连接后，WiFi Manager 会自动管理连接，不建议手动调用 `wifi_mgr_sta_connect()`
