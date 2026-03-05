# GifLib 基础示例

## 功能说明

演示如何使用 GifLib 库解析 GIF 文件。本示例展示了如何从内存中读取 GIF 数据、解析 GIF 文件结构、获取 GIF 信息和图像帧信息，以及解析扩展块（如图形控制扩展块）。

GifLib 是一个用于操作 GIF 文件的 C 语言库，支持读取 GIF 文件并将其转换为 RGB 位图，也支持将 RGB 位图写入为 GIF 文件。

## 硬件连接

无需外部连接，GifLib 为纯软件 GIF 处理库。

## 示例内容

1. 定义内置的测试 GIF 数据（1x1 像素的简单 GIF，包含黑白两色）
2. 实现自定义内存读取函数 `memory_read_func`
3. 使用 `DGifOpen()` 打开 GIF 文件（使用自定义读取函数从内存读取）
4. 使用 `DGifSlurp()` 解析整个 GIF 文件内容
5. 打印 GIF 基本信息：
   - 逻辑屏幕宽度和高度
   - 颜色分辨率
   - 背景色索引
   - 图像总数
   - 全局调色板信息（颜色数、BitsPerPixel、颜色值）
6. 遍历所有图像帧，打印每帧信息：
   - 图像位置（左上角坐标）
   - 图像宽度和高度
   - 交错标志
   - 局部调色板信息
   - 扩展块信息（功能码、大小）
   - 图形控制扩展块详细信息（处理方式、用户输入、透明度、延迟时间）
7. 使用 `DGifCloseFile()` 关闭 GIF 文件

## 编译运行

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
********Arcs SDK 0.1.0 @ v0.0.23.temp.docs-96-gf56c5084660d********
Running on hart-id: 1
I/elog            [1034:42:44.159 1 elog_async] EasyLogger V2.2.99 is initialize success.
[I][sample] 正在打开GIF数据
[I][sample] 正在解析GIF数据
[I][sample] GIF信息
[I][sample]   宽度: 1
[I][sample]   高度: 1
[I][sample]   颜色分辨率: 1
[I][sample]   背景色索引: 0
[I][sample]   图像总数: 1
[I][sample]   全局调色板
[I][sample]     颜色数: 2
[I][sample]     BitsPerPixel: 1
[I][sample]     颜色#0: RGB(0, 0, 0)
[I][sample]     颜色#1: RGB(255, 255, 255)
[I][sample] GIF包含 1 个图像
[I][sample] 图像 #0
[I][sample]   帧信息
[I][sample]     左上角: (0, 0)
[I][sample]     宽度: 1
[I][sample]     高度: 1
[I][sample]     交错: 否
[I][sample]     无局部调色板
[I][sample] GifLib sample completed successfully
```

**说明**：
- 输出开头包含系统启动信息和日志系统初始化信息
- 示例使用内置的 1x1 像素 GIF 数据，包含黑白两色
- GIF 信息包括逻辑屏幕尺寸、颜色分辨率、背景色、图像数量和调色板信息
- 图像帧信息包括位置、尺寸、交错标志和调色板信息
- 如果 GIF 包含图形控制扩展块，会显示处理方式、透明度、延迟时间等信息

## 核心 API

| API | 说明 |
|-----|------|
| `DGifOpen()` | 打开 GIF 文件（支持自定义读取函数） |
| `DGifSlurp()` | 解析整个 GIF 文件内容到内存 |
| `DGifCloseFile()` | 关闭 GIF 文件并释放资源 |
| `GifErrorString()` | 将错误代码转换为错误描述字符串 |

## 关键代码

```c
// 自定义内存读取函数
typedef struct {
    const unsigned char *data;
    size_t size;
    size_t pos;
} MemoryBuffer;

int memory_read_func(GifFileType *gif, GifByteType *buf, int len) {
    MemoryBuffer *buffer = (MemoryBuffer *)gif->UserData;
    
    if (buffer->pos >= buffer->size) {
        return 0;
    }
    
    int remaining = buffer->size - buffer->pos;
    int read_size = (remaining < len) ? remaining : len;
    
    memcpy(buf, buffer->data + buffer->pos, read_size);
    buffer->pos += read_size;
    
    return read_size;
}

// 打开 GIF 文件
MemoryBuffer buffer = {
    .data = sample_gif,
    .size = sizeof(sample_gif),
    .pos = 0
};

int error = 0;
GifFileType *gif = DGifOpen(&buffer, memory_read_func, &error);
if (!gif) {
    LOGI("无法打开GIF: %s", GifErrorString(error));
    return 1;
}

