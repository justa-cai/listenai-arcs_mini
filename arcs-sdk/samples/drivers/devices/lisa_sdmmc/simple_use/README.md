# LISA SDMMC 基础读写示例

## 功能说明

演示 LISA SDMMC 驱动的基本使用方法，包括设备获取、卡探测、状态查询、磁盘信息读取以及扇区读写操作。

通过本示例可以学习如何正确初始化 SDMMC 设备，查询磁盘容量，并执行基本的扇区读写验证。

## 硬件连接

SDMMC 需要配置以下引脚（参考 `boards/arcs_evb` 配置）：

- **PA6**: SDMMC_CLK（时钟）
- **PA7**: SDMMC_CMD（命令）
- **PA5**: SDMMC_DAT0（数据线 0）
- **PA4**: SDMMC_DAT1（数据线 1）
- **PA9**: SDMMC_DAT2（数据线 2）
- **PA8**: SDMMC_DAT3（数据线 3）

连接 SD/MMC 存储卡或使用板载 eMMC。

## 示例步骤

1. 获取 SDMMC 设备
2. 探测 SD/MMC 卡类型
3. 检查磁盘状态
4. 查询磁盘容量信息（扇区数、扇区大小）
5. 写入测试数据到指定扇区
6. 读取扇区并验证数据正确性

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 关键代码

```c
/* 1. 获取 SDMMC 设备 */
lisa_device_t *sdmmc = lisa_device_get(SDMMC_DEVICE_NAME);
if (!sdmmc) {
    LISA_LOGE(LOG_TAG, "Failed to get sdmmc device");
    return -1;
}

/* 2. 探测 SD/MMC 卡 */
ret = lisa_sdmmc_probe(sdmmc);
if (ret != LISA_DEVICE_OK) {
    LISA_LOGE(LOG_TAG, "Disk init failed: %d", ret);
    return ret;
}

/* 3. 检查磁盘状态 */
if (lisa_sdmmc_status(sdmmc) != LISA_SDMMC_STATUS_OK) {
    LISA_LOGE(LOG_TAG, "Disk not ready");
    return -1;
}

/* 4. 查询磁盘信息 */
uint32_t sector_count, sector_size;
lisa_sdmmc_get_sector_count(sdmmc, &sector_count);
lisa_sdmmc_get_sector_size(sdmmc, &sector_size);

/* 5. 使用 64 字节对齐的缓冲区以获得最佳性能 */
static uint8_t buffer[SECTOR_SIZE] __attribute__((aligned(64)));

/* 6. 写入测试数据（注意：使用偏移扇区避免覆盖文件系统区域） */
uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
ret = lisa_sdmmc_write(sdmmc, test_data, TEST_SECTOR_START, 1);

/* 7. 读取扇区并验证 */
ret = lisa_sdmmc_read(sdmmc, buffer, TEST_SECTOR_START, 1);
if (memcmp(buffer, test_data, sizeof(test_data)) == 0) {
    LISA_LOGI(LOG_TAG, "Read sector %u OK", TEST_SECTOR_START);
}
```

## 预期输出

**终端输出：**
```
I/sdmmc_sample     [1034:42:44.159 1 main] LISA SDMMC Driver Example
I/sdmmc_init      [1034:42:44.249 1 main] Card initialized successfully (Fixed/eMMC)
I/lisa_sdmmc_arcs  [1034:42:44.249 1 main] SDMMC device initialized successfully
I/sdmmc_sample     [1034:42:44.249 1 main] Disk Info:
I/sdmmc_sample     [1034:42:44.249 1 main]   Sector count: 15728639
I/sdmmc_sample     [1034:42:44.249 1 main]   Sector size:  512 bytes
I/sdmmc_sample     [1034:42:44.250 1 main]   Total size:   7679 MB
I/sdmmc_sample     [1034:42:44.254 1 main] Write sector 2048 OK
I/sdmmc_sample     [1034:42:44.255 1 main] Read sector 2048 OK
I/sdmmc_sample     [1034:42:44.255 1 main] LISA SDMMC Sample OK
```

输出说明：
- 第 1 行：示例启动
- 第 2-3 行：成功探测并初始化 eMMC 卡
- 第 4-7 行：显示磁盘容量信息（15728639 个扇区 × 512 字节 ≈ 7679 MB）
- 第 8-9 行：成功完成扇区 2048 的写入和读取验证
- 第 10 行：示例运行成功

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 通过名称获取 SDMMC 设备 |
| `lisa_sdmmc_probe()` | 探测 SD/MMC 卡并初始化 |
| `lisa_sdmmc_status()` | 查询磁盘就绪状态 |
| `lisa_sdmmc_get_sector_count()` | 获取扇区总数 |
| `lisa_sdmmc_get_sector_size()` | 获取扇区大小 |
| `lisa_sdmmc_write()` | 写入扇区数据 |
| `lisa_sdmmc_read()` | 读取扇区数据 |

## 注意事项

1. **缓冲区对齐**: 示例使用 `__attribute__((aligned(64)))` 确保 DMA 缓冲区 64 字节对齐，以获得最佳性能
2. **扇区偏移**: 示例使用扇区 2048 作为测试起点，避免覆盖文件系统可能占用的前部扇区
3. **错误处理**: 每步操作都检查返回值，确保程序健壮性
4. **初始化顺序**: 必须先探测（`lisa_sdmmc_probe()`）再检查状态，最后才能进行读写操作
