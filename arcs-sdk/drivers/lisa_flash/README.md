# Flash 驱动

基于 lisa_device 框架的 Flash 存储设备驱动，为 ARCS 平台提供统一的 Flash 读写擦除接口。

## 功能特性

- **设备支持**: SPI Flash 存储设备（自动检测容量）
- **存储操作**: Flash 读取、写入、擦除操作
- **参数查询**: 获取写块大小、擦除值、设备能力等参数
- **布局查询**: 获取 Flash 页面布局信息（页大小、页数量、总容量）
- **线程安全**: 内部使用互斥锁保护并发访问
- **智能写入**: 自动检测数据位置，避免 Flash 读写冲突
- **动态容量**: 通过读取 JEDEC ID 自动识别 Flash 容量

## 配置选项

在 Kconfig 中启用驱动：

```kconfig
CONFIG_LISA_FLASH_ARCS=y
```

### 常用配置参数

```kconfig
# Flash 扇区大小（擦除单位）
CONFIG_LISA_FLASH_ARCS_ERASE_SECTOR_SIZE=4096

# Flash 最小写入块大小
CONFIG_LISA_FLASH_ARCS_WRITE_BLOCK_SIZE=4

# Flash 数据宽度（1=标准SPI, 2=双SPI, 4=四SPI）
CONFIG_LISA_FLASH_ARCS_DATA_WIDTH=1

# Flash 地址字节数（3=<=16MB, 4=>16MB）
CONFIG_LISA_FLASH_ARCS_ADDR_BYTES=3

# 启用写保护
CONFIG_LISA_FLASH_ARCS_WRITE_PROTECT_ENABLE=y

# Flash 操作时禁用中断
CONFIG_LISA_FLASH_ARCS_DISABLE_INTERRUPTS=y
```

## API 接口

### 读取 Flash 数据

```c
int lisa_flash_read(lisa_device_t *dev, size_t offset, void *data, size_t len);
```

从 Flash 的指定偏移地址读取数据到缓冲区。

**参数**:
- `dev`: Flash 设备指针
- `offset`: 读取起始偏移地址（字节）
- `data`: 数据缓冲区指针
- `len`: 读取长度（字节）

**返回值**:
- `LISA_DEVICE_OK (0)`: 成功
- `LISA_DEVICE_ERR_INVALID`: 参数无效
- `LISA_DEVICE_ERR_NOT_SUPPORT`: 不支持该操作
- `LISA_DEVICE_ERR_IO`: IO 错误

### 写入 Flash 数据

```c
int lisa_flash_write(lisa_device_t *dev, size_t offset, const void *data, size_t len);
```

向 Flash 的指定偏移地址写入数据。

**参数**:
- `dev`: Flash 设备指针
- `offset`: 写入起始偏移地址（字节）
- `data`: 数据缓冲区指针
- `len`: 写入长度（字节）

**返回值**:
- `LISA_DEVICE_OK (0)`: 成功
- `LISA_DEVICE_ERR_INVALID`: 参数无效
- `LISA_DEVICE_ERR_NOT_SUPPORT`: 不支持该操作
- `LISA_DEVICE_ERR_IO`: IO 错误
- `LISA_DEVICE_ERR_NO_MEM`: 内存不足（当数据位于 Flash 区域且需要临时缓冲区时）

### 擦除 Flash 区域

```c
int lisa_flash_erase(lisa_device_t *dev, size_t offset, size_t size);
```

擦除 Flash 的指定区域。擦除后该区域的所有字节值为 0xFF。

**参数**:
- `dev`: Flash 设备指针
- `offset`: 擦除起始偏移地址（字节）
- `size`: 擦除大小（字节）

**返回值**:
- `LISA_DEVICE_OK (0)`: 成功
- `LISA_DEVICE_ERR_INVALID`: 参数无效
- `LISA_DEVICE_ERR_NOT_SUPPORT`: 不支持该操作
- `LISA_DEVICE_ERR_IO`: IO 错误

### 获取 Flash 参数

```c
const lisa_flash_parameters_t *lisa_flash_get_parameters(lisa_device_t *dev);
```

获取 Flash 设备的运行时参数信息，包括写块大小、能力标志、擦除值等。

**参数**:
- `dev`: Flash 设备指针

**返回值**:
- 指向 `lisa_flash_parameters_t` 结构体的指针：成功
- `NULL`: 参数无效或不支持该操作

**参数结构体包含**:
- `write_block_size`: 最小写对齐和大小（字节）
- `caps.no_explicit_erase`: 是否需要显式擦除（SPI Flash 为 false）
- `erase_value`: 擦除后的值（通常为 0xFF）

### 获取 Flash 页面布局

```c
const lisa_flash_pages_layout_t *lisa_flash_page_layout(lisa_device_t *dev, size_t *layout_size);
```

获取 Flash 设备的页面布局数组，描述 Flash 的完整布局信息。

**参数**:
- `dev`: Flash 设备指针
- `layout_size`: 输出参数，返回布局数组的元素数量

