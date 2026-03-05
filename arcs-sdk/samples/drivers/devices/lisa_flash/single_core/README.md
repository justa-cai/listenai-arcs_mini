# LISA Flash 读写示例

## 功能说明

本示例演示如何使用 LISA Flash 驱动进行基本的读写擦除操作。

> **注意**: Flash 设备在系统启动时自动初始化，无需手动调用 `lisa_flash_init()`。配置参数通过 Kconfig 设置。

## 示例内容

1. **获取 Flash 设备**
   - 获取已自动初始化的 Flash 设备实例

2. **查询 Flash 信息**
   - 获取 Flash 参数（写块大小、擦除值、设备能力）
   - 获取页面布局信息（页数、页大小、总容量）

3. **执行基本操作**
   - 擦除 Flash 区域
   - 写入测试数据
   - 读取数据
   - 验证数据完整性

## 配置参数

Flash 设备通过 Kconfig 配置，无需在代码中手动设置：

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_LISA_FLASH_DATA_WIDTH` | 数据宽度 (1/2/4) | 1 (标准 SPI) |
| `CONFIG_LISA_FLASH_SCLK_DIV` | SPI 时钟分频 | 2 |
| `CONFIG_LISA_FLASH_ADDR_BYTES` | 地址字节数 (3/4) | 3 |
| `CONFIG_LISA_FLASH_WRITE_PROTECT` | 写保护使能 | y |

## Flash API 使用示例

### 1. 获取设备

```c
// 设备已自动初始化，直接获取即可
lisa_device_t *flash = lisa_device_get("flash0");
if (!flash || !lisa_device_ready(flash)) {
    // 错误处理
}
```

### 2. 查询设备信息

```c
// 获取参数
const lisa_flash_parameters_t *params = lisa_flash_get_parameters(flash);
size_t write_block = lisa_flash_params_get_write_block_size(params);
uint8_t erase_val = lisa_flash_params_get_erase_value(params);

// 获取布局
size_t layout_count = 0;
const lisa_flash_pages_layout_t *layout = lisa_flash_page_layout(flash, &layout_count);
size_t total_size = lisa_flash_get_size_from_layout(layout, layout_count);
```

### 3. 读写操作

```c
// 擦除
lisa_flash_erase(flash, offset, page_size);

// 写入
lisa_flash_write(flash, offset, data, size);

// 读取
lisa_flash_read(flash, offset, buffer, size);
```

## 测试参数

- **测试区域偏移**: 0x10000 (64KB)
- **测试数据大小**: 256 字节
- **测试模式**: 擦除 → 写入 → 读取 → 验证

## 编译运行

```bash
cd arcs_sdk/samples/drivers/devices/lisa_flash
mkdir build && cd build
cmake ..
make
```

或使用项目提供的构建脚本（如果有）。

## 预期输出

```
========================================
=== LISA Flash Read/Write Example ===
========================================

1. Getting flash device 'flash0'...
   Flash device 'flash0' is ready and auto-initialized at 0x...

2. Querying flash device information...

=== Flash Device Information ===
Write block size: 1 bytes
Erase value: 0xFF
No explicit erase: No

=== Flash Layout ===
Layout segments: 1
Segment 0: 4096 pages × 4096 bytes (offset: 0x0)
Total flash size: 16777216 bytes (16384 KB)
================================

3. Running read/write test...

=== Flash Read/Write Test ===
Using page size: 4096 bytes

1. Preparing test data...
   Test data prepared: 256 bytes

2. Erasing flash at offset 0x100000, size 4096 bytes...
   Erase successful

3. Writing 256 bytes to flash at offset 0x100000...
   Write successful

4. Reading 256 bytes from flash at offset 0x100000...
   Read successful

5. Verifying data...
   Verification successful! All 256 bytes match.

6. First 16 bytes of data:
   Write: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
   Read:  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F

=== Test Completed Successfully ===

========================================
All tests passed successfully!
========================================
```

## 注意事项

1. **自动初始化**: Flash 设备在系统启动时自动初始化，无需调用 `lisa_flash_init()`
2. **配置方式**: 配置参数通过 Kconfig 设置，不在代码中配置
3. **地址对齐**: 擦除操作的地址和大小必须按照页面大小对齐
4. **写入前擦除**: 大多数 Flash 设备在写入前需要先擦除目标区域
5. **地址范围**: 确保操作的地址范围在 Flash 的有效范围内
6. **写保护**: 如果使能写保护，需要先解除保护才能进行写入和擦除操作