// 解析 GIF 内容
if (DGifSlurp(gif) != GIF_OK) {
    LOGI("无法读取GIF内容: %s", GifErrorString(gif->Error));
    DGifCloseFile(gif, &error);
    return 1;
}

// 访问 GIF 信息
LOGI("宽度: %d", gif->SWidth);
LOGI("高度: %d", gif->SHeight);
LOGI("图像总数: %d", gif->ImageCount);

// 访问全局调色板
if (gif->SColorMap) {
    LOGI("颜色数: %d", gif->SColorMap->ColorCount);
    for (int i = 0; i < gif->SColorMap->ColorCount; i++) {
        GifColorType color = gif->SColorMap->Colors[i];
        LOGI("颜色#%d: RGB(%d, %d, %d)", i, color.Red, color.Green, color.Blue);
    }
}

// 遍历所有图像帧
for (int i = 0; i < gif->ImageCount; i++) {
    SavedImage *image = &gif->SavedImages[i];
    LOGI("图像 #%d", i);
    LOGI("  宽度: %d", image->ImageDesc.Width);
    LOGI("  高度: %d", image->ImageDesc.Height);
    
    // 访问扩展块
    for (int j = 0; j < image->ExtensionBlockCount; j++) {
        ExtensionBlock *ext = &image->ExtensionBlocks[j];
        if (ext->Function == GRAPHICS_EXT_FUNC_CODE) {
            // 解析图形控制扩展块
            int delay = ext->Bytes[1] | (ext->Bytes[2] << 8);
            LOGI("  延迟时间: %d/100秒", delay);
        }
    }
}

// 关闭 GIF 文件
if (DGifCloseFile(gif, &error) != GIF_OK) {
    LOGI("关闭GIF文件时出错: %s", GifErrorString(error));
    return 1;
}
```

## GIF 数据结构说明

### GifFileType 结构

GIF 文件的主要结构，包含以下重要字段：

- **`SWidth`, `SHeight`**: 逻辑屏幕宽度和高度
- **`SColorResolution`**: 颜色分辨率（位数）
- **`SBackGroundColor`**: 背景色索引
- **`ImageCount`**: 图像帧数量
- **`SColorMap`**: 全局调色板指针
- **`SavedImages`**: 保存的图像数组

### SavedImage 结构

每个图像帧的信息：

- **`ImageDesc`**: 图像描述符（位置、尺寸、交错标志等）
- **`RasterBits`**: 图像像素数据
- **`ExtensionBlocks`**: 扩展块数组
- **`ExtensionBlockCount`**: 扩展块数量

### 扩展块类型

- **`GRAPHICS_EXT_FUNC_CODE`**: 图形控制扩展块，包含处理方式、透明度、延迟时间等信息
- **`COMMENT_EXT_FUNC_CODE`**: 注释扩展块
- **`PLAIN_TEXT_EXT_FUNC_CODE`**: 纯文本扩展块
- **`APPLICATION_EXT_FUNC_CODE`**: 应用程序扩展块

## 配置说明

### 必需配置

- **`CONFIG_SDK_MODULE_GIFLIB=y`**: 启用 GifLib 模块

### 可选配置

- **`CONFIG_MEM_CONFIG=y`**: 启用内存配置（如果需要）

## 注意事项

1. **内存读取函数**: 使用 `DGifOpen()` 时，可以提供自定义读取函数从内存、网络或其他数据源读取 GIF 数据，而不是从文件系统读取
2. **错误处理**: 所有 GifLib API 在失败时会设置错误代码，使用 `GifErrorString()` 可以获取可读的错误描述
3. **资源管理**: 使用 `DGifOpen()` 打开 GIF 后，必须使用 `DGifCloseFile()` 关闭并释放资源
4. **DGifSlurp 使用**: `DGifSlurp()` 会将整个 GIF 文件解析到内存中，适合处理较小的 GIF 文件；对于大文件，可能需要使用流式读取方式
5. **调色板访问**: 图像可以使用全局调色板（`gif->SColorMap`）或局部调色板（`image->ImageDesc.ColorMap`），访问前应检查指针是否为 NULL
6. **扩展块解析**: 扩展块的数据格式取决于功能码，图形控制扩展块（`GRAPHICS_EXT_FUNC_CODE`）的格式是固定的，其他扩展块格式可能不同
7. **像素数据**: `SavedImage->RasterBits` 包含解码后的像素数据，每个像素是一个索引值，需要通过调色板转换为 RGB 颜色
8. **交错图像**: 如果 `ImageDesc.Interlace` 为真，图像是交错存储的，需要按照 GIF 规范的交错顺序进行解码

