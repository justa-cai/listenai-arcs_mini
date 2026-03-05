# SDMMC 驱动

基于 lisa_device 框架的 SDMMC 设备驱动，为 ARCS 平台提供统一的 SD/MMC 存储访问接口。

## 功能特性

- **设备支持**: 支持 SD/MMC/eMMC 存储卡，自动识别可移除 TF 卡和固定 eMMC 卡
- **扇区读写**: 支持单扇区和多扇区连续读写操作
- **DMA 优化**: 对齐缓冲区直接 DMA 传输，非对齐缓冲区自动使用 wrap buffer 分块传输
- **参数查询**: 获取扇区数量、扇区大小、擦除块大小等磁盘信息
- **状态管理**: 支持探测、状态查询、同步等磁盘管理操作
- **线程安全**: 内部使用互斥锁保护，可在多线程环境中使用

## 配置选项

在 `prj.conf` 中启用驱动：

```kconfig
CONFIG_LISA_SDMMC_DEVICE=y        # 启用 SDMMC 驱动
```

### 可选配置

```kconfig
CONFIG_LISA_SDMMC_INIT_PRIORITY=60                  # 设备初始化优先级（默认 60）
CONFIG_LISA_SDMMC_SCAN_TIMEOUT_MS=3000              # 卡扫描超时时间（默认 3000ms）
CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE=64       # DMA 缓冲区对齐大小（默认 64 字节）
CONFIG_LISA_SDMMC_ACCESS_WRAP_BUFFER_SIZE=4096      # Wrap 缓冲区大小（默认 4KB）
```

## API 接口

### 探测与状态接口

```c
int lisa_sdmmc_probe(lisa_device_t *dev);
int lisa_sdmmc_status(lisa_device_t *dev);
```

探测 SD/MMC 卡并查询磁盘状态。

**重要**: 必须先调用 `lisa_sdmmc_probe()` 探测磁盘类型，才能使用下面的功能 API。

### 数据传输接口

```c
int lisa_sdmmc_read(lisa_device_t *dev, uint8_t *buff, uint32_t sector, uint32_t count);
int lisa_sdmmc_write(lisa_device_t *dev, const uint8_t *buff, uint32_t sector, uint32_t count);
```

扇区级别的读写操作，支持单扇区和多扇区连续传输。

### 参数查询接口

```c
int lisa_sdmmc_get_sector_count(lisa_device_t *dev, uint32_t *sector_count);
int lisa_sdmmc_get_sector_size(lisa_device_t *dev, uint32_t *sector_size);
```

获取磁盘容量信息。

### 控制接口

```c
int lisa_sdmmc_ioctl(lisa_device_t *dev, uint8_t cmd, void *buff);
int lisa_sdmmc_sync(lisa_device_t *dev);
```

通用 IOCTL 控制接口和缓存同步接口。

## 使用示例

### 基本读写操作

```c
#include "lisa_sdmmc.h"

// 1. 获取磁盘设备
lisa_device_t *sdmmc = lisa_device_get("sdmmc0");
if (!sdmmc) {
    return -1;
}

// 2. 探测 SD/MMC 卡
int ret = lisa_sdmmc_probe(sdmmc);
if (ret != LISA_DEVICE_OK) {
    return ret;
}

// 3. 检查磁盘状态
if (lisa_sdmmc_status(sdmmc) != LISA_SDMMC_STATUS_OK) {
    return -1;
}

// 4. 获取磁盘信息
uint32_t sector_count, sector_size;
lisa_sdmmc_get_sector_count(sdmmc, &sector_count);
lisa_sdmmc_get_sector_size(sdmmc, &sector_size);

// 5. 读取扇区
uint8_t buffer[512] __attribute__((aligned(64)));  // 建议 64 字节对齐
ret = lisa_sdmmc_read(sdmmc, buffer, 0, 1);
if (ret != LISA_DEVICE_OK) {
    // 处理错误
}

// 6. 写入扇区
ret = lisa_sdmmc_write(sdmmc, buffer, 0, 1);
if (ret != LISA_DEVICE_OK) {
    // 处理错误
}
```

### 多扇区操作

```c
// 读取多个连续扇区
#define SECTOR_COUNT 8
uint8_t large_buffer[512 * SECTOR_COUNT] __attribute__((aligned(64)));

// 从扇区 100 开始读取 8 个扇区
ret = lisa_sdmmc_read(sdmmc, large_buffer, 100, SECTOR_COUNT);
if (ret == LISA_DEVICE_OK) {
    // 处理读取的数据
}

// 写入多个连续扇区
ret = lisa_sdmmc_write(sdmmc, large_buffer, 100, SECTOR_COUNT);
if (ret == LISA_DEVICE_OK) {
    // 写入成功
}
```

