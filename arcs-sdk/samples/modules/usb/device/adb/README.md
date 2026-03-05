# USB Device ADB (Android Debug Bridge) 示例

## 功能说明

演示如何将设备实现为 USB ADB (Android Debug Bridge) 设备，支持通过 ADB 协议与主机进行通信。本示例基于 TinyUSB 库和 ADB 应用类，实现了 ADB 设备枚举和基本命令支持，包括 `adb shell`、`adb push`、`adb pull` 和 `adb ls`。

ADB 是 Android 平台常用的调试工具，本示例允许主机通过标准的 ADB 命令与设备进行交互，执行 shell 命令、传输文件等操作。

## 硬件连接

1. **USB 连接**: 将设备通过 USB 线连接到主机（PC/笔记本电脑）
2. **存储设备**（可选）: 如果使用 `adb push/pull` 功能，需要配置文件系统（如 SD 卡）

## 示例内容

1. **初始化文件系统**: 初始化 LSFS 文件系统（用于 `adb push/pull` 操作）
2. **初始化 USB 设备**: 配置 USB 硬件（时钟、PHY、设备模式）并初始化 TinyUSB
3. **初始化 ADB 功能**:
   - `adb_init()`: 初始化 ADB 设备核心功能
   - `adb_shell_init()`: 初始化 ADB Shell 功能（如果 `CONFIG_ADB_SHELL=y`）
   - `adb_sync_init()`: 初始化 ADB Sync 功能（如果 `CONFIG_ADB_SYNC=y`）
4. **USB 设备任务**: 在 FreeRTOS 任务中运行 `tud_task()` 处理 USB 事件
5. **内存监控**: 主循环中定期打印堆内存使用情况（用于调试）

## 编译运行

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
********Arcs SDK 0.1.0 @ v0.0.23.temp.docs-96-gf56c5084660d********
Running on hart-id: 1
I/elog            [1034:42:44.159 1 elog_async] EasyLogger V2.2.99 is initialize success.
[I][sample] USB CDC Echo example starting...
[I][sample] TinyUSB initialized
[I][sample] USB ADB example running...
[I][sample] [Start]     [End]  [Alloc/BK]  [Free/BK] [Total/BK] [MaxFree/BK] [Alloc/B]  [Free/B]  [MinFree/B]
[I][sample] 0x...       0x...  ...         ...       ...        ...          ...        ...       ...
```

**说明**：
- 输出开头包含系统启动信息和日志系统初始化信息
- ADB 设备初始化后显示 "TinyUSB initialized"
- 主循环中定期显示堆内存使用情况
- 当主机通过 ADB 连接时，设备会响应 ADB 协议消息
- 使用 `adb devices` 命令可以在主机上看到设备列表

## 主机端使用示例

### 查看设备

```bash
adb devices
```

输出示例：
```
List of devices attached
XXXXXXXX        device
```

### ADB Shell

```bash
adb shell
```

进入设备的 shell 环境，可以执行命令。

### ADB Push（上传文件到设备）

```bash
adb push local_file.txt /SD:/adb/remote_file.txt
```

**说明**：
- 如果不使用绝对路径，文件会被推送到 `CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT` 定义的路径（默认 `/SD:/adb/`）
- 示例速率：约 3.8 MB/s

### ADB Pull（从设备下载文件）

```bash
adb pull /SD:/adb/remote_file.txt local_file.txt
```

**说明**：
- 如果不使用绝对路径，文件会从 `CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT` 定义的路径拉取
- 示例速率：约 3.9 MB/s

### ADB LS（列出目录）

```bash
adb shell ls /SD:/adb/
```

## 核心 API

| API | 说明 |
|-----|------|
| `adb_init()` | 初始化 ADB 设备核心功能 |
| `adb_shell_init()` | 初始化 ADB Shell 功能 |
| `adb_sync_init()` | 初始化 ADB Sync 功能（文件传输） |
| `tud_init()` | 初始化 TinyUSB 设备栈 |
| `tud_task()` | 处理 USB 设备事件（必须定期调用） |
| `tud_connect()` / `tud_disconnect()` | 软件连接/断开 USB |

## 关键代码

```c
/* USB 任务 */
void usb_task(void *arg)
{
    // 配置 USB 硬件
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x01;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1;   // Config "B" device
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1; // 16bit mode
    
    // 初始化 ADB
    adb_init();
    
#if CONFIG_ADB_SHELL
    adb_shell_init();
#endif

#if CONFIG_ADB_SYNC
    adb_sync_init();
#endif

    // 初始化 TinyUSB
    tud_disconnect();
    tusb_init();
    tud_connect();
    
    // 处理 USB 事件
    while (1) {
        tud_task();
    }
}