**返回值**:
- 指向 `lisa_flash_pages_layout_t` 数组的指针：成功
- `NULL`: 参数无效或不支持该操作

**布局结构体包含**:
- `pages_count`: 此布局段中的页面数量
- `pages_size`: 每个页面的大小（字节）

### 辅助函数

```c
// 从页面布局计算 Flash 总大小
size_t lisa_flash_get_size_from_layout(const lisa_flash_pages_layout_t *layout, size_t layout_size);

// 获取写块大小
size_t lisa_flash_params_get_write_block_size(const lisa_flash_parameters_t *params);

// 获取擦除值
uint8_t lisa_flash_params_get_erase_value(const lisa_flash_parameters_t *params);

// 检查是否需要显式擦除
bool lisa_flash_params_get_no_explicit_erase(const lisa_flash_parameters_t *params);
```

## 使用方法

### 基本步骤

1. **获取设备**: 通过 `lisa_device_get()` 获取 Flash 设备
2. **查询参数**: 调用 `lisa_flash_get_parameters()` 和 `lisa_flash_page_layout()` 获取设备信息
3. **擦除区域**: 调用 `lisa_flash_erase()` 擦除目标区域（写入前必须先擦除）
4. **写入数据**: 调用 `lisa_flash_write()` 写入数据
5. **读取数据**: 调用 `lisa_flash_read()` 读取数据

### 基础使用示例

```c
#include "lisa_flash.h"

// 1. 获取 Flash 设备
lisa_device_t *flash = lisa_device_get("flash0");
if (!lisa_device_ready(flash)) {
    return -1;
}

// 2. 查询 Flash 参数
const lisa_flash_parameters_t *params = lisa_flash_get_parameters(flash);
if (!params) {
    return -1;
}

// 3. 查询页面布局
size_t layout_count = 0;
const lisa_flash_pages_layout_t *layout = lisa_flash_page_layout(flash, &layout_count);
if (!layout) {
    return -1;
}

// 获取页大小和总容量
size_t page_size = layout[0].pages_size;
size_t total_size = lisa_flash_get_size_from_layout(layout, layout_count);
printf("Flash: %zu bytes, page size: %zu bytes\n", total_size, page_size);

// 4. 准备测试数据
uint8_t write_buf[256];
uint8_t read_buf[256];
for (size_t i = 0; i < 256; i++) {
    write_buf[i] = (uint8_t)(i & 0xFF);
}

// 5. 擦除 Flash 区域（必须先擦除才能写入）
size_t offset = 0x100000;  // 1MB 偏移
int ret = lisa_flash_erase(flash, offset, page_size);
if (ret != LISA_DEVICE_OK) {
    printf("Erase failed: %d\n", ret);
    return ret;
}

// 6. 写入数据
ret = lisa_flash_write(flash, offset, write_buf, 256);
if (ret != LISA_DEVICE_OK) {
    printf("Write failed: %d\n", ret);
    return ret;
}

// 7. 读取数据
ret = lisa_flash_read(flash, offset, read_buf, 256);
if (ret != LISA_DEVICE_OK) {
    printf("Read failed: %d\n", ret);
    return ret;
}

// 8. 验证数据
if (memcmp(write_buf, read_buf, 256) == 0) {
    printf("Data verification successful!\n");
} else {
    printf("Data verification failed!\n");
}
```

### 查询 Flash 设备信息

```c
#include "lisa_flash.h"

lisa_device_t *flash = lisa_device_get("flash0");

// 获取 Flash 参数
const lisa_flash_parameters_t *params = lisa_flash_get_parameters(flash);
if (params) {
    size_t write_block = lisa_flash_params_get_write_block_size(params);
    uint8_t erase_value = lisa_flash_params_get_erase_value(params);
    bool no_erase = lisa_flash_params_get_no_explicit_erase(params);

    printf("Write block size: %zu bytes\n", write_block);
    printf("Erase value: 0x%02X\n", erase_value);
    printf("Requires explicit erase: %s\n", no_erase ? "No" : "Yes");
}

// 获取页面布局
size_t layout_count = 0;
const lisa_flash_pages_layout_t *layout = lisa_flash_page_layout(flash, &layout_count);
if (layout) {
    printf("Layout segments: %zu\n", layout_count);

    size_t total_offset = 0;
    for (size_t i = 0; i < layout_count; i++) {
        printf("Segment %zu: %zu pages × %zu bytes (offset: 0x%zx)\n",
               i, layout[i].pages_count, layout[i].pages_size, total_offset);
        total_offset += layout[i].pages_count * layout[i].pages_size;
    }

    size_t total_size = lisa_flash_get_size_from_layout(layout, layout_count);
    printf("Total flash size: %zu bytes (%zu KB)\n", total_size, total_size / 1024);
}
```

### 处理大数据写入

