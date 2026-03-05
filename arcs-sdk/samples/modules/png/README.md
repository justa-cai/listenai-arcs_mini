# PNG 解码示例

## 功能说明

演示如何使用 libpng 库从内存缓冲区读取 PNG 数据并解码为 RGB565 格式。本示例展示了如何设置自定义内存读取函数、解析 PNG 文件头、处理各种 PNG 颜色类型和位深度、逐行读取图像数据，以及将 RGB888 转换为 RGB565 格式。

libpng 是一个用于读取和写入 PNG 图像文件的 C 语言库，支持 PNG 规范的所有特性。

## 硬件连接

无需外部连接，PNG 解码为纯软件图像处理库。

## 示例内容

1. 定义 PNG 转换信息结构体（包含宽度、高度、RGB 数据指针和长度）
2. 定义内存读取缓冲区结构体
3. 实现自定义内存读取函数 `png_read_memory`
4. 实现 PNG 到 RGB565 的转换函数：
   - 验证输入参数和 PNG 签名
   - 创建 PNG 读取结构体和信息结构体
   - 设置错误处理（使用 `setjmp`/`longjmp`）
   - 设置自定义内存读取函数
   - 读取 PNG 文件头信息
   - 获取图像尺寸、位深度和颜色类型
   - 设置转换选项（处理 16 位、调色板、灰度、透明度等）
   - 逐行读取图像数据
   - 将每行从 RGB888 转换为 RGB565 格式（小端字节序）
   - 清理资源
5. 使用内置的测试 PNG 数据（256x256 像素）进行转换测试
6. 打印 PNG 数据大小、转换结果和图像尺寸

## 编译运行

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
********Arcs SDK 0.1.0 @ v0.0.23.temp.docs-96-gf56c5084660d********
Running on hart-id: 1
I/elog            [1034:42:44.159 1 elog_async] EasyLogger V2.2.99 is initialize success.
[I][sample] PNG (from memory) to RGB565 converter sample
[I][sample] PNG data size: XXX bytes
[I][sample] Image info: width=256, height=256, bit_depth=8, color_type=2
[I][sample] Row bytes: XXX
[I][sample] Conversion completed successfully
[I][sample] Image dimensions: 256 x 256
```

**说明**：
- 输出开头包含系统启动信息和日志系统初始化信息
- PNG 数据大小以字节为单位
- 图像信息包括宽度、高度、位深度和颜色类型
- 行字节数表示每行 RGB888 数据的字节数
- 转换成功后显示图像尺寸
- 如果转换失败，会输出 "Conversion failed" 或相应的错误信息

## 核心 API

| API | 说明 |
|-----|------|
| `png_create_read_struct()` | 创建 PNG 读取结构体 |
| `png_create_info_struct()` | 创建 PNG 信息结构体 |
| `png_set_read_fn()` | 设置自定义读取函数 |
| `png_sig_cmp()` | 检查 PNG 文件签名 |
| `png_read_info()` | 读取 PNG 文件头信息 |
| `png_get_image_width()` | 获取图像宽度 |
| `png_get_image_height()` | 获取图像高度 |
| `png_get_bit_depth()` | 获取位深度 |
| `png_get_color_type()` | 获取颜色类型 |
| `png_read_row()` | 读取一行图像数据 |
| `png_destroy_read_struct()` | 销毁 PNG 结构体并释放资源 |

## 关键代码

```c
/* 自定义内存读取函数 */
typedef struct {
    const uint8_t *data;
    size_t size;
    size_t offset;
} png_memory_buffer;

static void png_read_memory(png_structp png_ptr, png_bytep data, size_t length)
{
    png_memory_buffer *buffer = (png_memory_buffer*)png_get_io_ptr(png_ptr);
    
    if (buffer->offset + length > buffer->size) {
        png_error(png_ptr, "Read beyond end of buffer");
        return;
    }
    
    memcpy(data, buffer->data + buffer->offset, length);
    buffer->offset += length;
}

