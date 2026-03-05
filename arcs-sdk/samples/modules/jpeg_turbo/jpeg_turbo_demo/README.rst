libjpeg-turbo 编解码示例
===========================

本示例演示如何使用 libjpeg-turbo 库在嵌入式系统中进行 JPEG 图像的编码和解码。

.. tip:: **推荐使用原生 libjpeg API**

   虽然 TurboJPEG API 设计更简洁,但在嵌入式平台上 **原生 libjpeg API 更稳定可靠**。
   **生产环境建议使用原生 libjpeg API** （参考 ``decode_to_rgb565`` 和 ``decode_to_rgb888_native`` 示例）。

功能特性
--------

- **JPEG 解码**: 支持 RGB888, RGB565, YUV420 输出
- **JPEG 编码**: 支持 RGB888 输入
- **API 支持**: 包含 TurboJPEG API 和 原生 libjpeg API 示例

注意事项
--------

为了最佳兼容性和性能，建议 JPEG 图片满足以下条件：

1. **Baseline JPEG** （非 Progressive）
2. **移除元数据** （EXIF、XMP 等）
3. **色度采样**: 4:2:0 或 4:4:4

使用指南
--------

本示例包含辅助脚本，用于优化 JPEG 图片并将其转换为 C 头文件。

1. **准备图片**: 使用 ``strip_metadata.py`` 清除元数据

   .. code-block:: bash

      python3 tools/strip_metadata.py input.jpg output_clean.jpg

2. **生成头文件**: 使用 ``bin2header.py`` 转换为 C 数组

   .. code-block:: bash

      python3 tools/bin2header.py output_clean.jpg src/sample_jpeg.h sample_jpeg

3. **编译运行**:

   .. code-block:: bash

      ./build.sh
      cskburn -C arcs -s /dev/ttyACM0 -b 3000000 0x0 ./build/arcs.bin

关键函数说明
------------

- ``decode_to_rgb565()``: 使用原生 API 解码为 RGB565。**生产环境推荐**，内存占用低，稳定性高。
- ``decode_to_rgb888_native()``: 使用原生 API 解码为 RGB888。**生产环境推荐**。
- ``decode_to_rgb888()``: 使用 TurboJPEG API 解码为 RGB888。仅供参考。
- ``decode_to_yuv420()``: 使用 TurboJPEG API 解码为 YUV420。仅供参考。
- ``encode_rgb_to_jpeg()``: 使用 TurboJPEG API 将 RGB888 编码为 JPEG。

参考资料
--------

- `libjpeg-turbo 官方文档 <https://libjpeg-turbo.org/>`_

工具脚本
--------

.. toctree::
   :maxdepth: 1

   tools/README
