# FreeType 基础使用示例

## 功能说明

演示如何使用 FreeType 库进行字体加载、字符渲染和位图生成。本示例展示了 FreeType 的基本 API 使用方法，包括库初始化、从内存加载字体、设置字符大小、加载和渲染字符、获取字符度量信息以及打印字符位图。

FreeType 是一个用于渲染字体的 C 语言库，支持多种矢量字体和位图字体格式，能够生成高质量的字符图像。

## 硬件连接

无需外部连接，FreeType 为纯软件字体渲染库。

## 示例内容

1. 初始化 FreeType 库
2. 打印 FreeType 库版本信息
3. 从内存加载字体文件（使用内置的 As.I.Lay.Dying.ttf 字体）
4. 打印字体基本信息（字体族、样式、字符数量等）
5. 设置字符大小（16 点，300 DPI）
6. 循环加载并渲染字符 'A' 到 'D'
7. 对每个字符打印度量信息（宽度、高度、bearingX、bearingY、advance）
8. 使用 ASCII 字符打印字符位图（'#' 表示像素，空格表示空白）
9. 清理资源（释放字体和库）

## 编译运行

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
********Arcs SDK 0.1.0 @ v0.0.23.temp.docs-96-gf56c5084660d********
Running on hart-id: 1
I/elog            [1034:42:44.159 1 elog_async] EasyLogger V2.2.99 is initialize success.
[D][sample] FreeType version: 2.13.3
[D][sample] Font family: As I Lay Dying
[D][sample] Font style: Regular
[D][sample] Number of glyphs: XXX
[D][sample] Face flags: 0xXXXX

[D][sample] Character 'A' metrics:
[D][sample] width: XX
[D][sample] height: XX
[D][sample] horiBearingX: XX
[D][sample] horiBearingY: XX
[D][sample] horiAdvance: XX
[D][sample] Bitmap for 'A':
[字符位图，使用 '#' 和空格表示]
[D][sample] 

[D][sample] Character 'B' metrics:
...
[I][sample] FreeType Simple Use Completed
```

**说明**：
- 输出开头包含系统启动信息和日志系统初始化信息
- `FreeType version` 显示库版本号
- `Font family` 和 `Font style` 显示字体信息
- 每个字符的度量信息以像素为单位（已除以 64，因为 FreeType 使用 26.6 定点数格式）
- 字符位图使用 ASCII 字符可视化显示，'#' 表示有像素，空格表示空白

## 核心 API

| API | 说明 |
|-----|------|
| `FT_Init_FreeType()` | 初始化 FreeType 库 |
| `FT_Library_Version()` | 获取 FreeType 库版本 |
| `FT_New_Memory_Face()` | 从内存加载字体文件 |
| `FT_Set_Char_Size()` | 设置字符大小和设备分辨率 |
| `FT_Load_Char()` | 加载并渲染字符 |
| `FT_Done_Face()` | 释放字体对象 |
| `FT_Done_FreeType()` | 释放 FreeType 库 |

## 关键代码

```c
/* 初始化 FreeType 库 */
FT_Library library;
FT_Error error = FT_Init_FreeType(&library);
if (error) {
    CLOGE("Error: Could not init FreeType library\n");
    return -1;
}

/* 获取版本信息 */
FT_Int major, minor, patch;
FT_Library_Version(library, &major, &minor, &patch);
LOGD("FreeType version: %d.%d.%d\n", major, minor, patch);

/* 从内存加载字体 */
FT_Face face;
error = FT_New_Memory_Face(library,
                          data_As_I_Lay_Dying_ttf,     // 字体数据
                          data_As_I_Lay_Dying_ttf_len,  // 数据长度
                          0,                            // face index
                          &face);

/* 设置字符大小（16 点，300 DPI）*/
error = FT_Set_Char_Size(
    face,    /* handle to face object           */
    0,       /* char_width in 1/64th of points  */
    16*64,   /* char_height in 1/64th of points */
    300,     /* horizontal device resolution    */
    300);    /* vertical device resolution      */

/* 加载并渲染字符 */
error = FT_Load_Char(face, 'A', FT_LOAD_RENDER);
if (error) {
    CLOGE("Error: Could not load and render glyph\n");
}

/* 获取字符度量信息 */
FT_GlyphSlot glyph = face->glyph;
LOGD("width: %ld\n", glyph->metrics.width / 64);
LOGD("height: %ld\n", glyph->metrics.height / 64);
LOGD("horiAdvance: %ld\n", glyph->metrics.horiAdvance / 64);

/* 打印位图 */
print_bitmap(&glyph->bitmap);

/* 清理资源 */
FT_Done_Face(face);
FT_Done_FreeType(library);
```

## 字符度量说明

FreeType 使用 26.6 定点数格式存储度量值（1 个单位 = 1/64 像素），因此需要除以 64 才能得到像素值：

- **width**: 字符位图宽度（像素）
- **height**: 字符位图高度（像素）
- **horiBearingX**: 水平方向起始位置（相对于原点）
- **horiBearingY**: 垂直方向起始位置（相对于基线）
- **horiAdvance**: 水平方向前进距离（下一个字符的起始位置）

## 配置说明

### 必需配置

- **`CONFIG_SDK_MODULE_FREETYPE=y`**: 启用 FreeType 模块
- **`CONFIG_MAIN_TASK_STACK_SIZE=16384`**: 设置主任务栈大小（FreeType 需要较大的栈空间）

### 字体数据

示例使用内置的字体数据文件 `data/As.I.Lay.Dying.ttf`，通过 `simhei_font.c` 编译到程序中，无需额外配置字体路径。

## 注意事项

1. **内存管理**: 使用 `FT_New_Memory_Face()` 加载字体后，必须使用 `FT_Done_Face()` 释放；使用 `FT_Init_FreeType()` 初始化库后，必须使用 `FT_Done_FreeType()` 释放
2. **错误处理**: 所有 FreeType API 返回 `FT_Error` 类型，应检查返回值是否为 0（成功）
3. **度量值格式**: FreeType 使用 26.6 定点数格式，需要除以 64 才能得到像素值
4. **字符大小设置**: `FT_Set_Char_Size()` 的参数以 1/64 点为单位，例如 16 点需要传入 `16*64`
5. **设备分辨率**: DPI（每英寸点数）影响字符的实际渲染大小，300 DPI 是常见的打印分辨率
6. **位图访问**: 位图数据通过 `face->glyph->bitmap` 访问，`pitch` 是每行的字节数（可能包含对齐填充）
7. **加载标志**: `FT_LOAD_RENDER` 表示加载字符并渲染到位图，其他标志如 `FT_LOAD_DEFAULT` 只加载不渲染