```c
#include "lisa_flash.h"

lisa_device_t *flash = lisa_device_get("flash0");

// 获取页面布局
size_t layout_count = 0;
const lisa_flash_pages_layout_t *layout = lisa_flash_page_layout(flash, &layout_count);
size_t page_size = layout[0].pages_size;

// 准备大数据缓冲区（例如 8KB）
#define DATA_SIZE (8 * 1024)
uint8_t large_data[DATA_SIZE];
for (size_t i = 0; i < DATA_SIZE; i++) {
    large_data[i] = (uint8_t)(i & 0xFF);
}

// 计算需要擦除的页数
size_t offset = 0x200000;  // 2MB 偏移
size_t pages_needed = (DATA_SIZE + page_size - 1) / page_size;
size_t erase_size = pages_needed * page_size;

// 擦除足够的空间
printf("Erasing %zu pages (%zu bytes) at offset 0x%zx\n",
       pages_needed, erase_size, offset);
int ret = lisa_flash_erase(flash, offset, erase_size);
if (ret != LISA_DEVICE_OK) {
    printf("Erase failed: %d\n", ret);
    return ret;
}

// 写入大数据
printf("Writing %zu bytes...\n", DATA_SIZE);
ret = lisa_flash_write(flash, offset, large_data, DATA_SIZE);
if (ret != LISA_DEVICE_OK) {
    printf("Write failed: %d\n", ret);
    return ret;
}

printf("Write completed successfully\n");
```

### 检查 Flash 是否需要擦除

```c
#include "lisa_flash.h"

lisa_device_t *flash = lisa_device_get("flash0");
const lisa_flash_parameters_t *params = lisa_flash_get_parameters(flash);

// 准备数据
uint8_t data[256];
memset(data, 0x55, sizeof(data));

size_t offset = 0x100000;

// 检查是否需要显式擦除
if (!lisa_flash_params_get_no_explicit_erase(params)) {
    // 需要显式擦除（SPI Flash）
    printf("Device requires explicit erase\n");

    // 获取页大小
    size_t layout_count = 0;
    const lisa_flash_pages_layout_t *layout = lisa_flash_page_layout(flash, &layout_count);
    size_t page_size = layout[0].pages_size;

    // 擦除
    lisa_flash_erase(flash, offset, page_size);
} else {
    // 无需显式擦除（设备自动擦除或不需要擦除）
    printf("Device does not require explicit erase\n");
}

// 写入数据
lisa_flash_write(flash, offset, data, sizeof(data));
```

## 硬件配置

### Flash 设备规格

| 参数 | 说明 |
|------|------|
| 设备名称 | flash0 |
| 接口类型 | SPI（支持标准/双/四路模式）|
| 扇区大小 | 4096 字节（默认）|
| 写块大小 | 4 字节（最小写入单位）|
| 擦除值 | 0xFF |
| 容量检测 | 自动通过 JEDEC ID 识别 |

### 地址模式

驱动支持 3 字节和 4 字节地址模式：

| 地址字节数 | 支持容量 | 配置 |
|----------|---------|------|
| 3 字节 | ≤ 16MB | `CONFIG_LISA_FLASH_ARCS_ADDR_BYTES=3` |
| 4 字节 | > 16MB | `CONFIG_LISA_FLASH_ARCS_ADDR_BYTES=4` |

### SPI 数据宽度

| 数据宽度 | 模式 | 配置 |
|---------|------|------|
| 1 | 标准 SPI | `CONFIG_LISA_FLASH_ARCS_DATA_WIDTH=1` |
| 2 | 双 SPI | `CONFIG_LISA_FLASH_ARCS_DATA_WIDTH=2` |
| 4 | 四 SPI | `CONFIG_LISA_FLASH_ARCS_DATA_WIDTH=4` |

## 注意事项

1. **写入前必须擦除**: SPI Flash 必须先擦除目标区域才能写入，写入前必须调用 `lisa_flash_erase()`
2. **擦除对齐**: 擦除操作的 `offset` 和 `size` 必须按照页（扇区）大小对齐，通常为 4096 字节
3. **擦除后值**: Flash 擦除后所有字节值为 0xFF
4. **写入对齐**: 写入操作建议按照 `write_block_size` 对齐，通常为 4 字节
5. **地址范围**: 读写擦除操作的 `offset` 和 `size` 必须在有效的 Flash 容量范围内
6. **数据位置**: 如果写入的数据位于 Flash 物理地址区域，驱动会自动将数据复制到 RAM 缓冲区以避免冲突
7. **线程安全**: 驱动内部已实现线程保护，可在多线程环境中使用
8. **写保护**: 驱动会在写入和擦除操作时自动控制写保护（根据配置）
9. **中断控制**: 可配置在 Flash 操作期间禁用中断，避免中断访问 Flash 导致冲突
10. **设备初始化**: Flash 设备在系统启动时自动初始化，无需手动初始化
11. **容量检测**: 驱动通过读取 JEDEC ID 自动检测 Flash 容量，无需手动配置
12. **页面布局**: SPI Flash 通常为均匀扇区布局，`layout_count` 为 1

