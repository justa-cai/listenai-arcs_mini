.. _getting_started:

================
快速入门
================

本文档介绍如何快速开始使用 ARCS SDK 进行开发，包括环境搭建、编译示例和烧录运行。

.. note::
   目前仅支持 Linux 平台，推荐使用 Ubuntu 18.04 以上版本。

.. _environment_setup:

环境搭建
=======

自动搭建（推荐）
--------------

1. **下载开发工具包**

   在 SDK 根目录下运行脚本：

   .. code-block:: shell

      ./prepare_listenai_tools.sh

2. **下载工具链**

   运行脚本下载工具链：

   .. code-block:: shell

      ./prepare_toolchain.sh

手动搭建
--------

如果自动搭建失败，可以手动搭建开发环境：

1. **下载工具链**

   下载对应平台的工具链并解压（如果已存在工具链，可跳过此步骤）：

   - `Linux 工具链下载地址 <https://iflyos-external.oss-cn-shanghai.aliyuncs.com/chip_arcs/listenai-linux-amd64-tools.tar.gz>`_

2. **下载 ListenAI 开发工具包**

   - `Linux 开发工具包下载地址 <http://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/ARCS/tools/dev-tools/linux-amd64/v0.0.1/listenai-tools.tar.gz>`_

3. **设置环境变量**

   .. code-block:: shell

      # 设置工具链路径
      export NUCLEI_TOOLCHAIN_PATH=/path/to/toolchain
      
      # 设置 ListenAI 工具包路径
      export LISTENAI_TOOLS_PATH=/path/to/listenai-tools

   其中：
   
   - ``NUCLEI_TOOLCHAIN_PATH`` 指向解压后的工具链路径
   - ``LISTENAI_TOOLS_PATH`` 指向解压后的 ListenAI 工具包路径

.. _quick_start:

快速开始
========

编译示例
--------

以 helloworld 工程为例，演示如何编译项目：

1. **编译命令**

   在 SDK 根目录下执行：

   .. code-block:: shell

      ./build.sh -S samples/helloworld -C

   命令参数说明：
   
   - ``-S``: 指定项目源码路径
   - ``-C``: 清理构建目录

2. **编译输出**

   编译成功后会在 ``build`` 目录下生成构建产物，包括：
   
   - ``helloworld.bin``: 烧录文件
   - ``helloworld.elf``: 调试文件
   - 其他相关文件

.. _flashing:

烧录运行
========

准备工作
--------

1. **连接硬件**

   将串口板连接到开发板：
   
   - 开发板 TX 脚 (PA2) 连接串口板 RX
   - 开发板 RX 脚 (PA3) 连接串口板 TX
   - 开发板 GND 连接串口板 GND

2. **进入烧录模式**

   按住 BOOT 脚后复位开发板，进入烧录模式。

   .. note::
      每次重新烧录前，都需要执行按住 BOOT 脚后复位开发板的操作。

自动烧录（推荐）
--------------

如果希望实现自动烧录，可以连接控制引脚：

- 开发板 BOOT 脚连接串口板 RTS 脚
- 开发板 RESET 脚连接串口板 DTR 脚

这样 cskburn 工具可以自动控制进入烧录模式。

烧录命令
--------

使用 cskburn 工具进行烧录：

.. code-block:: shell

   ./tools/burn/cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/helloworld.bin

命令参数说明：

- ``-s``: 指定烧录设备（串口设备）
- ``-b``: 指定烧录波特率
- ``0x0``: 烧录起始地址（基于 0x30000000 flash 起始地址的偏移）
- ``build/helloworld.bin``: 烧录文件路径

更多烧录工具使用方法，请参考 `cskburn 文档 <../tools/burn/README.MD>`_。

验证运行
--------

烧录完成后复位开发板，应该可以在串口控制台看到以下输出：

.. code-block:: text

   ********arcs boot on hart id:1********
   boot hart:1
   Hello, world!

.. _project_configuration:

项目配置
========

1. **文本配置**

   可在项目顶级目录的 ``prj.conf`` 文件中进行配置：

   .. code-block:: text

      # 项目可根据需要在此文件中设置对应的配置
      CONFIG_HEAP_SIZE=32768
      CONFIG_PSRAM_HEAP_SIZE=1048576

2. **图形化配置**

   通过 menuconfig 进行图形化配置：

   .. code-block:: shell

      ./build.sh -t menuconfig

   .. note::
      当项目目录同时存在 ``.config`` 文件和 ``prj.conf`` 文件时，``.config`` 文件会覆盖 ``prj.conf`` 文件中的配置。运行 menuconfig 时，可选择将 ``.config`` 文件保存到工程目录下。

.. _troubleshooting:

常见问题
========

1. **权限问题**

   如果遇到串口权限问题，将当前用户添加到 dialout 组：

   .. code-block:: shell

      sudo usermod -a -G dialout $USER

   然后重新登录。

2. **串口设备问题**

   使用 ``dmesg`` 或 ``ls /dev/ttyUSB*`` 查看串口设备：

   .. code-block:: shell

      ls /dev/ttyUSB*

3. **环境变量问题**

   确保已正确设置环境变量，可以使用以下命令检查：

   .. code-block:: shell

      echo $NUCLEI_TOOLCHAIN_PATH
      echo $LISTENAI_TOOLS_PATH
