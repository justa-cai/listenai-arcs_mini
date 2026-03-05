# WiFi & BT 单核共存示例

## 功能说明

此示例演示如何在 CP (Communication Processor) 单核上同时运行 WiFi 和经典蓝牙(BT)协议栈,实现无线功能的共存。

主要功能:
1. **协议栈共存**:同时初始化 WiFi 和经典蓝牙协议栈,共享射频资源。
2. **WiFi 功能**:支持扫描、连接 AP、DHCP 获取 IP 等基础 WiFi 功能。
3. **BT 功能**:通过 `LISA_BLUETOOTH` 组件支持经典蓝牙初始化及基础功能(包括音频相关功能)。
4. **KV/FS 存储**:在 WiFi 初始化完成回调中初始化文件系统与 `lisa_kv`,用于保存网络配置或蓝牙绑定信息。
5. **RF 校准**:射频校准由底层在启动阶段自动完成,应用无需手动调用。

## 硬件连接

本示例使用芯片内部 WiFi 和 BT 外设,无需额外接线。
需通过串口查看日志输出:
- 连接到 PC 串口工具
- 波特率:**921600** (默认)

## 使用场景

适用于以下场景:
- 需要同时使用 WiFi 和经典蓝牙功能的 IoT 设备(如智能音箱、蓝牙音频设备)。
- 验证单核模式下 WiFi 与经典蓝牙的共存性能。
- 开发基于 WiFi/BT 的混合应用。

## 示例步骤

1. **系统初始化**:初始化 Shell, Crypto, Event 等基础组件。
2. **MAC/网络初始化**:初始化 `mac_manager` 并注册 DHCP 状态回调。
3. **WiFi 协议栈启动**:调用 `lisa_wifi_init(&ops)` 初始化 WiFi,等待 `init_done` 回调。
4. **BT 协议栈启动**:调用 `lisa_bluetooth_init()` 初始化经典蓝牙及音频任务。
5. **Shell 交互**:通过串口 Shell 使用 `wifi` 或 `bt` 相关命令(如果已使能)。

## Kconfig 配置

本示例相关的蓝牙配置如下(见 `prj.conf`):
- `CONFIG_LISA_BLUETOOTH=y` 使能 Lisa Bluetooth 组件。
- `CONFIG_LISA_BLUETOOTH_CLASSIC=y` 使能经典蓝牙。
- `CONFIG_LISA_BLUETOOTH_CLASSIC_AUDIO=y` 使能经典蓝牙音频(A2DP/AVRCP),并自动选中 `BT_AUDIO`/`BT_MUSIC` 依赖。
- `CONFIG_LISA_BLUETOOTH_DEVICE_NAME="xxx"` 设置蓝牙设备名称(默认 `"ARCS"`)。
- `CONFIG_LISA_BLUETOOTH_AUD_TASK_STACK_SIZE` / `CONFIG_LISA_BLUETOOTH_AUD_TASK_PRIORITY` 配置音频任务栈/优先级(默认与 `wifi_bt_demo` 一致)。
- `CONFIG_LISA_BLUETOOTH_AUD_PRO_TASK_STACK_SIZE` / `CONFIG_LISA_BLUETOOTH_AUD_PRO_TASK_PRIORITY` 配置音频处理任务栈/优先级。
- `CONFIG_LISA_BLUETOOTH_WHITE_LIST_ADD` / `CONFIG_LISA_BLUETOOTH_RESOVLE_LIST_ADD` 控制 BLE 白名单/解析列表添加功能(组件默认开启,本示例在 `prj.conf` 中关闭)。

## 编译运行

**编译:**

```{eval-rst}
.. include:: /sample_build.rst
```

**烧录:**

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

系统启动后,终端将输出初始化日志:

```
[INFO] app-wifi: EVENT_WIFI_INIT_DONE
...
INF:a2dp enable cmp!
...
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_wifi_init()` | 初始化 WiFi 协议栈并注册回调 |
| `wifi_mgr_init()` / `wifi_mgr_sta_enable()` | 初始化并使能 WiFi manager |
| `lisa_bluetooth_init()` | 初始化经典蓝牙协议栈及音频任务 |
| `mac_manager_init()` | 初始化 MAC 管理器,提供自定义 MAC |
| `net_dhcp_register_status_callback()` | 注册 DHCP 状态回调 |

## 关键代码

### 1. 系统与存储初始化

本示例不使用 NVS/NVDS,应用启动后初始化 AT 命令、Shell、MAC 管理与 DHCP 回调,然后启动 WiFi/BT 协议栈。

```c
int main(int argc, char **argv)
{
    atcmd_init();
    lisa_shell_init();

    user_mac_manager_init();

    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    lisa_wifi_ops_t ops = {
        .init_done = cb_lisa_wifi_init_done,
        .custom_mac = custom_get_wifi_mac,
    };
    lisa_wifi_init(&ops);

    lisa_bluetooth_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### 2. 协议栈共存初始化

依次初始化 WiFi 和经典蓝牙协议栈。

```c
static void cb_lisa_wifi_init_done(void)
{
    LOGI("lisa_wifi_init_done");

    user_fs_init();
    lisa_kv_init();
    user_wifi_manager_init();
}
```

### 3. WiFi 事件处理

```c
static void dhcp_status_callback(int vif_idx, bool success, uint32_t ip_addr,
                                 uint32_t netmask, uint32_t gateway, void *arg)
{
    if (success) {
        LOGI("DHCP Success on VIF-%d: IP=%d.%d.%d.%d",
             vif_idx,
             ip_addr & 0xff, (ip_addr >> 8) & 0xff,
             (ip_addr >> 16) & 0xff, (ip_addr >> 24) & 0xff);
    } else {
        LOGI("DHCP Failed on VIF-%d", vif_idx);
    }
}

static void wifi_mgr_connection_cb(wifi_mgr_connection_info_t *connection_info, void *arg)
{
    switch (connection_info->status)
    {
        case WIFI_MGR_STA_CONNECTED:
            net_if_up(net_if_get(WIFI_VIF_STA_IDX));
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
            break;
        case WIFI_MGR_STA_DISCONNECTED:
            ls_dhcpc_stop(WIFI_VIF_STA_IDX);
            net_if_down(net_if_get(WIFI_VIF_STA_IDX));
            break;
        default:
            break;
    }
}
```

## 与 WiFi & BLE 单核共存的差异

本示例与 `wifi_ble_single_core` 的主要差异:

1. **蓝牙协议**:
   - `wifi_ble_single_core`: 使用 BLE (低功耗蓝牙)
   - `wifi_bt_single_core`: 使用经典蓝牙 (支持音频等传统蓝牙功能)

2. **库文件**:
   - 额外链接 `libbt.a` 和 `libbt_music.a` 以支持经典蓝牙功能

3. **配置差异** (`bt_config.h`):
   - `BT_EMB_PRESENT = 1` (启用经典蓝牙)
   - `BT_STACK_PRESENT = 1` (启用经典蓝牙协议栈)
   - `BT_MUSIC_PRESENT = 1` (启用蓝牙音频功能)

## 注意事项

1. **资源共享**:WiFi 和经典蓝牙共享同一个射频前端,驱动层会自动处理时分复用(TDM),但在高吞吐量场景下可能会有性能相互影响。
2. **初始化顺序**:RF 校准由底层完成,应用在启动后直接初始化 WiFi/BT 即可。
3. **存储说明**:本示例使用 `lisa_kv` 保存配置,未使用 NVS/NVDS。
4. **内存需求**:经典蓝牙相比 BLE 需要更多内存资源,请根据实际需求调整堆大小配置。
