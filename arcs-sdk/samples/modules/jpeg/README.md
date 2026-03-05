# JPEG 解码示例

## 功能说明

演示如何使用 IJG JPEG 库从内存缓冲区读取 JPEG 数据并解码为 RGB565 格式。本示例展示了如何设置自定义错误处理、从内存读取 JPEG 数据、配置解压缩参数、逐行解码图像数据，以及将 RGB888 转换为 RGB565 格式。

IJG (Independent JPEG Group) JPEG 库是一个广泛使用的 JPEG 图像压缩和解压缩库，支持多种输入输出格式和颜色空间。

## 硬件连接

无需外部连接，JPEG 解码为纯软件图像处理库。

## 示例内容

1. 定义自定义错误处理结构（使用 `setjmp`/`longjmp` 进行错误恢复）
2. 实现自定义错误处理函数 `my_error_exit`
3. 实现 RGB888 到 RGB565 的转换函数
6. 实现 JPEG 到 RGB565 的转换函数：
   - 设置错误处理
   - 创建解压缩对象
   - 指定内存数据源
   - 读取 JPEG 文件头
   - 设置输出颜色空间为 RGB
   - 启动解压缩器
   - 获取图像尺寸
   - 逐行读取并转换像素数据
   - 完成解压缩并释放资源
7. 使用内置的测试 JPEG 数据（1x1 像素）进行转换测试
8. 打印转换结果和图像尺寸
9. 释放 RGB565 数据内存

## 编译运行

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
********Arcs SDK 0.1.0 @ v0.0.23.temp.docs-96-gf56c5084660d********
Running on hart-id: 1
I/elog            [1034:42:44.159 1 elog_async] EasyLogger V2.2.99 is initialize success.
[I][sample] JPEG (from memory) to RGB565 converter sample
[I][sample] Conversion completed successfully
[I][sample] Image dimensions: 1 x 1
```

**说明**：
- 输出开头包含系统启动信息和日志系统初始化信息
- 示例使用内置的 1x1 像素 JPEG 数据进行测试
- 转换成功后显示图像尺寸
- 如果转换失败，会输出 "Conversion failed"

## 核心 API

| API | 说明 |
|-----|------|
| `jpeg_create_decompress()` | 创建 JPEG 解压缩对象 |
| `jpeg_mem_src()` | 指定内存数据源 |
| `jpeg_read_header()` | 读取 JPEG 文件头并获取图像信息 |
| `jpeg_start_decompress()` | 启动解压缩过程 |
| `jpeg_read_scanlines()` | 读取一行或多行扫描线数据 |
| `jpeg_finish_decompress()` | 完成解压缩 |
| `jpeg_destroy_decompress()` | 销毁解压缩对象并释放资源 |
| `jpeg_std_error()` | 获取标准错误处理管理器 |

## 关键代码

```c
/* 自定义错误处理结构 */
struct my_error_mgr {
    struct jpeg_error_mgr pub;    /* "public" fields */
    jmp_buf setjmp_buffer;        /* for return to caller */
};

/* 自定义错误处理函数 */
METHODDEF(void)
my_error_exit(j_common_ptr cinfo)
{
    my_error_ptr myerr = (my_error_ptr) cinfo->err;
    (*cinfo->err->output_message) (cinfo);
    longjmp(myerr->setjmp_buffer, 1);
}

/* RGB888 到 RGB565 转换 */
static uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t rgb565 = ((r & 0xF8) << 8) |   /* 5 bits for red */
                      ((g & 0xFC) << 3) |   /* 6 bits for green */
                      ((b & 0xF8) >> 3);    /* 5 bits for blue */
    return rgb565;
}

