# WiFi & BLE 单核共存示例

## 功能说明

此示例演示如何在 CP (Communication Processor) 单核上同时运行 WiFi 和 BLE 协议栈，实现无线功能的共存。

主要功能：
1. **协议栈共存**：同时初始化 WiFi 和 BLE 协议栈，共享射频资源。
2. **WiFi 功能**：支持扫描、连接 AP、DHCP 获取 IP 等基础 WiFi 功能。
3. **BLE 功能**：支持 BLE 初始化及基础功能。
4. **NVS 存储**：演示如何初始化 NVS (Non-Volatile Storage) 用于存储网络配置或蓝牙绑定信息。
5. **RF 校准**：执行射频校准以确保最佳无线性能。

## 硬件连接

本示例使用芯片内部 WiFi 和 BLE 外设，无需额外接线。
需通过串口查看日志输出：
- 连接到 PC 串口工具
- 波特率：**921600** (默认)

## 使用场景

适用于以下场景：
- 需要同时使用 WiFi 和 BLE 功能的 IoT 设备（如智能配网、网关设备）。
- 验证单核模式下 WiFi 与 BLE 的共存性能。
- 开发基于 WiFi/BLE 的混合应用。

## 示例步骤

1. **系统初始化**：初始化 Shell, Crypto, Event 等基础组件。
2. **存储初始化**：初始化 Flash 和 NVS 文件系统。
3. **RF 初始化**：
   - 探测射频硬件 (`ls_rf_probe`)。
   - 执行射频校准 (`ls_rf_cali_proc`)。
4. **协议栈启动**：
   - 初始化 WiFi 协议栈 (`ls_wifi_init`)。
   - 初始化 BLE 协议栈 (`ls_ble_init`)。
5. **Shell 交互**：通过串口 Shell 使用 `wifi` 或 `ble` 相关命令（如果已使能）。

## 编译运行

**编译：**

```{eval-rst}
.. include:: /sample_build.rst
```

**烧录：**

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

系统启动后，终端将输出初始化日志：

```
[INFO] app-wifi: EVENT_WIFI_INIT_DONE
...
[INFO] app-wifi: EVENT_WIFI_AP_STARTED
...
```

## 核心 API

| API | 说明 |
|-----|------|
| `arcs_nvs_init()` | 初始化 NVS 存储系统 |
| `ls_rf_probe()` | 探测射频硬件 |
| `ls_rf_cali_proc()` | 执行射频校准 |
| `ls_wifi_init()` | 初始化 WiFi 协议栈 |
| `ls_ble_init()` | 初始化 BLE 协议栈 |

## 关键代码

### 1. 系统与存储初始化

初始化 NVS 以支持配置存储，随后进行 RF 校准。

```c
int main(int argc, char **argv)
{
    atcmd_init();

#if CONFIG_ARCS_HAL_MODULE_SHELL
    shell_init(cli_shell_process);
#endif

    // 初始化 NVS
    arcs_nvs_init();

    // RF 初始化与校准
    ls_rf_probe();
    ls_rf_cali_proc();
    
    // ...
}
```

### 2. 协议栈共存初始化

依次初始化 WiFi 和 BLE 协议栈。

```c
    // 初始化 WiFi
    ls_wifi_init();

    // 初始化 BLE
    ls_ble_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
```

### 3. WiFi 事件处理

```c
static int _app_wifi_event_cb(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    switch (event_id) 
    {
        case EVENT_WIFI_INIT_DONE:
            LISA_LOGI(TAG, "EVENT_WIFI_INIT_DONE");
            break;
        case EVENT_WIFI_CONNECTED:
            LISA_LOGI(TAG, "EVENT_WIFI_CONNECTED");
            net_if_up(net_if_get(WIFI_VIF_STA_IDX));
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
            break;
        // ... 其他事件
    }
    return 0;
}
```

## 注意事项

1. **资源共享**：WiFi 和 BLE 共享同一个射频前端，驱动层会自动处理时分复用（TDM），但在高吞吐量场景下可能会有性能相互影响。
2. **初始化顺序**：建议先进行 RF 校准，再初始化协议栈。
3. **NVS 配置**：本示例使用 Flash 的最后 32KB 作为 NVS 分区，请确保该区域未被其他数据（如固件）覆盖。
