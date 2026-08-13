LNN 模块使用指南
================

LNN 实验室内容仍在演进中。当前仓库提供的主要能力是：在 ARCS AP Core
固件中集成 Thinker 推理运行时，通过 Luna 执行模型中的计算任务，并以
``thinker_resnet18`` 和 ``thinker_resnet18_real`` 示例验证静态图片推理与
双核摄像头实时识别链路。

关键目录
--------

当前 LNN 支持由以下目录组成：

- ``labs/lnn/common/arcs/libraries/lunna``：Luna 静态库与头文件
- ``labs/lnn/common/arcs/libraries/thinker``：Thinker 静态库、头文件和 Kconfig 开关
- ``labs/lnn/common/arcs/dma_cpy``：算法数据搬运使用的 DMA copy 封装
- ``labs/lnn/thinker_resnet18``：Thinker ResNet18 示例工程
- ``labs/lnn/thinker_resnet18_real``：CP 摄像头预览、AP Thinker 推理的双核实时识别示例工程

``common/arcs`` 目录不是独立应用，而是供具体 LNN 示例通过
``add_subdirectory()`` 接入的公共库集合。

构建集成方式
------------

典型 LNN 示例需要先把公共库加入当前工程，再链接 Thinker 与 DMA copy：

.. code-block:: cmake

   add_subdirectory(
       ${CMAKE_CURRENT_LIST_DIR}/../common/arcs
       ${CMAKE_CURRENT_BINARY_DIR}/lnn_common_arcs
   )

   target_link_libraries(${PROJECT_NAME} PRIVATE
       thinker
       arcs_dma_cpy
   )

其中 ``thinker`` 会继续链接 Luna 相关静态库；``arcs_dma_cpy`` 负责为算法
数据搬运提供 DMA 或 memcpy 后端。

运行核心与关键配置
------------------

``thinker_resnet18`` 示例固件运行在 AP Core，并由 AP Core 初始化 Luna、
Thinker 和 DMA copy 路径。关键配置包括：

.. code-block:: text

   CONFIG_ARCS_AP_CORE=y
   CONFIG_THINKER_LIB=y
   CONFIG_THINKER_RESNET18_MODEL_ADDR=0x30300000
   CONFIG_THINKER_RESNET18_MODEL_SIZE=0x1FD1E0
   CONFIG_THINKER_RESNET18_PSRAM_POOL_SIZE=0x10000
   CONFIG_THINKER_RESNET18_SHARE_POOL_SIZE=0x58000

``CONFIG_THINKER_LIB`` 使能 Thinker 公共库；模型地址和大小用于从外部
Flash 读取 ``resnet18_arcs.bin``。PSRAM pool 用于 Thinker 运行期普通
内存计划项，SHARE pool 位于 APRAM，作为 Luna 可见的共享内存窗口。

模型资源烧录
------------

``thinker_resnet18`` 示例把应用固件和模型资源分开烧录。默认配置下，应用
固件占用 ``0x30000000`` 起始的 Flash 前 3MB，模型资源放在
``0x30300000``，对应烧录偏移 ``0x300000``。

示例模型烧录命令：

.. code-block:: shell

   cd labs/lnn/thinker_resnet18
   cskburn -C arcs -s /dev/ttyACM0 -b 3000000 0x300000 ./resources/resnet18_arcs.bin

如果替换模型文件，需要同步更新 ``prj.conf`` 中的
``CONFIG_THINKER_RESNET18_MODEL_SIZE``。如果调整模型烧录地址，也需要同步更新
``CONFIG_THINKER_RESNET18_MODEL_ADDR`` 和应用 Flash 空间规划。

推理运行流程
------------

``thinker_resnet18`` 的运行流程如下：

1. AP Core 使能 Luna 时钟，初始化 Luna 和 Thinker。
2. 调用 ``tGetMemoryPlan()`` 获取模型需要的内存计划。
3. 应用侧为 PSRAM 和 APRAM 内存计划项填入实际地址。
4. 调用 ``tModelInit()`` 与 ``tCreateExecutor()`` 初始化模型和执行器。
5. 将 RGB565 测试图缩放到模型输入尺寸，并转换为量化后的 int8 输入。
6. 调用 ``tSetInput()``、``tForward()``、``tGetOutput()`` 完成推理。
7. 在输出的 ``[1, 100]`` int8 分数中选择最大值，映射到 CIFAR-100 标签。

``thinker_resnet18_real`` 在此基础上拆分为 AP/CP 两个固件：AP Core 负责
Thinker 和 Luna 推理，CP Core 负责摄像头采集、LVGL 界面和 LCD 显示。
CP 将当前摄像头帧预处理为 ``int8 [1, 3, 32, 32]`` 输入 tensor，通过 HAL IPC
共享 RAM 中的 ``.ipc.lnn`` 控制块传给 AP，并使用 HAL IPC endpoint 做请求和
结果通知。

测试图片转换
------------

示例默认使用 ``src/test_image_apple.h`` 中的 RGB565 静态数组。替换测试图片时，
可使用内置脚本将 PNG、JPEG 等图片转换为可直接编译进固件的 C 头文件：

.. code-block:: shell

   cd labs/lnn/thinker_resnet18
   ./scripts/image_to_rgb565_header.py resources/img_apple.png src/test_image_apple.h

脚本默认输出 ``64x64`` 图片、数组名 ``test_image``，并生成
``TEST_IMAGE_WIDTH`` 和 ``TEST_IMAGE_HEIGHT`` 宏。可通过参数调整尺寸、符号名
和透明背景合成颜色：

.. code-block:: shell

   ./scripts/image_to_rgb565_header.py input.png src/test_image_apple.h \
     --width 64 --height 64 \
     --symbol test_image \
     --background "#ffffff"

脚本依赖 Python Pillow 库。缺少依赖时可执行：

.. code-block:: shell

   python3 -m pip install Pillow

注意事项
--------

- 当前 LNN 示例面向 ARCS AP Core，不是 Host 侧推理程序。
- Thinker 模型资源必须与运行库、目标平台和输入预处理保持一致。
- ``thinker_resnet18`` 当前模型输入为 int8 ``[1, 3, 32, 32]``，输出为 int8 ``[1, 100]``。
- APRAM 用作 Luna 共享内存，``CONFIG_THINKER_RESNET18_SHARE_POOL_SIZE`` 不足时初始化会失败。
- 示例固定使用算法 DMA channel，和其他占用同一 DMA channel 的业务组合时需要重新评估资源分配。