/* JPEG 到 RGB565 转换函数 */
int jpeg_memory_to_rgb565(const unsigned char *jpeg_data, size_t jpeg_size, 
                         uint16_t **rgb565_output, int *width, int *height)
{
    struct jpeg_decompress_struct cinfo;
    struct my_error_mgr jerr;
    JSAMPARRAY buffer;
    int row_stride;
    uint16_t *rgb565_buffer = NULL;
    
    /* 设置错误处理 */
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    
    /* 建立错误恢复点 */
    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        if (rgb565_buffer != NULL) {
            free(rgb565_buffer);
        }
        return -1;
    }
    
    /* 初始化解压缩对象 */
    jpeg_create_decompress(&cinfo);
    
    /* 指定内存数据源 */
    jpeg_mem_src(&cinfo, jpeg_data, jpeg_size);
    
    /* 读取文件头 */
    (void) jpeg_read_header(&cinfo, TRUE);
    
    /* 设置输出颜色空间为 RGB */
    cinfo.out_color_space = JCS_RGB;
    
    /* 启动解压缩器 */
    (void) jpeg_start_decompress(&cinfo);
    
    /* 获取图像尺寸 */
    *width = cinfo.output_width;
    *height = cinfo.output_height;
    
    /* 计算行步长 */
    row_stride = cinfo.output_width * cinfo.output_components;
    
    /* 分配行缓冲区 */
    buffer = (*cinfo.mem->alloc_sarray)
        ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
    
    /* 分配 RGB565 输出缓冲区 */
    rgb565_buffer = (uint16_t *)malloc(
        cinfo.output_width * cinfo.output_height * sizeof(uint16_t));
    
    /* 逐行处理 */
    uint16_t *current_line = rgb565_buffer;
    while (cinfo.output_scanline < cinfo.output_height) {
        /* 读取一行像素 */
        (void) jpeg_read_scanlines(&cinfo, buffer, 1);
        
        /* 转换为 RGB565 */
        for (int i = 0; i < cinfo.output_width; i++) {
            uint8_t r = buffer[0][i * 3];     /* Red */
            uint8_t g = buffer[0][i * 3 + 1];  /* Green */
            uint8_t b = buffer[0][i * 3 + 2];  /* Blue */
            current_line[i] = rgb888_to_rgb565(r, g, b);
        }
        current_line += cinfo.output_width;
    }
    
    /* 完成解压缩 */
    (void) jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    
    /* 设置输出指针 */
    *rgb565_output = rgb565_buffer;
    return 0;
}
```

## 颜色格式说明

### RGB888 (24-bit)
- 每个像素 3 字节（红、绿、蓝各 1 字节）
- 红色：8 位（0-255）
- 绿色：8 位（0-255）
- 蓝色：8 位（0-255）

### RGB565 (16-bit)
- 每个像素 2 字节（16 位）
- 红色：5 位（高 5 位）
- 绿色：6 位（中间 6 位）
- 蓝色：5 位（低 5 位）
- 转换公式：`RGB565 = (R & 0xF8) << 8 | (G & 0xFC) << 3 | (B & 0xF8) >> 3`

## 配置说明

### 必需配置

- **`CONFIG_SDK_MODULE_IJG=y`**: 启用 IJG JPEG 库模块

### 可选配置

- **`CONFIG_MEM_CONFIG=y`**: 启用内存配置（如果需要）

## 注意事项

1. **错误处理**: IJG JPEG 库使用 `setjmp`/`longjmp` 机制进行错误处理，必须设置自定义错误处理函数，否则错误会导致程序崩溃
2. **内存管理**: 使用 `jpeg_mem_src()` 时，JPEG 数据必须完整存在于内存中，不能是流式数据
3. **资源清理**: 解压缩完成后必须调用 `jpeg_finish_decompress()` 和 `jpeg_destroy_decompress()` 释放资源
4. **输出缓冲区**: RGB565 输出缓冲区由调用者分配，使用完毕后必须由调用者释放（使用 `free()`）
5. **颜色空间**: 输出颜色空间设置为 `JCS_RGB`，输出为每像素 3 字节的 RGB888 格式，需要手动转换为 RGB565
6. **行缓冲区**: 使用 `alloc_sarray()` 分配的行缓冲区由 JPEG 库管理，无需手动释放
7. **图像尺寸**: 图像尺寸在 `jpeg_start_decompress()` 后通过 `cinfo.output_width` 和 `cinfo.output_height` 获取
8. **逐行处理**: 使用 `jpeg_read_scanlines()` 逐行读取数据，适合处理大图像，避免一次性加载整个图像到内存
9. **内存数据源**: 使用 IJG 库提供的 `jpeg_mem_src()` 函数从内存读取 JPEG 数据，该函数内部处理了内存源管理，无需手动实现