/* 主函数 */
int main(int argc, char **argv)
{
    // 初始化文件系统
    extern int test_lsfs_init(void);
    test_lsfs_init();
    
    // 创建 USB 任务
    xTaskCreate(usb_task, "usb_task", 1024 * 1, NULL, 5, NULL);
    
    // 主循环（打印内存信息）
    while (1) {
        LOGI("USB ADB example running...\n");
        heap_caps_travel(heap_travel_cb1);
        vTaskDelay(1000);
    }
    
    return 0;
}
```

## ADB 协议说明

ADB 使用基于消息的协议，主要消息类型：

- **A_CNXN**: 连接消息，设备向主机发送设备信息
- **A_OPEN**: 打开服务（如 shell、sync）
- **A_OKAY**: 确认消息
- **A_CLSE**: 关闭服务
- **A_WRTE**: 写入数据
- **A_SYNC**: 同步消息（用于文件传输）

## 配置说明

### 必需配置

- **`CONFIG_TINY_USB=y`**: 启用 TinyUSB 库
- **`CONFIG_TINY_USB_USE_CUSTOM_CONFIG_FILE=y`**: 使用自定义 TinyUSB 配置文件
- **`CONFIG_TUSB_APP_CLASS=y`**: 启用 TinyUSB 应用类支持
- **`CONFIG_ADB_SYNC=y`**: 启用 ADB Sync 功能（文件传输）
- **`CONFIG_ADB_SHELL=y`**: 启用 ADB Shell 功能

### 文件系统配置（用于 ADB Sync）

- **`CONFIG_FILE_SYSTEM=y`**: 启用文件系统
- **`CONFIG_FATFS_FILESYSTEM=y`**: 启用 FATFS 文件系统
- **`CONFIG_DISK_DRIVER=y`**: 启用磁盘驱动
- **`CONFIG_DISK_DRIVER_SDMMC=y`**: 启用 SDMMC 磁盘驱动
- **`CONFIG_LSFS=y`**: 启用 LSFS 抽象文件系统
- **`CONFIG_LSFS_FAT=y`**: 启用 LSFS FAT 支持

### ADB 配置选项

- **`CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT`**: ADB push/pull 的默认根目录（默认 `/SD:/adb/`）
- **`CONFIG_ADB_MAX_PAYLOAD_SIZE`**: ADB 最大载荷大小（默认 51200 字节）
- **`CONFIG_ADB_SHELL_BUFFER_SIZE`**: ADB Shell 缓冲区大小（默认 4096 字节）

## 注意事项

1. **USB 任务**: `tud_task()` 必须定期调用（在独立任务中运行），否则 USB 通信会失败
2. **文件系统**: 如果使用 `adb push/pull` 功能，必须正确初始化文件系统和存储设备
3. **默认路径**: `adb push/pull` 命令如果不使用绝对路径，会在 `CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT` 定义的路径下操作
4. **传输速率**: 文件传输速率受 USB 速度、存储设备速度、文件系统性能等因素影响，实际速率可能有所不同
5. **ADB 版本**: 主机上的 ADB 版本应与设备支持的 ADB 协议版本兼容
6. **USB 连接**: 代码使用软件连接/断开（`tud_connect()` / `tud_disconnect()`），允许在初始化完成后再连接主机
7. **FreeRTOS**: 代码在 FreeRTOS 环境中运行，USB 设备任务在独立任务中运行
8. **内存监控**: 主循环中的内存监控功能主要用于调试，实际应用中可以移除
9. **Shell 命令**: `adb shell` 支持的命令取决于 `CONFIG_ADB_SHELL` 的实现，可能支持基本的 shell 命令
10. **文件路径**: 文件路径应使用设备上的文件系统路径格式（如 `/SD:/adb/file.txt`）

