# FreeType 性能基准测试示例

## 功能说明

演示如何使用 FreeType 库进行各种操作的性能测试。本示例提供了全面的基准测试工具，用于评估不同场景下 FreeType 的性能表现，帮助开发者选择最优的配置和使用方式。

本示例包含 7 个测试场景，覆盖字符加载、批量处理、渲染性能、缓存效率、变换操作、渲染模式和轮廓提取等方面。

## 硬件连接

无需外部连接，FreeType 为纯软件字体渲染库。

## 示例内容

本示例运行以下 7 组基准测试：

### 1. 单字符加载测试
- 测试不同字体大小（12、16、24、32 点）下的字符加载性能
- 测试字符：'A', 'B', 'C', 'D', '1', '2', '3', '4'
- 测量加载时间和内存使用

### 2. 批量字符加载测试
- 测试不同长度文本的加载性能
- 测试用例：
  - 短文本："Hello"（5 个字符）
  - 中等文本："Hello, World!"（13 个字符）
  - 长文本："The quick brown fox jumps over the lazy dog"（43 个字符）

### 3. 渲染性能测试
- 测试不同渲染配置的性能：
  - 默认配置（`FT_LOAD_DEFAULT`）
  - 无微调（`FT_LOAD_NO_HINTING`）
  - 强制自动微调（`FT_LOAD_FORCE_AUTOHINT`）
  - 轻量级目标（`FT_LOAD_TARGET_LIGHT`）

### 4. 字形缓存性能测试
- 测试重复加载相同字符的性能
- 不同迭代次数：1、10、100
- 评估 FreeType 内部缓存机制的效率

### 5. 字形变换测试
- 测试各种变换操作的性能：
  - 旋转 45 度
  - 水平拉伸 2 倍
  - 垂直拉伸 2 倍
  - 倾斜变换

### 6. 渲染模式测试
- 评估不同渲染模式的性能：
  - 普通（抗锯齿，`FT_RENDER_MODE_NORMAL`）
  - 轻量级（`FT_RENDER_MODE_LIGHT`）
  - 单色（`FT_RENDER_MODE_MONO`）
  - LCD 水平 RGB（`FT_RENDER_MODE_LCD`）
  - LCD 垂直 RGB（`FT_RENDER_MODE_LCD_V`）

### 7. 轮廓提取测试
- 测试不同大小字符的轮廓提取性能
- 字体大小：12、24、48、72 点
- 测试字符：'A', 'B', '@', '8'

## 编译运行

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
********Arcs SDK 0.1.0 @ v0.0.23.temp.docs-96-gf56c5084660d********
Running on hart-id: 1
I/elog            [1034:42:44.159 1 elog_async] EasyLogger V2.2.99 is initialize success.

FreeType Benchmark Started

=== Single Character Loading Benchmarks ===
Test Name                                    | Time (ms) | Memory (bytes)
---------------------------------------------|-----------|---------------
Single char load (size 12, char 'A')        | X.XX      | XXXX
Single char load (size 12, char 'B')        | X.XX      | XXXX
...

=== Batch Character Loading Benchmarks ===
Test Name                                    | Time (ms) | Memory (bytes)
---------------------------------------------|-----------|---------------
Batch load (5 chars)                         | X.XX      | XXXX
Batch load (13 chars)                        | X.XX      | XXXX
Batch load (43 chars)                        | X.XX      | XXXX

=== Rendering Performance Benchmarks ===
Test Name                                    | Time (ms) | Memory (bytes)
---------------------------------------------|-----------|---------------
Render with Default                          | X.XX      | XXXX
Render with No Hinting                       | X.XX      | XXXX
Render with Force Autohint                   | X.XX      | XXXX
Render with Light Target                     | X.XX      | XXXX

=== Glyph Cache Performance Benchmarks ===
Test Name                                    | Time (ms) | Memory (bytes)
---------------------------------------------|-----------|---------------
Cache test (5 chars, 1 iterations)           | X.XX      | XXXX
Cache test (5 chars, 10 iterations)          | X.XX      | XXXX
Cache test (5 chars, 100 iterations)         | X.XX      | XXXX
...

=== Glyph Transform Benchmarks ===
Test Name                                    | Time (ms) | Memory (bytes)
---------------------------------------------|-----------|---------------
Transform (Rotate 45 degrees)                 | X.XX      | XXXX
Transform (Scale 2x horizontal)              | X.XX      | XXXX
Transform (Scale 2x vertical)                | X.XX      | XXXX
Transform (Shear)                            | X.XX      | XXXX

=== Render Mode Benchmarks ===
Test Name                                    | Time (ms) | Memory (bytes)
---------------------------------------------|-----------|---------------
Render mode (Normal (anti-aliased))          | X.XX      | XXXX
Render mode (Light)                          | X.XX      | XXXX
Render mode (Monochrome)                     | X.XX      | XXXX
Render mode (LCD (horizontal RGB))           | X.XX      | XXXX
Render mode (LCD (vertical RGB))             | X.XX      | XXXX

=== Outline Extraction Benchmarks ===
Test Name                                    | Time (ms) | Memory (bytes)
---------------------------------------------|-----------|---------------
Outline (size 12, char 'A')                  | X.XX      | XXXX
Outline (size 12, char 'B')                  | X.XX      | XXXX
...

