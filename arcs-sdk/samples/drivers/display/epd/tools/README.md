# PNG to EPD 图像转换工具

这些Python脚本可以将PNG图像转换为UC8253C墨水屏支持的1bpp单色数据格式。

## 文件说明

### 1. `convert_to_epd.py` - 通用转换脚本

功能强大的通用图像转换工具，支持多种抖动算法。

**依赖包:**
```bash
pip install pillow numpy
```

**用法:**
```bash
# 基本用法 (使用Floyd-Steinberg抖动)
python convert_to_epd.py input.png

# 指定抖动算法
python convert_to_epd.py input.png --algorithm floyd
python convert_to_epd.py input.png --algorithm ordered
python convert_to_epd.py input.png --algorithm atkinson
python convert_to_epd.py input.png --algorithm sierra

# 自定义输出文件和参数
python convert_to_epd.py input.png -o output.h -t 140 --preview

# 自定义屏幕尺寸
python convert_to_epd.py input.png --width 240 --height 416
```

**支持的抖动算法:**
- **floyd**: Floyd-Steinberg抖动 (默认，推荐)
- **ordered**: 有序抖动 (Bayer矩阵)
- **atkinson**: Atkinson抖动
- **sierra**: Sierra抖动

### 2. `convert_pearl_girl.py` - 专用转换脚本

专门用于转换 `Meisje_met_de_parel_240x351.png` 的简化脚本。

**用法:**
```bash
cd tools/
python convert_pearl_girl.py
```

**输出文件:**
- `pearl_girl_image.h` - C头文件格式的图像数据
- `pearl_girl_preview.png` - 转换后的预览图像

## 转换流程

1. **图像预处理:**
   - 转换为灰度图像
   - 调整尺寸以适应墨水屏分辨率 (240x412)
   - 对于较小的图像会自动居中

2. **抖动处理:**
   - 应用选定的抖动算法
   - 将灰度图像转换为黑白二值图像
   - 保持视觉细节和层次感

3. **数据转换:**
   - 转换为1bpp格式 (8个像素打包成1个字节)
   - 生成适用于墨水屏的字节数组
   - 黑色像素 = 1，白色像素 = 0

4. **输出格式:**
   - C头文件格式，包含静态数组定义
   - 包含尺寸和大小的宏定义
   - 可选的预览图像

## 墨水屏规格

- **分辨率**: 240 x 412 像素
- **颜色**: 单色 (黑白)
- **数据格式**: 1bpp (每像素1位)
- **内存需求**: 12,360 字节 (240 × 412 ÷ 8)

## 抖动算法比较

| 算法 | 特点 | 适用场景 |
|------|------|---------|
| Floyd-Steinberg | 高质量，自然过渡 | 照片、复杂图像 (推荐) |
| Ordered (Bayer) | 规则图案，快速 | 简单图形、文本 |
| Atkinson | 柔和，少量噪点 | 人像、艺术图像 |
| Sierra | 高精度，慢速 | 高质量要求的图像 |

## 使用示例

### 转换珍珠女孩图像
```bash
cd tools/
python convert_pearl_girl.py
```

### 转换自定义图像
```bash
python convert_to_epd.py my_image.png --algorithm floyd --preview
```

### 批量转换
```bash
for file in *.png; do
    python convert_to_epd.py "$file" --algorithm floyd
done
```

## 在代码中使用

转换后的头文件可以直接包含到C项目中：

```c
#include "pearl_girl_image.h"

// 显示图像
display_write(0, 0, &buffer_desc, pearl_girl_image);
```

## 注意事项

1. **图像质量**: Floyd-Steinberg抖动通常提供最佳效果
2. **阈值调整**: 对于特别亮或暗的图像，可能需要调整阈值 (默认128)
3. **内存限制**: 确保目标设备有足够内存存储图像数据
4. **预览检查**: 使用 `--preview` 选项查看转换效果

## 故障排除

**错误: "No module named 'PIL'"**
```bash
pip install pillow
```

**图像过大/过小:**
- 脚本会自动调整尺寸
- 可以使用 `--width` 和 `--height` 参数自定义

**转换效果不佳:**
- 尝试不同的抖动算法
- 调整阈值参数 (`--threshold`)
- 预处理图像 (调整对比度、亮度)
