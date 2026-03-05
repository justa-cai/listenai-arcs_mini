# sys_heap 示例

## 功能说明

演示如何使用系统堆管理组件分配和释放内存，包括内部 SRAM 和外部 PSRAM 的基本操作。

## 硬件连接

无需外部连接，系统堆为芯片内部资源。

## 示例内容

1. 从内部 SRAM 分配 256 字节内存
2. 从外部 PSRAM 分配 1024 字节内存
3. 使用 `psram_calloc()` 分配并清零内存
4. 使用 `psram_realloc()` 重新分配内存
5. 查看堆使用统计信息

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
[I][sample] === System Heap Example ===
[I][sample]
1. SRAM allocation:
[I][sample]    Allocated 256 bytes at: 0x2005a100
[I][sample]    Data: Hello from SRAM
[I][sample]
2. PSRAM allocation:
[I][sample]    Allocated 1024 bytes at: 0x28000400
[I][sample]    Data: Hello from PSRAM
[I][sample]
3. PSRAM calloc (zeroed):
[I][sample]    Allocated 10 uint32_t at: 0x28000800
[I][sample]    First element (0): 0
[I][sample]    Modified to: 12345
[I][sample]
4. Memory reallocation:
[I][sample]    64 bytes: Initial data
[I][sample]    Reallocated to 128 bytes at: 0x28000c00
[I][sample]    Data preserved: Initial data
[I][sample]
5. Heap summary:
   [Start]      [End]   [Alloc/BK]    [Free/BK]   [Total/BK] [MaxFree/BK]    [Alloc/B]     [Free/B]  [MinFree/B]
0x20055000 0x2005a000           0            1            1         20480            0        20480        20480
0x28000000 0x28020000           0            1            1        131072            0       131072       131072
[I][sample]
=== Example completed ===
```

## 核心 API

| API | 说明 |
|-----|------|
| `inram_malloc()` | 从内部 SRAM 分配内存 |
| `inram_free()` | 释放内部 SRAM 内存 |
| `psram_malloc()` | 从 PSRAM 分配内存 |
| `psram_calloc()` | 从 PSRAM 分配并清零内存 |
| `psram_realloc()` | 重新分配 PSRAM 内存 |
| `psram_free()` | 释放 PSRAM 内存 |
| `heap_summary_info()` | 打印堆统计信息 |

## 关键代码

```c
/* SRAM 内存分配（4 字节对齐） */
uint8_t *sram_buf = inram_malloc(4, 256);
if (sram_buf) {
    strcpy((char *)sram_buf, "Hello from SRAM");
    inram_free(sram_buf);
}

/* PSRAM 内存分配 */
uint8_t *psram_buf = psram_malloc(1024);
if (psram_buf) {
    strcpy((char *)psram_buf, "Hello from PSRAM");
    psram_free(psram_buf);
}

/* 分配并清零 */
uint32_t *array = psram_calloc(10, sizeof(uint32_t));

/* 重新分配内存 */
char *new_buffer = psram_realloc(buffer, 128);

/* 查看堆统计 */
heap_summary_info();
```

## 内存类型说明

系统堆组件支持两种内存类型：

### 内部 SRAM
- **特点**：访问速度快，容量小（约 20KB）
- **适用场景**：频繁访问的小数据结构、临时变量
- **接口**：`inram_malloc()`, `inram_calloc()`, `inram_realloc()`, `inram_free()`

### 外部 PSRAM
- **特点**：容量大（约 128KB），访问速度较 SRAM 慢
- **适用场景**：大缓冲区、图像数据、音频数据
- **接口**：`psram_malloc()`, `psram_calloc()`, `psram_realloc()`, `psram_free()`

## 配置说明

### 堆大小配置

在 Kconfig 中可配置堆大小（组件默认已配置）：

- **`CONFIG_HEAP_SIZE`**: 内部 SRAM 堆大小，默认 0x5000 (20KB)
- **`CONFIG_PSRAM_HEAP_SIZE`**: PSRAM 堆大小，默认 0x20000 (128KB)

### 对齐要求

- **`inram_malloc(align, size)`**: `align` 参数指定对齐字节数，必须是 2 的幂次方（4, 8, 16, 32 等）
- **`psram_malloc(size)`**: 默认 4 字节对齐
- **`psram_malloc_align(align, size)`**: 指定对齐字节数，用于 DMA 等需要特定对齐的场景

## 注意事项

1. **返回值检查**：分配内存后务必检查返回值是否为 NULL，防止使用无效指针
2. **内存释放**：使用完毕后及时释放内存，避免内存泄漏
3. **对齐参数**：`align` 参数必须是 2 的幂次方，否则会导致对齐错误
4. **自动初始化**：系统堆在 main 函数前自动初始化，无需手动调用 `sysheap_init()`
5. **线程安全**：所有堆分配接口都是线程安全的，可在多任务环境下使用
