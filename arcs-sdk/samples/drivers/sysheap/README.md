# Sysheap 示例

本示例演示 sysheap（系统堆）的内存分配功能，分别展示从 PSRAM 和 SRAM 分配内存空间。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **SRAM 内存分配**：使用 `inram_malloc()` 从 SRAM 分配内存
- **PSRAM 内存分配**：使用 `psram_calloc_align()` 从 PSRAM 分配内存
- **内存释放**：使用 `inram_free()` 和 `psram_free()` 释放内存
- **内存分配验证**：检查分配是否成功并打印地址

### 硬件要求

- ARCS 系列开发板
- USB 转串口工具（用于查看日志输出）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/sysheap
./build.sh
```

构建成功后，会在 `build` 目录下生成 `arcs.bin` 文件。

### 2. 烧录运行

将开发板连接到 PC，执行烧录命令：

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/arcs.bin -C arcs
```

参数说明：
- `-s /dev/ttyUSB0`：串口设备路径（根据实际情况修改）
- `-b 3000000`：烧录波特率
- `0x0`：烧录起始地址
- `build/arcs.bin`：烧录固件路径

> 💡 详细烧录步骤请参考 {ref}`快速开始 - 烧录运行 <flashing>`。

### 3. 查看输出

烧录完成后，复位开发板，通过串口工具查看日志输出。

## 🔧 配置说明

### 项目配置

本示例在 `prj.conf` 中配置：

```conf
CONFIG_MEM_CONFIG=y
```

### 内存分配参数

在源码中定义的参数：

| 参数 | 值 | 说明 |
|------|-----|------|
| `size` | 1024 | 每块内存大小（字节） |
| `align` | 4 | 内存对齐字节数 |
| `num` | 16 | 分配块数 |

## 📋 代码解析

### 关键代码段

#### 1. 内存分配参数定义

```c
size_t size = 1024;     // 每块内存大小：1024 字节
size_t align = 4;       // 4字节对齐
size_t num = 16;        // 分配块数：16 块
```

#### 2. 从 SRAM 分配内存

```c
void malloc_from_sram(void)
{
    void *sram_ptr = inram_malloc(align, size);

    if (sram_ptr == NULL) {
        printf("SRAM malloc failed!\n");
        return;
    } else {
        printf("SRAM malloc success: %p\n", sram_ptr);
    }
    
    inram_free(sram_ptr);
}
```

#### 3. 从 PSRAM 分配内存

```c
void malloc_from_psram(void)
{
    void *psram_ptr = psram_calloc_align(align, num, size);

    if (psram_ptr == NULL) {
        printf("PSRAM malloc failed!\n");
        return;
    } else {
        printf("PSRAM malloc success: %p\n", psram_ptr);
    }
    
    psram_free(psram_ptr);
}
```

#### 4. 主函数

```c
int main(int argc, char **argv)
{
    // sysheap 在 main 函数前初始化
    printf("Hello, world! sysheap\n");
    
    malloc_from_sram();
    malloc_from_psram();
    
    printf("sysheap end.");
    return 0;
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
Hello, world! sysheap
SRAM malloc success: 0x2001321c
PSRAM malloc success: 0x28003378
sysheap end.
```

输出说明：
- `Hello, world! sysheap`：程序启动信息
- `SRAM malloc success: 0x2001321c`：SRAM 内存分配成功，地址为 0x2001321c
- `PSRAM malloc success: 0x28003378`：PSRAM 内存分配成功，地址为 0x28003378
- `sysheap end.`：示例执行完成

## ⚠️ 注意事项

1. **SRAM 内存分配**：
   - 使用 `inram_malloc(align, size)` 分配
   - 参数：对齐字节数、内存大小
   - 使用 `inram_free()` 释放

2. **PSRAM 内存分配**：
   - 使用 `psram_calloc_align(align, num, size)` 分配
   - 参数：对齐字节数、块数、每块大小
   - 使用 `psram_free()` 释放

3. **内存地址范围**：
   - SRAM 地址以 0x20 开头
   - PSRAM 地址以 0x28 开头

4. **返回值检查**：
   - 分配成功返回有效指针
   - 分配失败返回 NULL
   - 应始终检查返回值

5. **内存释放**：
   - 使用完内存后应及时释放
   - 避免内存泄漏

6. **Sysheap 初始化**：
   - sysheap 在 main 函数前自动初始化
   - 无需手动初始化
