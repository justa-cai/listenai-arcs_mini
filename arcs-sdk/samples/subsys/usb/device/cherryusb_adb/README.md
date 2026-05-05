# USB Device Cherryusb ADB (Android Debug Bridge) 示例

## 功能说明

演示如何将设备实现为 USB ADB (Android Debug Bridge) 设备，支持通过 ADB 协议与主机进行通信。本示例基于 CherryUSB 设备栈和 `components/cherryusb-appclass/adb`，实现了 ADB 设备枚举和基本命令支持，包括 `adb shell`、`adb push`、`adb pull` 和 `adb ls`。

ADB 是 Android 平台常用的调试工具，本示例允许主机通过标准的 ADB 命令与设备进行交互，执行 shell 命令、传输文件等操作。

## 硬件连接

1. **USB 连接**: 将设备通过 USB 线连接到主机（PC/笔记本电脑）
2. **存储设备**（可选）: 如果使用 `adb push/pull` 功能，需要配置文件系统（如 SD 卡）

## 示例内容

1. **初始化文件系统**: 初始化 LSFS 文件系统（用于 `adb push/pull` 操作）
2. **初始化 USB 设备**: 配置 USB 硬件（时钟、PHY、设备模式）并初始化 CherryUSB
3. **初始化 ADB 功能**:
   - `adb_init()`: 初始化 ADB 设备核心功能
   - `adb_shell_init()`: 初始化 ADB Shell 功能（如果 `CONFIG_ADB_SHELL=y`）
   - `adb_sync_init()`: 初始化 ADB Sync 功能（如果 `CONFIG_ADB_SYNC=y`）
4. **注册 raw ADB interface**: 通过 `adb_dev_init_intf()` 注册 CherryUSB vendor bulk endpoint，由 `adb.c` 处理完整 ADB 协议
5. **内存监控**: 主循环中定期打印堆内存使用情况（用于调试）

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

```
********Arcs SDK x.y.z @ vX.Y.Z********
Running on hart-id: 1
I/elog            [...] EasyLogger V2.2.99 is initialize success.
[I][sample] CherryUSB ADB example starting...
[I][sample] CherryUSB ADB initialized
[I][sample] CherryUSB ADB example running...
[I][sample] [Start]     [End]  [Alloc/BK]  [Free/BK] [Total/BK] [MaxFree/BK] [Alloc/B]  [Free/B]  [MinFree/B]
[I][sample] 0x...       0x...  ...         ...       ...        ...          ...        ...       ...
```

**说明**：
- 输出开头包含系统启动信息和日志系统初始化信息
- ADB 设备初始化后显示 `CherryUSB ADB initialized`
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

### ADB Pull（从设备下载文件）

```bash
adb pull /SD:/adb/remote_file.txt local_file.txt
```

**说明**：
- 如果不使用绝对路径，文件会从 `CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT` 定义的路径拉取

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
| `adb_dev_init_intf()` | 注册 CherryUSB raw ADB interface |
| `usbd_desc_register()` | 注册设备/配置/string descriptor 回调 |
| `usbd_initialize()` | 初始化 CherryUSB 设备栈 |

## 关键代码

```c
int cherryusb_adb_start(void)
{
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x01;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;

    usbd_desc_register(g_usb_busid, &cherryusb_adb_descriptor);
    usbd_add_interface(g_usb_busid,
                       adb_dev_init_intf(g_usb_busid, &adb_intf,
                                         g_adb_in_ep, g_adb_out_ep));

    adb_init();
#if CONFIG_ADB_SHELL
    adb_shell_init();
#endif
#if CONFIG_ADB_SYNC
    adb_sync_init();
#endif

    usbd_initialize(g_usb_busid, USBC_BASE, usbd_event_handler);
    return 0;
}

int main(int argc, char **argv)
{
    extern int test_lsfs_init(void);
    test_lsfs_init();

    xTaskCreate(usb_task, "usb_task", 1024 * 2, NULL, 5, NULL);

    while (1) {
        LOGI("CherryUSB ADB example running...\n");
        heap_caps_travel(heap_travel_cb1);
        vTaskDelay(pdMS_TO_TICKS(1000));
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

- **`CONFIG_CHERRYUSB=y`**: 启用 CherryUSB
- **`CONFIG_CHERRYUSB_DEVICE=y`**: 启用 CherryUSB 设备栈
- **`CONFIG_CHERRYUSB_APP_CLASS=y`**: 启用 CherryUSB 应用类
- **`CONFIG_CHERRYUSB_DEVICE_MUSB_LISA=y`**: 选择 LISA MUSB 设备端口
- **`CONFIG_ADB=y`**: 启用 ADB 核心功能
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
- **`CONFIG_ADB_SHELL_BUFFER_SIZE`**: ADB Shell 缓冲区大小

## 注意事项

1. **USB 栈**: 本示例使用 CherryUSB，不再依赖 TinyUSB 的 `tud_task()` 轮询模型
2. **raw ADB transport**: 不启用 `CONFIG_CHERRYUSB_DEVICE_ADB`，而是通过 `adb_device.c` 的 raw vendor endpoint transport 让 `adb.c` 处理完整协议
3. **文件系统**: 如果使用 `adb push/pull` 功能，必须正确初始化文件系统和存储设备
4. **默认路径**: `adb push/pull` 命令如果不使用绝对路径，会在 `CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT` 定义的路径下操作
5. **传输速率**: 文件传输速率受 USB 速度、存储设备速度、文件系统性能等因素影响，实际速率可能有所不同
6. **ADB 版本**: 主机上的 ADB 版本应与设备支持的 ADB 协议版本兼容
7. **FreeRTOS**: 代码在 FreeRTOS 环境中运行，USB 初始化在独立任务中执行
8. **内存监控**: 主循环中的内存监控功能主要用于调试，实际应用中可以移除
9. **Shell 命令**: `adb shell` 支持的命令取决于 `CONFIG_ADB_SHELL` 的实现，可能支持基本的 shell 命令
10. **文件路径**: 文件路径应使用设备上的文件系统路径格式（如 `/SD:/adb/file.txt`）
