# sys_heap组件

基于heap_caps 框架的系统堆管理组件,为 ARCS 平台提供统一的多内存类型堆分配和管理接口。

## 功能特性

- **多内存类型支持**: 支持内部 SRAM、外部 PSRAM (缓存/非缓存) 等多种内存堆
- **对齐内存分配**: 所有接口均支持指定内存对齐,适用于 DMA 等硬件需求
- **统一管理**: 基于heap_caps 能力标志系统,统一管理不同类型的内存区域
- **自动初始化**: 系统启动时自动初始化,应用程序开箱即用
- **调试支持**: 提供堆使用统计和摘要信息输出,便于内存调试
- **线程安全**: 底层使用互斥锁保护,支持多线程环境

## 配置选项

在 `prj.conf` 中配置堆大小:

```kconfig
# 启用堆模块 (必需)
CONFIG_MODULE_HEAP=y

# 内部 SRAM 堆 (默认启用)
CONFIG_HEAP=y
CONFIG_HEAP_SIZE=0x5000              # 内部 SRAM 堆大小 (默认 20KB)

# PSRAM 堆 (可选)
CONFIG_PSRAM_HEAP=y                  # 启用 PSRAM 堆
CONFIG_PSRAM_HEAP_SIZE=0x20000       # PSRAM 堆大小 (默认 128KB)

# PSRAM 非缓存堆 (可选,用于 DMA)
CONFIG_PSRAM_NOCACHE_HEAP=y          # 启用 PSRAM 非缓存堆
CONFIG_PSRAM_NOCACHE_HEAP_SIZE=0x1000  # 非缓存堆大小 (必须是 2 的幂次方)

# PSRAM 初始化 (使用 PSRAM 时需启用)
CONFIG_PSRAM_INIT=y                  # 自动初始化 PSRAM 硬件
```

**注意**: `CONFIG_PSRAM_NOCACHE_HEAP_SIZE` 必须是 2 的幂次方 (如 0x1000, 0x2000, 0x4000 等),否则编译时会报错。

## API 接口

### 初始化接口

```c
void sysheap_init(void);
```

**说明**: 由系统启动流程自动调用,应用程序无需手动调用。

### 内部 SRAM 堆接口

```c
void *inram_malloc(size_t align, size_t size);
void *inram_calloc(size_t align, size_t num, size_t size);
void *inram_realloc(void *ptr, size_t size);
void inram_free(void *ptr);
```

**特点**: 内存位于芯片内部 SRAM,访问速度快,但容量有限。

### PSRAM 堆接口

```c
void *psram_malloc(size_t size);                           // 4 字节对齐
void *psram_malloc_align(size_t align, size_t size);       // 指定对齐
void *psram_calloc(size_t num, size_t size);               // 4 字节对齐
void *psram_calloc_align(size_t align, size_t num, size_t size);
void *psram_realloc(void *ptr, size_t size);
void psram_free(void *ptr);
```

**特点**: 内存位于外部 PSRAM,容量大 (通常数 MB),但访问速度较内部 SRAM 慢。

### 外部 RAM 堆接口 (兼容接口)

```c
void *exram_malloc(size_t align, size_t size);
void *exram_calloc(size_t align, size_t num, size_t size);
void *exram_realloc(void *ptr, size_t size);
void exram_free(void *ptr);
```

**说明**: 这些接口内部调用 `psram_xxx` 系列函数,提供向后兼容性。

### 调试接口

```c
void heap_summary_info(void);
```

打印所有堆区域的统计信息,包括已分配/空闲块数量、内存使用量等。

## 使用示例

### 基本内存分配

```c
#include "sysheap.h"

// 1. 分配内部 SRAM (4 字节对齐)
uint8_t *buf_sram = inram_malloc(4, 1024);
if (buf_sram) {
    // 使用内部 SRAM 缓冲区
    inram_free(buf_sram);
}

// 2. 分配 PSRAM (默认 4 字节对齐)
uint8_t *buf_psram = psram_malloc(4096);
if (buf_psram) {
    // 使用 PSRAM 缓冲区
    psram_free(buf_psram);
}

// 3. 分配并清零内存
uint32_t *array = psram_calloc(100, sizeof(uint32_t));  // 分配 100 个 uint32_t,并清零
if (array) {
    // 使用数组
    psram_free(array);
}
```

### DMA 对齐内存分配

```c
#include "sysheap.h"

// DMA 通常需要 32 字节对齐的内存
void dma_example(void)
{
    // 分配 32 字节对齐的 PSRAM 内存用于 DMA
    uint8_t *dma_buf = psram_malloc_align(32, 2048);
    if (dma_buf) {
        // 配置 DMA 传输
        // ...

        psram_free(dma_buf);
    }

    // 或使用内部 SRAM (更快,但容量有限)
    uint8_t *dma_buf_sram = inram_malloc(32, 512);
    if (dma_buf_sram) {
        // ...
        inram_free(dma_buf_sram);
    }
}
```

### 内存重新分配

```c
#include "sysheap.h"

void realloc_example(void)
{
    // 初始分配 1KB
    char *buffer = psram_malloc(1024);
    if (!buffer) {
        return;
    }

    // 使用缓冲区
    strcpy(buffer, "Hello");

    // 需要更大的空间,扩展到 4KB
    char *new_buffer = psram_realloc(buffer, 4096);
    if (new_buffer) {
        buffer = new_buffer;  // 更新指针
        // 原内容 "Hello" 会被保留
        strcat(buffer, " World");
    }

    psram_free(buffer);
}
```

