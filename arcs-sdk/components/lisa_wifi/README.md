# LISA WiFi 组件

## 简介

LISA WiFi 是一个 WiFi 初始化和管理组件，封装了 WiFi 底层驱动的初始化流程，支持单核和双核两种运行模式，提供统一的 API 接口，简化 WiFi 功能的集成和使用。

## 主要特性

- **统一的初始化接口**：提供 `lisa_wifi_init()` 统一接口，自动适配不同运行模式
- **双核架构支持**：支持 WiFi 协议栈和 LWIP 分别运行在不同核心
- **单核架构支持**：支持 WiFi 和 LWIP 运行在同一核心
- **异步初始化**：WiFi 初始化为异步过程，通过回调通知完成
- **自定义 MAC 地址**：支持用户自定义 MAC 地址获取逻辑
- **自动依赖管理**：通过 Kconfig 自动选择所需的依赖模块

## 架构说明

### 运行模式

LISA WiFi 支持两种运行模式：

#### 1. 单核模式 (WIFI_LWIP_SAME_CORE)

WiFi 协议栈和 LWIP 网络栈运行在同一个核心上。

```
┌─────────────────────────────┐
│         应用层              │
├─────────────────────────────┤
│         LWIP                │
├─────────────────────────────┤
│      LISA WiFi              │
├─────────────────────────────┤
│      WiFi 驱动              │
└─────────────────────────────┘
```

#### 2. 双核模式 (WIFI_LWIP_DIFF_CORE)

WiFi 协议栈和 LWIP 网络栈分别运行在不同核心，通过 IPC 通信。

```
    AP 核 (WiFi)              CP 核 (LWIP)
┌──────────────────┐      ┌──────────────────┐
│   LISA WiFi      │      │   应用层         │
│   (WiFi 侧)      │      ├──────────────────┤
├──────────────────┤      │     LWIP         │
│   WiFi 驱动      │      ├──────────────────┤
│   RF 校准        │      │   LISA WiFi      │
└──────────────────┘      │   (LWIP 侧)      │
         ↕                └──────────────────┘
    IPC 通信
```

## API 参考

### 数据结构

#### lisa_wifi_ops_t

WiFi 操作回调函数集合。

```c
typedef struct {
    custom_mac_func_t custom_mac;   /**< 用户自定义 MAC 地址接口 */
    wifi_init_done_cb_t init_done;  /**< WiFi 初始化完成回调 */
} lisa_wifi_ops_t;
```

**成员说明：**

- `custom_mac`: 自定义 WiFi MAC 地址
  - 函数签名：`int8_t (*custom_mac_func_t)(uint8_t mac_addr[6])`
  - 参数：`mac_addr` - 6 字节的 MAC 地址数组，用户需要填充此数组
  - 返回值：0 成功，非 0 失败
  - **注意**：此函数在双核模式下会被对端核心调用，不要执行耗时操作

- `init_done`: WiFi 初始化完成回调
  - 函数签名：`void (*wifi_init_done_cb_t)(void)`
  - 在 WiFi 初始化完成后被调用
  - 可在此回调中初始化依赖 WiFi 的上层模块

### 核心函数

#### lisa_wifi_init()

初始化 WiFi 功能。

**函数签名：**

根据不同的配置模式，函数签名有所不同：

```c
// 双核模式 - WiFi 协议栈侧 (CONFIG_WIFI_LWIP_DIFF_CORE && CONFIG_WIFI)
int lisa_wifi_init(void);

// 双核模式 - LWIP 侧 (CONFIG_WIFI_LWIP_DIFF_CORE && CONFIG_LWIP)
int lisa_wifi_init(lisa_wifi_ops_t *ops);

// 单核模式 (CONFIG_WIFI_LWIP_SAME_CORE)
int lisa_wifi_init(lisa_wifi_ops_t *ops);
```

**参数：**

- `ops`: WiFi 操作回调函数集合
  - 双核 WiFi 侧：不需要参数，ops 由 LWIP 侧设置
  - 双核 LWIP 侧：可以设置自定义回调
  - 单核模式：可以设置自定义回调

**返回值：**

- `0`: 成功
- 非 `0`: 失败

**注意事项：**

- 此函数是**异步初始化**，函数返回不代表 WiFi 已完全初始化
- WiFi 初始化完成后会调用 `ops->init_done` 回调
- 在双核模式下，必须先启动 AP 核，再启动 CP 核

## 使用示例

### 单核模式示例

```c
#include "lisa_wifi.h"

// 自定义 WiFi MAC 地址
// 用户需要根据实际情况实现此函数，提供自定义的 WiFi MAC 地址
static int8_t custom_wifi_mac(uint8_t mac_addr[6])
{
    // 示例：设置自定义的 WiFi MAC 地址
    // 返回 0 表示成功，非 0 表示失败
    return user_provide_custom_mac(mac_addr, 6);
}

// WiFi 初始化完成回调
static void wifi_init_done_cb(void)
{
    LOGI("WiFi init done");
    // 在这里初始化依赖 WiFi 的上层模块
}

int main(void)
{
    // 配置 WiFi 操作回调
    lisa_wifi_ops_t ops = {
        .custom_mac = custom_wifi_mac,
        .init_done = wifi_init_done_cb,
    };
    
    // 初始化 WiFi
    lisa_wifi_init(&ops);
    
    // 主循环
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    return 0;
}
```