### 使用 IOCTL 查询磁盘参数

```c
// 获取擦除块大小
uint32_t erase_block_size;
ret = lisa_sdmmc_ioctl(sdmmc, LISA_SDMMC_IOCTL_GET_ERASE_BLOCK_SZ, &erase_block_size);
if (ret == LISA_DEVICE_OK) {
    // 使用擦除块大小
}

// 同步缓存到磁盘
ret = lisa_sdmmc_sync(sdmmc);
// 或使用 IOCTL 方式
ret = lisa_sdmmc_ioctl(sdmmc, LISA_SDMMC_IOCTL_CTRL_SYNC, NULL);
```

## 硬件配置

### 引脚复用配置

SDMMC 驱动在初始化时会自动调用板型目录中定义的 `lisa_sdmmc_pinmux()` 函数，用于配置 SDMMC 设备的引脚复用。

**配置位置**:
- **定义**: `boards/<板型名>/pinmux.c` 中实现 `lisa_sdmmc_pinmux()` 函数
- **声明**: `boards/<板型名>/pinmux.h` 中声明 `void lisa_sdmmc_pinmux()`
- **调用时机**: SDMMC 设备初始化时自动调用

**示例** (参考 `boards/arcs_evb/pinmux.c`):
```c
void lisa_sdmmc_pinmux()
{
    // CLK: PA6 功能 15
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 6, CSK_IOMUX_FUNC_ALTER15);
    // CMD: PA7 功能 15
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 7, CSK_IOMUX_FUNC_ALTER15);
    // DAT0: PA5 功能 15
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5, CSK_IOMUX_FUNC_ALTER15);
    // DAT1: PA4 功能 15
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, CSK_IOMUX_FUNC_ALTER15);
    // DAT2: PA9 功能 15
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 9, CSK_IOMUX_FUNC_ALTER15);
    // DAT3: PA8 功能 15
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 8, CSK_IOMUX_FUNC_ALTER15);
}
```

**注意**:
- 该函数由板型相关代码实现，不同板型的引脚配置可能不同
- SDMMC 需要配置 6 个引脚：CLK、CMD、DAT0-DAT3
- 引脚功能码需参考芯片手册的引脚复用表

## IOCTL 命令

| 命令 | 说明 | buff 参数 |
|------|------|-----------|
| `LISA_SDMMC_IOCTL_CTRL_SYNC` | 同步缓存到磁盘 | NULL |
| `LISA_SDMMC_IOCTL_GET_SECTOR_COUNT` | 获取扇区总数 | `uint32_t*` |
| `LISA_SDMMC_IOCTL_GET_SECTOR_SIZE` | 获取扇区大小 | `uint32_t*` |
| `LISA_SDMMC_IOCTL_GET_ERASE_BLOCK_SZ` | 获取擦除块大小 | `uint32_t*` |

## 磁盘状态

| 状态 | 值 | 说明 |
|------|-----|------|
| `LISA_SDMMC_STATUS_OK` | 0 | 磁盘就绪 |
| `LISA_SDMMC_STATUS_UNINIT` | 1 | 磁盘未初始化 |
| `LISA_SDMMC_STATUS_BUSY` | 2 | 磁盘忙 |
| `LISA_SDMMC_STATUS_NO_MEDIA` | 3 | 无存储介质 |

## 注意事项

1. **初始化顺序**: 必须先调用 `lisa_sdmmc_probe()` 探测磁盘类型，才能进行读写操作
2. **状态检查**: 读写前应调用 `lisa_sdmmc_status()` 确认磁盘状态为 `LISA_SDMMC_STATUS_OK`
3. **缓冲区对齐**: 为获得最佳性能，建议使用 64 字节对齐的缓冲区（`__attribute__((aligned(64)))`）。非对齐缓冲区会自动使用 wrap buffer 分块传输，性能略有下降
4. **扇区大小**: 标准扇区大小为 512 字节，建议使用 `lisa_sdmmc_get_sector_size()` 动态获取
5. **扇区范围**: 写入前应确认扇区号在有效范围内（0 到 sector_count-1），避免数据损坏
6. **文件系统兼容**: 如果使用文件系统（如 FAT32），避免直接操作文件系统占用的扇区
7. **线程安全**: 驱动内部使用互斥锁保护，可在多线程环境下安全使用
8. **错误处理**: 所有 API 返回 `LISA_DEVICE_OK` (0) 表示成功，负值表示错误，应检查返回值
9. **卡检测**: 对于可移除 TF 卡，移除后需重新调用 `lisa_sdmmc_probe()` 重新初始化
10. **数据同步**: 重要数据写入后建议调用 `lisa_sdmmc_sync()` 确保数据已刷新到存储介质
