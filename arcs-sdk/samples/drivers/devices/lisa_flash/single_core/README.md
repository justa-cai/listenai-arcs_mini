# Flash 基础读写示例

## 功能说明

本示例演示如何使用 LISA Flash 驱动进行基本的读写擦除操作，包括获取设备、查询信息、擦除、写入、读取和数据验证。

## 硬件连接

无需外部连接。Flash 设备在系统启动时自动初始化，配置参数通过 Kconfig 设置。

## 示例内容

1. 获取已自动初始化的 Flash 设备实例
2. 查询 Flash 参数（写块大小、擦除值、设备能力）
3. 获取页面布局信息（页数、页大小、总容量）
4. 擦除 Flash 目标区域
5. 写入测试数据并读取验证数据完整性

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
Segment 0: 4096 pages x 4096 bytes (offset: 0x0)
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

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 Flash 设备实例 |
| `lisa_flash_get_parameters()` | 获取 Flash 参数（写块大小、擦除值等） |
| `lisa_flash_page_layout()` | 获取页面布局信息 |
| `lisa_flash_erase()` | 擦除 Flash 区域 |
| `lisa_flash_write()` | 写入 Flash 数据 |
| `lisa_flash_read()` | 读取 Flash 数据 |

## 关键代码

```c
/* 设备已自动初始化，直接获取 */
lisa_device_t *flash = lisa_device_get("flash0");

/* 获取参数 */
const lisa_flash_parameters_t *params = lisa_flash_get_parameters(flash);

/* 擦除 */
lisa_flash_erase(flash, offset, page_size);

/* 写入 */
lisa_flash_write(flash, offset, data, size);

/* 读取 */
lisa_flash_read(flash, offset, buffer, size);
```

## 注意事项

1. **自动初始化**：Flash 设备在系统启动时自动初始化，无需调用 `lisa_flash_init()`
2. **地址对齐**：擦除操作的地址和大小必须按照页面大小对齐
3. **写入前擦除**：大多数 Flash 设备在写入前需要先擦除目标区域
4. **地址范围**：确保操作的地址范围在 Flash 的有效范围内
5. **写保护**：如果使能写保护，需要先解除保护才能进行写入和擦除操作
