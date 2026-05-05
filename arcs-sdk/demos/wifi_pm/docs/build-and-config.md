# 构建与配置

## 构建系统

基于 CMake 的 listenai-cmake 构建框架，包含 Boot 和 App 两个构建目标。

### 构建命令

```bash
# 常规构建
./build.sh

# 清理后构建
./build.sh -p

# 打开 menuconfig 交互式配置
./build.sh -m

# 发布模式构建
./build.sh -r
```

### 构建产物

Boot 通过 `boot.cmake` 脚本将 `boot.bin` 和 `app.bin` 合并为最终固件：

```
  Flash
  0x30000000 ┬──────────────────┐
             │  boot.bin (16KB) │
  0x30004000 ├──────────────────┤
             │  app.bin         │
             │  (代码 + 只读数据) │
  0x30800000 └──────────────────┘
```

### CMakeLists.txt 编译常量

```cmake
PACKET_TX_TIMEOUT_MS=300000U    # MQTT 发送超时 (5 分钟)
PACKET_RX_TIMEOUT_MS=300000U    # MQTT 接收超时 (5 分钟)
```

---

## 项目配置 (prj.conf)

### 内存配置

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `CONFIG_HEAP_SIZE` | `0x08000` | SRAM 堆大小 (32KB) |
| `CONFIG_PSRAM_HEAP` | `y` | 使能 PSRAM 堆 |
| `CONFIG_PSRAM_HEAP_SIZE` | `0x20000` | PSRAM 堆大小 (128KB) |
| `CONFIG_MEM_CONFIG_USE_CUSTOM_FILE` | `y` | 使用自定义 memap.h |
| `CONFIG_ARCS_STARTUP_CUSTOM_LINKER_FILE` | `y` | 使用自定义 system.ld |

### 电源管理配置

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `CONFIG_ARCS_HAL_PM` | `y` | 使能 PM 框架 |
| `CONFIG_ARCS_HAL_PM_KEEP_ALIVE` | `y` | 使能 ARP 保活 (30s 周期) |
| `CONFIG_ARCS_HAL_PM_DEBUG` | `y` | PM 调试日志输出 |
| `CONFIG_ARCS_HAL_PM_CLOSE_AP` | `y` | 睡眠时关闭 AP 域 |
| `CONFIG_ARCS_HAL_PM_UART_WAKEUP` | `y` | 使能 UART 唤醒 |

### WiFi 配置

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `CONFIG_LISA_WIFI` | `y` | 使能 WiFi |
| `CONFIG_WIFI_LWIP_SAME_CORE` | `y` | WiFi 和 LWIP 在同一核运行 |
| `CONFIG_LISA_WIFI_CB_TASK_STACK_SIZE` | `4096` | WiFi 回调任务栈大小 |
| `CONFIG_LWIP_NETAL_TX_BUF_CNT` | `4` | LWIP TX 缓冲区数量 |
| `CONFIG_LWIP_DHCP_RESTORE_LAST_IP` | `y` | DHCP 恢复上次获取的 IP |
| `CONFIG_WIFI_MANAGER` | `n` | 不使用 wifi_mgr，直接调用底层 API |
| `CONFIG_MEM_WIFI_LA_DUMP_SIZE` | `0` | 不预留 WiFi LA dump 空间 |

### MQTT 配置

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `CONFIG_SDK_MODULE_COREMQTT` | `y` | 使能 coreMQTT 库 |

### 串口与日志

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `CONFIG_CONSOLE_UART_NAME` | `"uart1"` | 控制台走 UART1 (避免拉起 UART0 增加功耗) |
| `CONFIG_CONSOLE_UART_BAUDRATE` | `115200` | 串口波特率 |
| `CONFIG_SYSLOG_UART_DEVICE_UART1` | `y` | 日志走 UART1 |
| `CONFIG_SYSLOG_UART_PORT` | `1` | 日志端口号 |
| `CONFIG_SYSLOG_PRINTF_REDIRECT` | `y` | printf 重定向到 syslog |
| `CONFIG_EASYLOGGER_ASYNC_BUF_SIZE` | `4096` | 异步日志缓冲区 (从 10240 降低) |
| `CONFIG_EASYLOGGER_LINE_BUF_SIZE` | `512` | 单行日志缓冲区 (从 1024 降低) |
| `CONFIG_LOG_MODE_ASYNC` | `n` | 禁用异步日志模式 |

