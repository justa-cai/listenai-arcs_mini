工具脚本说明
============

本目录包含用于处理 JPEG 图像和生成嵌入式资源的辅助脚本。

脚本列表
--------

1. **strip_metadata.py**: JPEG 元数据清除工具
2. **bin2header.py**: 二进制转 C 头文件工具

环境要求
--------

- Python 3.x
- Pillow 库 (仅 ``strip_metadata.py`` 需要)

安装依赖:

.. code-block:: bash

   pip install Pillow

strip_metadata.py
-----------------

该脚本用于清除 JPEG 文件中的所有元数据（EXIF、XMP、ICC Profile 等），并将其转换为标准的 Baseline JPEG 格式。这对于减小文件体积和提高嵌入式系统的兼容性非常有用。

用法
~~~~

.. code-block:: bash

   python3 strip_metadata.py <input_file> <output_file> [quality]

参数说明
~~~~~~~~

- ``input_file``: 输入 JPEG 图片路径
- ``output_file``: 输出处理后的图片路径
- ``quality``: (可选) JPEG 压缩质量，范围 1-100，默认为 90

示例
~~~~

.. code-block:: bash

   # 清除元数据，使用默认质量 (90)
   python3 strip_metadata.py input.jpg output_clean.jpg

   # 清除元数据，指定质量为 85
   python3 strip_metadata.py input.jpg output_clean.jpg 85

bin2header.py
-------------

该脚本将任意二进制文件（如 JPEG 图片）转换为 C 语言头文件，其中包含该文件的字节数组。这方便将图片资源直接嵌入到固件代码中。

用法
~~~~

.. code-block:: bash

   python3 bin2header.py <input_file> <output_file> [array_name]

参数说明
~~~~~~~~

- ``input_file``: 输入二进制文件路径
- ``output_file``: 输出 .h 头文件路径
- ``array_name``: (可选) 生成的 C 数组变量名。如果不指定，将根据输入文件名自动生成。

示例
~~~~

.. code-block:: bash

   # 将图片转换为头文件，数组名自动生成 (例如 sample_jpeg)
   python3 bin2header.py sample.jpg sample_jpeg.h

   # 指定数组名为 my_image_data
   python3 bin2header.py sample.jpg image_data.h my_image_data

生成的代码示例
~~~~~~~~~~~~~~

生成的头文件内容如下所示：

.. code-block:: c

   /**
    * @file image_data.h
    * @brief Binary data from sample.jpg
    * @note Auto-generated file, do not edit manually
    */

   #ifndef MY_IMAGE_DATA_H
   #define MY_IMAGE_DATA_H

   #include <stdint.h>

   static const unsigned char my_image_data[] = {
       0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
       // ... 数据内容 ...
   };
   static const unsigned int my_image_data_len = 12345;

   #endif // MY_IMAGE_DATA_H
