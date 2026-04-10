# quirc QR 码识别示例

## 功能说明

演示如何使用 quirc 库进行 QR 码识别和解码，包括创建 quirc 对象、设置图像尺寸、获取图像缓冲区以及 QR 码解码等基本操作。

## 硬件连接

无需外部连接，quirc 为纯软件 QR 码识别库。

## 示例内容

1. 创建和初始化 quirc 对象
2. 设置图像尺寸并获取图像缓冲区
3. 将灰度图像数据写入缓冲区
4. 执行 QR 码识别和解码
5. 打印解码结果和错误处理信息

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
[I][sample] quirc initialized
[I][sample] Image buffer: 640x480
[I][sample] QR codes identified: 0
[I][sample] quirc sample completed
```

## 核心 API

| API | 说明 |
|-----|------|
| `quirc_new()` | 创建新的 quirc 对象 |
| `quirc_resize()` | 设置图像尺寸 |
| `quirc_begin()` | 获取图像缓冲区指针 |
| `quirc_end()` | 完成图像写入并执行识别 |
| `quirc_count()` | 获取识别到的 QR 码数量 |
| `quirc_extract()` | 提取指定 QR 码的数据 |
| `quirc_decode()` | 解码 QR 码数据 |
| `quirc_destroy()` | 销毁 quirc 对象 |

## 关键代码

```c
/* 创建 quirc 对象 */
struct quirc *qr = quirc_new();

/* 设置图像尺寸 */
quirc_resize(qr, width, height);

/* 获取图像缓冲区并写入数据 */
uint8_t *image = quirc_begin(qr, NULL, NULL);
memcpy(image, gray_data, width * height);
quirc_end(qr);

/* 提取和解码 QR 码 */
int count = quirc_count(qr);
for (int i = 0; i < count; i++) {
    struct quirc_code code;
    struct quirc_data data;
    quirc_extract(qr, i, &code);
    quirc_decode(&code, &data);
}

/* 销毁对象 */
quirc_destroy(qr);
```

### 使用预生成测试数据

在 `prj.conf` 或代码中启用：

```c
#define USE_QR_TEST_DATA
```

烧录到开发板后运行，程序将自动测试所有预生成的 QR 码。

## 测试的 QR 码规格

生成的测试数据包含以下类型的 QR 码：

| # | 名称 | QR 版本 | 尺寸 | 纠错级别 | 数据内容 | 数据类型 |
|---|------|---------|------|----------|----------|----------|
| 1 | small_low | 1 | 21x21 | L (Low) | "Hello World" | 字节 |
| 2 | small_medium | 1 | 21x21 | M (Medium) | "QUIRC" | 字节 |
| 3 | medium_high | 3 | 29x29 | H (High) | URL | 字节 |
| 4 | large_quartile | 5 | 37x37 | Q (Quartile) | "QR Code Test Data 12345" | 字节 |
| 5 | numeric_only | 1 | 21x21 | L (Low) | "1234567890" | 数字 |
| 6 | alphanumeric | 2 | 25x25 | M (Medium) | "ABC-123" | 字母数字 |
| 7 | chinese_text | 4 | 33x33 | M (Medium) | "聆思科技" | 字节(UTF-8) |

**测试覆盖：**
- QR 码版本：Version 1-5（尺寸从 21x21 到 37x37）
- 错误纠正级别：L/M/Q/H 四个级别
- 数据类型：数字、字母数字、字节、UTF-8 编码文本

## 项目结构

```
samples/media/quirc/
├── CMakeLists.txt          # CMake 配置
├── Kconfig                 # 配置选项
├── prj.conf                # 项目配置
├── sample.yaml             # 示例描述
├── README.md               # 本文件
├── src/
│   ├── main.c              # 主程序
│   └── qr_test_data.h      # QR 码测试数据（生成后）
└── tools/
    ├── qr_generator.py     # QR 码生成器
    ├── requirements.txt    # Python 依赖
    ├── generate_qr.sh      # 生成脚本
    └── output/             # 生成的文件输出目录
        ├── qr_*.png        # QR 码图片
        └── *.h             # 头文件
```

## QR 码生成器选项

`qr_generator.py` 支持以下参数：

- `-o, --output DIR` - 指定输出目录（默认：output）
- `-c, --combined` - 仅生成合并的头文件
- `-s, --separate` - 仅生成单独的头文件
- `-a, --all` - 生成所有格式（默认）

## 自定义 QR 码测试数据

编辑 `tools/qr_generator.py` 中的 `QR_CONFIGS` 数组来添加自定义的 QR 码配置：

```python
{
    "name": "custom_qr",
    "version": 2,  # QR 码版本 (1-40)
    "error_correction": qrcode.constants.ERROR_CORRECT_M,  # L/M/Q/H
    "data": "Your custom data",
    "box_size": 10,
    "border": 4,
}
```

## 注意事项

1. **图像格式**: quirc 仅接受灰度图像，彩色图像需先转换为灰度格式
2. **内存使用**: quirc 每个像素占用一个字节，加上解码器对象的几 KB 内存
3. **图像质量**: 确保输入图像清晰，QR 码占据图像的合理比例，避免过度模糊或倾斜
4. **测试数据**: 默认模式仅演示 API 使用，启用 `USE_QR_TEST_DATA` 可使用预生成的 QR 码进行实际解码测试
5. **错误处理**: `quirc_decode()` 返回错误码时，使用 `quirc_strerror()` 获取错误描述