FreeType Benchmark Completed
```

**说明**：
- 输出开头包含系统启动信息和日志系统初始化信息
- 测试结果以表格形式展示，包含测试名称、执行时间（毫秒）和内存使用（字节）
- 每个测试场景都有标题分隔
- 实际数值会根据系统性能和配置而有所不同

## 核心 API

| API | 说明 |
|-----|------|
| `benchmark_collector_init()` | 初始化测试结果收集器 |
| `benchmark_collector_add()` | 添加测试结果 |
| `benchmark_collector_print_all()` | 打印所有测试结果 |
| `benchmark_collector_free()` | 释放测试结果收集器 |
| `benchmark_single_char_load()` | 单字符加载测试 |
| `benchmark_batch_chars_load()` | 批量字符加载测试 |
| `benchmark_glyph_render()` | 字符渲染测试 |
| `benchmark_glyph_cache()` | 字形缓存测试 |
| `benchmark_glyph_transform()` | 字形变换测试 |
| `benchmark_render_mode()` | 渲染模式测试 |
| `benchmark_glyph_outline()` | 轮廓提取测试 |

## 关键代码

```c
/* 初始化测试结果收集器 */
benchmark_collector_t collector;
benchmark_collector_init(&collector);

/* 初始化 FreeType */
FT_Library library;
FT_Init_FreeType(&library);

FT_Face face;
FT_New_Memory_Face(library, data_As_I_Lay_Dying_ttf, 
                   data_As_I_Lay_Dying_ttf_len, 0, &face);

/* 运行单字符加载测试 */
void run_single_char_benchmarks(FT_Face face, benchmark_collector_t* collector) {
    render_config_t configs[] = {
        {12, 300, FT_LOAD_DEFAULT},
        {16, 300, FT_LOAD_DEFAULT},
        {24, 300, FT_LOAD_DEFAULT},
        {32, 300, FT_LOAD_DEFAULT}
    };
    
    char test_chars[] = {'A', 'B', 'C', 'D', '1', '2', '3', '4'};
    
    for (size_t i = 0; i < sizeof(configs)/sizeof(configs[0]); i++) {
        for (size_t j = 0; j < sizeof(test_chars); j++) {
            benchmark_result_t result = benchmark_single_char_load(
                face, &configs[i], test_chars[j]);
            benchmark_collector_add(collector, test_name, &result);
        }
    }
}

/* 运行所有测试 */
run_single_char_benchmarks(face, &collector);
run_batch_char_benchmarks(face, &collector);
run_rendering_benchmarks(face, &collector);
run_cache_benchmarks(face, &collector);
run_transform_benchmarks(face, &collector);
run_render_mode_benchmarks(face, &collector);
run_outline_benchmarks(face, &collector);

/* 打印所有测试结果 */
benchmark_collector_print_all(&collector);

/* 清理资源 */
benchmark_collector_free(&collector);
FT_Done_Face(face);
FT_Done_FreeType(library);
```

## 测试配置说明

### render_config_t 结构

```c
typedef struct {
    int char_size;      // 字符大小（点）
    int dpi;            // 设备分辨率（DPI）
    FT_Int32 load_flags; // 加载标志
} render_config_t;
```

### 加载标志（FT_LOAD_*）

- **`FT_LOAD_DEFAULT`**: 默认加载方式
- **`FT_LOAD_NO_HINTING`**: 禁用微调
- **`FT_LOAD_FORCE_AUTOHINT`**: 强制自动微调
- **`FT_LOAD_TARGET_LIGHT`**: 轻量级渲染目标

### 渲染模式（FT_RENDER_MODE_*）

- **`FT_RENDER_MODE_NORMAL`**: 普通抗锯齿渲染
- **`FT_RENDER_MODE_LIGHT`**: 轻量级渲染
- **`FT_RENDER_MODE_MONO`**: 单色渲染
- **`FT_RENDER_MODE_LCD`**: LCD 水平 RGB 子像素渲染
- **`FT_RENDER_MODE_LCD_V`**: LCD 垂直 RGB 子像素渲染

## 配置说明

### 必需配置

- **`CONFIG_SDK_MODULE_FREETYPE=y`**: 启用 FreeType 模块

### 字体数据

示例使用内置的字体数据文件 `data/As.I.Lay.Dying.ttf`，通过 `simhei_font.c` 编译到程序中。

## 注意事项

1. **测试环境**: 测试结果受系统性能、内存配置、编译器优化等因素影响，不同环境下结果可能差异较大
2. **内存统计**: 内存使用统计功能需要系统支持，某些平台可能无法准确统计
3. **测试时间**: 完整运行所有测试可能需要较长时间，特别是在资源受限的嵌入式系统上
4. **缓存效果**: 字形缓存测试可以评估 FreeType 内部缓存机制的效率，重复加载相同字符时缓存会显著提升性能
5. **渲染模式选择**: 不同渲染模式在质量和性能之间有不同的权衡，应根据实际应用场景选择
6. **变换操作**: 字形变换（旋转、缩放、倾斜）会增加计算开销，应谨慎使用
7. **轮廓提取**: 轮廓提取用于获取字符的矢量路径，适用于需要进一步处理字符形状的场景