### Boot 配置

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `CONFIG_BOOT` | `y` | 使能 Boot |
| `CONFIG_BOOT_FLASH_SIZE` | `0x4000` | Boot 占用 Flash 大小 (16KB) |
| `CONFIG_BOOT_CP_ENTRY` | `0x30004000` | CP 核入口地址 (紧跟 Boot 之后) |
| `CONFIG_BOOT_LOG_LVL_NON` | `y` | Boot 阶段不输出日志 (节省空间和时间) |
| `CONFIG_BOOT_WITH_WATCHDOG` | `n` | Boot 不使能看门狗 |

### 其他

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `CONFIG_MAC_MANAGER` | `y` | 使能 MAC 地址管理 |
| `CONFIG_MAC_MANAGER_MAC_BY_CHIP_ID` | `y` | 根据芯片 ID 生成 MAC |
| `CONFIG_LISA_SHELL` | `n` | 禁用交互式 Shell |
| `CONFIG_FILE_SYSTEM` | `n` | 禁用文件系统 |
| `CONFIG_BACK_TRACE` | `y` | 使能异常回溯 |
| `CONFIG_LISA_ADC` | `y` | 使能 ADC |

---

## 应用层编译开关 (main.c)

```c
#define LOW_POWER_ENABLED   1    // 1=启用低功耗, 0=禁用 (调试时可关闭)
#define MQTT_ENABLED        1    // 1=启用MQTT, 0=仅WiFi连接
```

---

## 内存布局 (memap.h)

| 区域 | 起始地址 | 大小 | 用途 |
|------|---------|------|------|
| ILM | `0x00280000` | 16KB | 指令紧耦合存储器 |
| DLM | `0x00300000` | 8KB | 数据紧耦合存储器 |
| SRAM | `0x20000000` | 320KB | 主 RAM |
| BTRAM | `0x200C0000` | 32KB | BT 堆 |
| LUNA_RAM | `0x200B0000` | 24KB | 双核共享存储 |
| Flash | `0x30004000` | 8MB - 16KB | 代码/只读数据 |
| PSRAM | `0x28000000` | 8MB | 外部 PSRAM |

附加预留：
- WiFi 校准数据：16KB
- WiFi Trace：0KB (已禁用)
- 中断栈：4KB

---

## 链接脚本 (system.ld)

### 关键内存段

| 段名 | 加载位置 | 运行位置 | 说明 |
|------|---------|---------|------|
| `.wifi.noinit` | — | WIFI_RAM | WiFi 校准/共享数据 (NOLOAD) |
| `.init` | ROM | ROM | 向量表 + 启动代码 |
| `.scatab` | ROM | ROM | Scatter Copy/Load 表 |
| `.fast.text` | ROM | SRAM | 中断处理/PM 代码 (需快速执行) |
| `.text` | ROM | ROM | 主代码段 |
| `.rodata` | ROM | ROM | 只读数据 |
| `.data` | ROM | SRAM | 已初始化数据 |
| `.bss` | — | SRAM | 未初始化数据 (NOLOAD) |
| `.heapstack` | — | SRAM | 堆 + 栈 (16 字节对齐) |

### PM 专属段

```text
/* mem_copy 函数：放在 Flash 中，唤醒时在 RAM 恢复之前就可执行 */
.ilm.mem_copy : {
    *(.text.mem_copy)
} >ROM AT>ROM

/* 唤醒时的 RAM 恢复拷贝表 */
.copy_table : {
    /* .fast.text: Flash → SRAM */
    LONG(LOADADDR(.fast.text))
    LONG(ADDR(.fast.text))
    LONG(ADDR(.fast.text) + SIZEOF(.fast.text))
    /* .data: Flash → SRAM */
    LONG(LOADADDR(.data))
    LONG(ADDR(.data))
    LONG(ADDR(.data) + SIZEOF(.data))
} >ROM AT>ROM
```

唤醒时 `pm_sleep_startup()` 遍历此拷贝表，将关键代码段从 Flash 重新加载到 SRAM/ILM。

### WiFi RAM 约束

```text
/* WiFi 外设只能访问 0x20000000 ~ 0x20000000 + 256KB 这段内存 */
ASSERT((_wifi_ram_start >= 0x20000000), "error: ...")
ASSERT((_wifi_ram_end <= 0x20000000 + 256 * 1024), "error: ...")
```

---

## 引脚复用 (IOMux)

```c
// UART1 引脚 (控制台)
IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, CSK_IOMUX_FUNC_ALTER3);  // TX
IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5, CSK_IOMUX_FUNC_ALTER3);  // RX

// ADC 引脚
AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 7, 3);
```
