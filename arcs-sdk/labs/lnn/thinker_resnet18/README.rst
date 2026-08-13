Thinker ResNet18 示例
=====================

功能说明
--------

本示例演示在 ARCS 上直接链接 Thinker 推理库，加载外部 Flash 中的
``resnet18_arcs.bin``，对一帧 RGB565 测试图进行 CIFAR-100 分类。
示例固件运行在 AP Core，由 AP Core 初始化 Luna 并调用 Thinker 完成推理。

模型资源
--------

默认配置：

.. code-block:: text

   CONFIG_THINKER_RESNET18_MODEL_ADDR=0x30300000
   CONFIG_THINKER_RESNET18_MODEL_SIZE=0x1FD1E0

对应烧录偏移为 ``0x300000``。如模型大小或烧录地址不同，需要同步修改
``prj.conf`` 中的配置。

.. include:: /sample_build.rst

.. include:: /sample_flash.rst

模型烧录示例
------------

.. code-block:: shell

   cskburn -C arcs -s /dev/ttyACM0 -b 3000000 0x300000 ./resources/resnet18_arcs.bin

测试图片转换
------------

示例入口使用 ``src/test_image_apple.h`` 中的 RGB565 静态数组作为测试图片。
如需替换图片，可使用 ``scripts/image_to_rgb565_header.py`` 将 PNG、JPEG 等图片
转换为可直接编译进固件的 C 头文件：

.. code-block:: shell

   cd labs/lnn/thinker_resnet18
   ./scripts/image_to_rgb565_header.py resources/img_apple.png src/test_image_apple.h

脚本默认输出 ``64x64`` 的 RGB565 数据，数组名为 ``test_image``，并生成
``TEST_IMAGE_WIDTH`` 和 ``TEST_IMAGE_HEIGHT`` 宏。常用参数如下：

.. code-block:: shell

   ./scripts/image_to_rgb565_header.py input.png src/test_image_apple.h \
     --width 64 --height 64 \
     --symbol test_image \
     --background "#ffffff"

脚本依赖 Python Pillow 库；若本机缺少依赖，可执行：

.. code-block:: shell

   python3 -m pip install Pillow

测试图片预览
------------

.. image:: resources/img_apple.png
   :alt: Thinker ResNet18 示例测试图片预览
   :align: center
   :width: 360px

预期输出
--------

.. code-block:: text

   [I][resnet18] memory plan: entries=...
   [I][resnet18] best score: ..., index: ..., label: ...
   [I][thinker_resnet18] resnet18 result: label=... score=...

核心 API
--------

.. list-table::
   :header-rows: 1

   * - API
     - 说明
   * - ``tGetMemoryPlan()``
     - 获取模型运行所需内存段
   * - ``tModelInit()``
     - 初始化 Thinker 模型
   * - ``tCreateExecutor()``
     - 创建推理执行器
   * - ``tSetInput()``
     - 设置输入张量
   * - ``tForward()``
     - 执行推理
   * - ``tGetOutput()``
     - 获取输出张量

注意事项
--------

1. 默认模型地址 ``0x30300000`` 位于应用镜像之后，``CONFIG_MEM_FLASH_SIZE`` 已限制为 ``0x00300000``，避免应用和模型资源重叠。
2. ``CONFIG_THINKER_RESNET18_PSRAM_POOL_SIZE`` 控制 Thinker 运行期 PSRAM 内存池大小，内存计划不足时会在日志中报出需要的大小。
3. ``CONFIG_THINKER_RESNET18_SHARE_POOL_SIZE`` 控制 Thinker ``SHARE_MEM`` 内存池大小，默认链接到 ``APRAM``，即 Luna 共享内存窗口 ``0x20050000`` 起始区域。
4. 本示例固定使用 common ``arcs_dma_cpy`` 的 DMA copy 路径，并持久占用算法 DMA channel。
