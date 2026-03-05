# USB Device MSC (Mass Storage Class) 示例

## 功能说明

演示如何将设备实现为 USB Mass Storage Class (MSC) 设备，将 eMMC/SD 卡作为 USB 存储设备暴露给主机。本示例基于 TinyUSB 库，实现了完整的 MSC 设备功能，包括磁盘容量查询、读写操作、设备挂载/卸载、弹出等。

当设备通过 USB 连接到主机时，主机会将其识别为一个可移动存储设备，用户可以在主机上直接访问 eMMC/SD 卡上的文件系统。

## 硬件连接

1. **USB 连接**: 将设备通过 USB 线连接到主机（PC/笔记本电脑）
2. **存储设备**: 需要 eMMC 或 SD 卡作为存储介质（通过 SDMMC 接口）

## 示例内容

1. **初始化磁盘驱动**: 初始化 SDMMC 接口和磁盘访问层
2. **初始化 USB 设备**: 配置 USB 硬件（时钟、PHY、设备模式）并初始化 TinyUSB
3. **实现 MSC 回调函数**:
   - `tud_mount_cb()` / `tud_umount_cb()`: 设备挂载/卸载回调
   - `tud_suspend_cb()` / `tud_resume_cb()`: USB 总线挂起/恢复回调
   - `tud_msc_inquiry_cb()`: 响应 SCSI INQUIRY 命令，提供厂商、产品 ID 和版本信息
   - `tud_msc_test_unit_ready_cb()`: 响应 SCSI TEST UNIT READY 命令，检查设备是否就绪
   - `tud_msc_capacity_cb()`: 响应 SCSI READ CAPACITY 命令，返回磁盘容量（块数和块大小）
   - `tud_msc_start_stop_cb()`: 响应 SCSI START STOP UNIT 命令，处理磁盘加载/弹出
   - `tud_msc_read10_cb()`: 响应 SCSI READ10 命令，从磁盘读取数据
   - `tud_msc_write10_cb()`: 响应 SCSI WRITE10 命令，向磁盘写入数据
   - `tud_msc_is_writable_cb()`: 检查磁盘是否可写
   - `tud_msc_scsi_cb()`: 处理其他 SCSI 命令
4. **USB 设备任务**: 在 FreeRTOS 任务中运行 `tud_task()` 处理 USB 事件

## 编译运行

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
********Arcs SDK 0.1.0 @ v0.0.23.temp.docs-96-gf56c5084660d********
Running on hart-id: 1
I/elog            [1034:42:44.159 1 elog_async] EasyLogger V2.2.99 is initialize success.
[I][main] Start usb device msc sample
[I][main] user_disk_init success
[D][main] TinyUSB MSC class ready
[D][main] [tud_mount_cb] Device mounted
[D][main] [tud_msc_capacity_cb] block_count: XXXXX, block_size: 512
```

**说明**：
- 输出开头包含系统启动信息和日志系统初始化信息
- 磁盘初始化成功后显示 "user_disk_init success"
- USB 设备初始化后显示 "TinyUSB MSC class ready"
- 当主机识别并挂载设备时，会触发 `tud_mount_cb` 回调，显示 "Device mounted"
- 主机查询磁盘容量时，会显示块数和块大小信息
- 如果磁盘被弹出，会显示 "Device unmounted" 和 "disk ejected"

## 核心 API

| API | 说明 |
|-----|------|
| `tud_init()` | 初始化 TinyUSB 设备栈 |
| `tud_task()` | 处理 USB 设备事件（必须定期调用） |
| `tud_connect()` / `tud_disconnect()` | 软件连接/断开 USB |
| `tud_msc_set_sense()` | 设置 SCSI Sense 代码（用于错误报告） |
| `disk_access_ioctl()` | 磁盘访问控制（获取容量、扇区大小等） |
| `disk_access_read()` | 从磁盘读取数据 |
| `disk_access_write()` | 向磁盘写入数据 |
| `disk_access_status()` | 检查磁盘状态 |

## 关键代码

```c
/* 初始化磁盘 */
static int user_disk_init(void)
{
    lisa_sdmmc_probe(lisa_device_get("sdmmc0"));
    disk_init(NULL);
    return 0;
}

/* 初始化 USB 设备 */
static void user_usbd_msc_init(void)
{
    __HAL_CRM_USB_CLK_ENABLE();
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1; // Config "B" device
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1; // 16bit mode
    
    tud_disconnect();
    tusb_init();
    tud_connect();
}