### 双核模式示例

#### AP 核 (WiFi 协议栈侧)

```c
#include "lisa_wifi.h"

int main(void)
{
    // 启动 CP 核
    LOGI("boot cp from address: 0x%x", MEM_CP_FLASH_BASE);
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = MEM_CP_FLASH_BASE;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
    
    // 初始化 WiFi（WiFi 侧不需要 ops 参数）
    lisa_wifi_init();
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    
    return 0;
}
```

#### CP 核 (LWIP 侧)

```c
#include "lisa_wifi.h"

// 自定义 WiFi MAC 地址
// 用户需要根据实际情况实现此函数，提供自定义的 WiFi MAC 地址
static int8_t custom_wifi_mac(uint8_t mac_addr[6])
{
    // 示例：设置自定义的 WiFi MAC 地址
    return user_provide_custom_mac(mac_addr, 6);
}

// WiFi 初始化完成回调
static void wifi_init_done_cb(void)
{
    LOGI("WiFi init done");
    // 在这里初始化依赖 WiFi 的上层模块
}

int main(void)
{
    lisa_wifi_ops_t ops = {
        .custom_mac = custom_wifi_mac,
        .init_done = wifi_init_done_cb,
    };
    
    // 初始化 WiFi（LWIP 侧设置 ops）
    lisa_wifi_init(&ops);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    return 0;
}
```

## 配置说明

### Kconfig 配置

在 `prj.conf` 中启用 LISA WiFi：

```ini
CONFIG_LISA_WIFI=y
```

### 运行模式配置

**单核模式：**

```ini
CONFIG_LISA_WIFI=y
CONFIG_WIFI_LWIP_SAME_CORE=y
```

**双核模式 - WiFi 侧 (AP 核)：**

```ini
CONFIG_LISA_WIFI=y
CONFIG_WIFI_LWIP_DIFF_CORE=y
CONFIG_WIFI=y
CONFIG_ARCS_HAL_WCND_RF_BOARD_VER=1  # 射频板型配置
```

**双核模式 - LWIP 侧 (CP 核)：**

```ini
CONFIG_LISA_WIFI=y
CONFIG_WIFI_LWIP_DIFF_CORE=y
CONFIG_LWIP=y
```

### 自动依赖

LISA WiFi 会根据配置模式自动选择所需的依赖模块：

- **单核模式**：
  - `ARCS_HAL_UTILS`
  
- **双核模式 - WiFi 侧**：
  - `ARCS_HAL_SIMSOCKET`
  - `ARCS_HAL_IPC_HALT_BY_PEER_CORE`
  - `ARCS_HAL_IPC_MRPC_CLIENT_FLASH_IF`
  - `ARCS_HAL_IPC_MRPC_CLIENT_UTILS_S2M`
  - `ARCS_HAL_IPC_MRPC_SERVER_UTILS_M2S`
  - `WIFI_TX_BUF_COPY`
  
- **双核模式 - LWIP 侧**：
  - `ARCS_HAL_FLASH_IF`
  - `ARCS_HAL_IPC_HALT_PEER_CORE`
  - `ARCS_HAL_IPC_FLASH_AGENT`
  - `ARCS_HAL_IPC_MRPC_SERVER_FLASH_IF`
  - `ARCS_HAL_IPC_MRPC_SERVER_UTILS_S2M`
  - `ARCS_HAL_IPC_MRPC_CLIENT_UTILS_M2S`
  - `ARCS_HAL_UTILS`
  - `LWIP_TX_BUF_COPY`

- **所有模式**：
  - `SDK_MODULE_MBEDTLS`

## 注意事项

1. **异步初始化**：`lisa_wifi_init()` 是异步函数，不要在函数返回后立即使用 WiFi 功能，应在 `init_done` 回调中初始化依赖模块。

2. **双核启动顺序**：在双核模式下，必须先启动 AP 核（WiFi 侧），再启动 CP 核（LWIP 侧）。

3. **自定义 MAC 地址函数**：在双核模式下，`custom_mac` 函数会被对端核心通过 IPC 调用，不要在此函数中执行耗时操作或访问不可跨核访问的资源。

4. **射频板型配置**：双核模式 WiFi 侧需要配置 `CONFIG_ARCS_HAL_WCND_RF_BOARD_VER`，通常设置为 `1`。

## 示例代码

完整的示例代码请参考：

- 单核模式：`samples/network/wifi_single_core`
- 双核模式 AP 核：`samples/network/wifi_dual_core/ap`
- 双核模式 CP 核：`samples/network/wifi_dual_core/cp`