### 调试内存使用情况

```c
#include "sysheap.h"
#include <stdio.h>

void debug_heap_usage(void)
{
    printf("=== Heap Summary ===\n");
    heap_summary_info();

    // 输出示例:
    //    [Start]      [End]   [Alloc/BK]    [Free/BK]   [Total/BK] [MaxFree/BK]    [Alloc/B]     [Free/B]  [MinFree/B]
    // 0x20055000 0x2005a000           10            5           15         2048        12288         8192         4096
    // 0x28000000 0x28020000           25           12           37        16384        98304        32768        16384
}
```

### 根据使用场景选择合适的内存类型

```c
#include "sysheap.h"

void memory_selection_example(void)
{
    // 场景 1: 频繁访问的小数据结构 -> 使用内部 SRAM
    typedef struct {
        int state;
        float value;
    } sensor_data_t;
    sensor_data_t *sensor = inram_malloc(4, sizeof(sensor_data_t));

    // 场景 2: 大缓冲区,访问频率低 -> 使用 PSRAM
    uint8_t *image_buffer = psram_malloc(640 * 480 * 2);  // 307KB 图像缓冲

    // 场景 3: DMA 传输缓冲区 -> 使用对齐的 PSRAM 或 SRAM
    uint8_t *dma_rx_buf = psram_malloc_align(32, 4096);   // 32 字节对齐

    // 场景 4: 大量小对象数组 -> 使用 PSRAM calloc
    int *counters = psram_calloc(1000, sizeof(int));      // 1000 个计数器,初始化为 0

    // 使用完毕后释放
    inram_free(sensor);
    psram_free(image_buffer);
    psram_free(dma_rx_buf);
    psram_free(counters);
}
```

## 内存类型对比

| 内存类型 | 容量 | 速度 | 适用场景 | 分配接口 |
|---------|------|------|---------|---------|
| **内部 SRAM** | 小 (~20KB) | 快 | 频繁访问的数据结构、临时变量 | `inram_xxx()` |
| **PSRAM (缓存)** | 大 (~8MB) | 中 | 大缓冲区、图像数据、音频数据 | `psram_xxx()` |
| **PSRAM (非缓存)** | 中 (~4KB) | 慢 | DMA 描述符、硬件共享内存 | 通过 `heap_caps_xxx()` 高级接口 |

## 底层实现

### 内存能力标志

sysheap 组件基于heap_caps 框架,使用以下能力标志:

```c
// 内部 SRAM: MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL
// PSRAM:     MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM
// 非缓存:    MALLOC_CAP_NOCACHE | MALLOC_CAP_SPIRAM
```

### 堆区域注册

系统启动时,`sysheap_init()` 会注册以下堆区域:

1. **内部 SRAM 堆**: 由 `CONFIG_HEAP_SIZE` 配置,静态分配在 `.noinit` 段
2. **PSRAM 堆**: 由 `CONFIG_PSRAM_HEAP_SIZE` 配置,静态分配在 `.psram.noinit` 段
3. **PSRAM 非缓存堆**: 由 `CONFIG_PSRAM_NOCACHE_HEAP_SIZE` 配置,位于 `.psram.nocache_heap` 段

### 与标准库 malloc 的关系

ARCS 平台使用 newlib,标准库的 `malloc()`、`free()`、`realloc()`、`calloc()` 已被重定向到 `heap_caps_xxx_default()` 函数,默认从外部 PSRAM 堆分配。

```c
// startup/arcs/sysheap.c
void *_malloc_r(struct _reent *_r, size_t size)
{
    return heap_caps_malloc_default(size);  // 使用默认堆 (PSRAM)
}
```

因此:
- `malloc(size)` → 外部 PSRAM 堆
- `psram_malloc(size)` → PSRAM 堆
- `inram_malloc(4, size)` → 内部 SRAM 堆 (显式指定)

## 注意事项

1. **自动初始化**: `sysheap_init()` 由系统启动流程自动调用,应用程序无需手动调用
2. **对齐要求**: 所有 `xxx_malloc()` 接口的 `align` 参数必须是 2 的幂次方 (4, 8, 16, 32 等)
3. **非缓存堆大小**: `CONFIG_PSRAM_NOCACHE_HEAP_SIZE` 必须是 2 的幂次方,且该区域会自动配置为 Device 区域 (非缓存)
4. **PSRAM 初始化**: 使用 PSRAM 堆前,需启用 `CONFIG_PSRAM_INIT=y`,否则 PSRAM 硬件未初始化
5. **内存碎片**: 长时间运行的系统应注意内存碎片问题,可使用 `heap_summary_info()` 监控堆状态
6. **分配失败处理**: 内存分配失败时会触发失败回调 (`heap_caps_failed_alloc_callback`),打印堆信息并触发断点 (`__builtin_trap()`)
7. **线程安全**: 所有接口都是线程安全的,内部使用互斥锁保护
8. **PSRAM 性能**: PSRAM 访问速度约为内部 SRAM 的 1/3~1/2,应避免频繁小块读写
9. **Cache 一致性**: 使用 PSRAM 作为 DMA 缓冲区时,需注意 Cache 一致性,必要时使用非缓存堆或手动刷新 Cache