/* PNG 到 RGB565 转换函数 */
int png_convert_2_rgb(const uint8_t *png_data, int32_t png_data_len, 
                      png_convert_info *info)
{
    /* 检查 PNG 签名 */
    if (png_sig_cmp(png_data, 0, 8) != 0) {
        return -1;
    }
    
    /* 创建 PNG 结构体 */
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 
                                                    NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    
    /* 设置错误处理 */
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return -1;
    }
    
    /* 设置内存读取函数 */
    png_memory_buffer buffer = {
        .data = png_data,
        .size = png_data_len,
        .offset = 0
    };
    png_set_read_fn(png_ptr, &buffer, png_read_memory);
    
    /* 读取 PNG 信息 */
    png_read_info(png_ptr, info_ptr);
    
    /* 获取图像信息 */
    info->width = png_get_image_width(png_ptr, info_ptr);
    info->height = png_get_image_height(png_ptr, info_ptr);
    int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    int color_type = png_get_color_type(png_ptr, info_ptr);
    
    /* 设置转换选项 */
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);
    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);
    
    png_read_update_info(png_ptr, info_ptr);
    
    /* 获取行字节数 */
    size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    
    /* 分配行缓冲区 */
    png_bytep row = (png_bytep)malloc(rowbytes + 16);
    size_t out_rowbytes = info->width * 2;
    uint8_t *out_row = (uint8_t*)malloc(out_rowbytes + 16);
    
    /* 逐行处理 */
    for (int y = 0; y < info->height; y++) {
        /* 读取一行 */
        png_read_row(png_ptr, row, NULL);
        
        /* 转换为 RGB565 */
        uint8_t *dst = out_row;
        for (int x = 0; x < info->width; x++) {
            png_byte *px = &(row[x * 3]);
            uint16_t r = px[0];
            uint16_t g = px[1];
            uint16_t b = px[2];
            
            /* RGB888 转 RGB565 */
            uint16_t rgb565 = ((r & 0xF8) << 8) | 
                             ((g & 0xFC) << 3) | 
                             (b >> 3);
            
            /* 小端格式存储 */
            *dst++ = rgb565 & 0xFF;
            *dst++ = (rgb565 >> 8) & 0xFF;
        }
    }
    
    /* 清理资源 */
    free(out_row);
    free(row);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    
    return 0;
}
```

## PNG 颜色类型说明

libpng 支持以下颜色类型：

- **`PNG_COLOR_TYPE_GRAY`**: 灰度图像（1 通道）
- **`PNG_COLOR_TYPE_GRAY_ALPHA`**: 带透明度的灰度图像（2 通道）
- **`PNG_COLOR_TYPE_PALETTE`**: 调色板图像（使用颜色表）
- **`PNG_COLOR_TYPE_RGB`**: RGB 图像（3 通道）
- **`PNG_COLOR_TYPE_RGB_ALPHA`**: RGBA 图像（4 通道）

代码中的转换选项会将所有类型统一转换为 RGB888 格式（每像素 3 字节），然后再转换为 RGB565。

## 配置说明

### 必需配置

- **`CONFIG_SDK_MODULE_LIBPNG=y`**: 启用 libpng 模块

### 可选配置

- **`CONFIG_MEM_CONFIG=y`**: 启用内存配置（如果需要）

### 依赖关系

libpng 依赖 zlib 库进行数据解压缩，zlib 通常会自动包含在 libpng 模块中。

## 注意事项

1. **错误处理**: libpng 使用 `setjmp`/`longjmp` 机制进行错误处理，必须在使用 `png_read_*` 函数前设置 `setjmp`，否则错误会导致程序崩溃
2. **内存读取**: 使用 `png_set_read_fn()` 时，PNG 数据必须完整存在于内存中，不能是流式数据
3. **资源清理**: 使用完 PNG 结构体后必须调用 `png_destroy_read_struct()` 释放资源
4. **行缓冲区**: 代码为每行分配独立的输入和输出缓冲区，逐行处理，适合处理大图像，避免一次性加载整个图像到内存
5. **颜色转换**: 代码会将所有 PNG 颜色类型转换为 RGB888，然后再转换为 RGB565，确保输出格式统一
6. **字节序**: RGB565 数据以小端格式存储（低字节在前，高字节在后）
7. **图像尺寸限制**: 代码检查图像尺寸是否在合理范围内（1-4096 像素），超出范围会返回错误
8. **输出数据**: 代码中 `png_convert_info` 结构体定义了 `rgb_data` 字段，但实际代码中并未将转换后的数据保存到该字段，仅逐行处理并释放
9. **位深度处理**: 16 位图像会被转换为 8 位，调色板图像会被转换为 RGB，灰度图像会被转换为 RGB

