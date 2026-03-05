# quirc - QR 码识别示例

## 概述

这个示例展示了如何使用 quirc QR 码识别库的基本功能。

## 功能特性

- 创建和销毁 quirc 对象
- 设置图像尺寸
- 获取图像缓冲区
- QR 码识别和解码
- 错误处理
- 多种规格 QR 码测试

## quirc 库简介

quirc 是一个用于从图像中提取和解码 QR 码的库，具有以下特点：

- **快速**: 在现代 x86 核心上处理 VGA 帧大约需要 50ms
- **鲁棒性强**: 能正确识别旋转和倾斜的 QR 码
- **易于使用**: 简单的 API 接口
- **小巧**: 无依赖，易于嵌入
- **内存占用小**: 每个像素一个字节，加上解码器对象的几 KB

## 使用说明

### 基本测试（无真实 QR 码数据）

1. 构建项目：
```bash
bash build.sh
```

2. 烧录到开发板后运行

此模式仅演示 quirc 库的基本 API 使用方法。

### 使用真实 QR 码数据测试

#### 1. 生成 QR 码测试数据

首先需要安装 Python 依赖：

```bash
pip3 install -r tools/requirements.txt
```

然后运行生成脚本：

```bash
bash tools/generate_qr.sh
```

或者手动运行 Python 脚本：

```bash
# 生成所有格式（合并和单独的头文件）
python3 tools/qr_generator.py -a

# 仅生成合并的头文件
python3 tools/qr_generator.py -c

# 仅生成单独的头文件
python3 tools/qr_generator.py -s

# 指定输出目录
python3 tools/qr_generator.py -o gen_qrcode -a
```

生成的文件：
- `tools/output/qr_*.png` - QR 码图片
- `tools/output/qr_*.h` - 单个 QR 码的头文件
- `tools/output/qr_test_data.h` - 合并的测试数据头文件
- `src/qr_test_data.h` - 自动复制到源码目录

#### 2. 启用测试数据

编辑 `src/main.c`，取消注释以下行：

```c
// #define USE_QR_TEST_DATA
```

改为：

```c
#define USE_QR_TEST_DATA
```

#### 3. 构建和运行

```bash
bash build.sh
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
samples/modules/quirc/
├── CMakeLists.txt          # CMake 配置
├── Kconfig                 # 配置选项
├── prj.conf                # 项目配置
├── build.sh                # 构建脚本
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

- 默认模式下，示例仅演示 quirc 库的基本 API 使用方法，不包含真实的 QR 码图像数据
- 要测试真实的 QR 码解码，需要生成测试数据并启用 `USE_QR_TEST_DATA`
- 在实际应用中，QR 码数据通常来自摄像头或图像文件
- 确保图像数据已转换为灰度格式再送入 quirc 解码

## 参考文档

更多信息请参考：
- quirc 库的 README.md 文件（modules/quirc/README.md）
- quirc API 文档（modules/quirc/lib/quirc.h）