/* MSC 容量回调 */
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
    const char *pdrv = CONFIG_DISK_SDMMC_VOLUME_NAME;
    uint32_t sector_count = 0;
    uint32_t sector_size = 0;
    
    if (disk_access_ioctl(pdrv, DISK_IOCTL_GET_SECTOR_COUNT, &sector_count) != 0 ||
        disk_access_ioctl(pdrv, DISK_IOCTL_GET_SECTOR_SIZE, &sector_size) != 0) {
        sector_count = 0x0001000; // 默认 4MB
        sector_size = DISK_BLOCK_SIZE;
    }
    
    *block_count = sector_count;
    *block_size = sector_size;
}

/* MSC 读取回调 */
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, 
                          void* buffer, uint32_t bufsize)
{
    const char *pdrv = CONFIG_DISK_SDMMC_VOLUME_NAME;
    
    if (disk_access_status(pdrv) != DISK_STATUS_OK) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00);
        return -1;
    }
    
    int ret = disk_access_read(pdrv, buffer, lba, bufsize / DISK_BLOCK_SIZE);
    if (ret != 0) {
        tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x03, 0x00);
        return -1;
    }
    
    return bufsize;
}

/* MSC 写入回调 */
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, 
                          uint8_t* buffer, uint32_t bufsize)
{
    const char *pdrv = CONFIG_DISK_SDMMC_VOLUME_NAME;
    
    if (disk_access_status(pdrv) != DISK_STATUS_OK) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00);
        return -1;
    }
    
    int ret = disk_access_write(pdrv, buffer, lba, bufsize / DISK_BLOCK_SIZE);
    if (ret != 0) {
        tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x03, 0x00);
        return -1;
    }
    
    return bufsize;
}

/* USB 设备任务 */
static void usb_device_task(void *param)
{
    tud_init(BOARD_TUD_RHPORT);
    while (1) {
        tud_task();
    }
}
```

## SCSI 命令说明

MSC 设备需要响应主机的 SCSI 命令：

- **INQUIRY**: 查询设备信息（厂商、产品 ID、版本）
- **TEST UNIT READY**: 检查设备是否就绪
- **READ CAPACITY**: 读取磁盘容量
- **READ10**: 读取数据块
- **WRITE10**: 写入数据块
- **START STOP UNIT**: 启动/停止设备或加载/弹出介质
- **MODE SENSE**: 查询设备模式参数

## 配置说明

### 必需配置

- **`CONFIG_TINY_USB=y`**: 启用 TinyUSB 库
- **`CONFIG_TINY_USB_USE_CUSTOM_CONFIG_FILE=y`**: 使用自定义 TinyUSB 配置文件
- **`CONFIG_DISK_DRIVER=y`**: 启用磁盘驱动
- **`CONFIG_DISK_DRIVER_SDMMC=y`**: 启用 SDMMC 磁盘驱动

### 可选配置

- **`CONFIG_LOG=y`**: 启用日志输出（用于调试）

## 注意事项

1. **磁盘初始化**: 必须在使用 USB MSC 功能前初始化磁盘驱动和 SDMMC 接口
2. **USB 任务**: `tud_task()` 必须定期调用（建议在独立任务中运行），否则 USB 通信会失败
3. **块大小**: 代码中定义的块大小为 512 字节，这是 MSC 标准的常见块大小
4. **错误处理**: 所有回调函数应正确设置 SCSI Sense 代码以报告错误，否则主机可能无法正确处理错误
5. **磁盘状态**: 在读写操作前应检查磁盘状态，确保磁盘已就绪
6. **弹出功能**: 当磁盘被弹出（`ejected = true`）时，`tud_msc_test_unit_ready_cb()` 会返回 false，阻止主机继续访问
7. **容量查询**: 如果无法获取实际磁盘容量，代码会使用默认值（4MB），实际应用中应确保能正确获取容量
8. **USB 连接**: 代码使用软件连接/断开（`tud_connect()` / `tud_disconnect()`），允许在初始化完成后再连接主机
9. **FreeRTOS**: 代码支持 FreeRTOS 环境，USB 设备任务在独立任务中运行
10. **文件系统**: 主机访问磁盘时，磁盘上应该有有效的文件系统（如 FAT32），否则主机可能无法识别或格式化磁盘

