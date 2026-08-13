.. _getting_started:

================
快速入门
================

本文档介绍如何快速开始使用 ARCS SDK 进行开发，包括环境搭建、编译示例和烧录运行。

.. note::
   SDK 支持 Linux 和 Windows PowerShell 开发环境；Linux 推荐使用 Ubuntu 18.04 以上版本。

.. _environment_setup:

环境搭建
========

Linux 系统依赖
----------------

.. code-block:: shell

   sudo apt update
   sudo apt install -y wget bzip2 python3 git vim-common

.. note::
   ``vim-common`` 提供 ``tools/mkhdr/mkhdr.sh`` 写 image header 所需的 ``xxd`` 命令。

Windows 前置条件
----------------

Windows 环境请使用 PowerShell，建议使用 Windows 10/11 自带的 Windows PowerShell 5.1 或更新版本。

Windows 系统依赖：

- ``Git for Windows``：用于获取 SDK 仓库；构建时 CMake 也会用 ``git`` 生成版本信息。
- ``Python 3``：用于运行 ``tools/mkhdr/mkhdr.py`` 等 Python 工具，需确保命令行中可直接执行 ``python``。
- 网络访问：首次运行 ``env.ps1`` 时会下载 Windows 工具包和 GCC 工具链。
- ``Invoke-WebRequest`` / ``Expand-Archive``：PowerShell 内置下载与解压能力；下载失败时 ``env.ps1`` 会回退使用系统 ``curl.exe``。
- ``cmd.exe`` / ``subst``：当 SDK 路径包含中文、空格或 OneDrive 同步目录时，``env.ps1`` 会使用 ``subst`` 做兼容路径映射。

可通过以下命令快速检查 Git 和 Python：

.. code-block:: powershell

   git --version
   python --version

``env.ps1`` 会自动下载以下工具包，也可以手动下载后解压：

- ``listenai-tools-windows-v0.0.1.zip``：包含 Windows 版本的 CMake、Ninja、Kconfig、menuconfig 等工具
- ``nuclei_riscv_newlibc_prebuilt_win64_2025.10.zip``：Windows 版本 RISC-V GCC 工具链

Windows 版本工具下载地址：

- `Windows 开发工具包下载地址 <https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/ARCS/tools/dev-tools/windows-amd64/v0.0.1/listenai-tools-windows-v0.0.1.zip>`_
- `Windows 工具链下载地址 <https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/ARCS/tools/toolchain/windows-amd64/nuclei_riscv_newlibc_prebuilt_win64_2025.10.zip>`_

如遇 PowerShell 执行策略限制，可在当前终端临时放开：

.. code-block:: powershell

   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

Linux 自动搭建（推荐）
-----------------------

在 SDK 根目录下执行：

.. code-block:: shell

   source env.sh

该命令会自动完成以下操作：

- 检测工具链环境变量，已有有效配置则直接使用，不覆盖
- 工具链缺失时自动查找或下载安装
- 设置环境变量（``NUCLEI_TOOLCHAIN_PATH``、``LISTENAI_TOOLS_PATH``、``PATH``）
- 检测子模块状态，有异常时提示修复命令（`labs/zig/adapter` 已随主仓分发，不需要单独初始化）

已就绪的环境 source 后秒完成，可重复执行。

.. tip::
   可使用子命令进行专项排查：

   .. code-block:: shell

      source env.sh check              # 仅检测环境状态
      source env.sh setup              # 仅安装工具链
      source env.sh submodule sync     # 仅同步子模块
      source env.sh info               # 查看版本信息

Linux 手动搭建
--------------

如果自动搭建失败，可以手动搭建开发环境：

1. **下载工具链**

   下载对应平台的工具链并解压（如果已存在工具链，可跳过此步骤）：

   - `Linux 工具链下载地址 <http://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/ARCS/tools/toolchain/linux-amd64/nuclei_riscv_newlibc_prebuilt_linux64_2025.02.tar.bz2>`_

   也可使用 SDK 内置脚本：

   .. code-block:: shell

      bash tools/scripts/prepare_toolchain.sh

2. **下载 ListenAI 开发工具包**

   - `Linux 开发工具包下载地址 <http://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/ARCS/tools/dev-tools/linux-amd64/v0.0.1/listenai-tools.tar.gz>`_

   也可使用 SDK 内置脚本：

   .. code-block:: shell

      bash tools/scripts/prepare_listenai_tools.sh

3. **设置环境变量**

   .. code-block:: shell

      # 设置工具链路径
      export NUCLEI_TOOLCHAIN_PATH=$HOME/.listenai/gcc

      # 设置 ListenAI 工具包路径
      export LISTENAI_TOOLS_PATH=$HOME/.listenai/listenai-tools

   .. warning::
      **必须使用绝对路径！** 环境变量的路径必须是绝对路径（如 ``/home/user/.listenai/gcc``），不能使用相对路径（如 ``./toolchain`` 或 ``../toolchain``），否则会导致编译失败。
      ``$HOME`` 会由 shell 自动展开为用户主目录的绝对路径；请勿在 IDE 配置等非 shell 环境中使用 ``~``，它不会被自动展开。

   其中：

   - ``NUCLEI_TOOLCHAIN_PATH`` 指向 ``$HOME/.listenai/gcc``\ （工具链安装目录）
   - ``LISTENAI_TOOLS_PATH`` 指向 ``$HOME/.listenai/listenai-tools``\ （开发工具包安装目录）

Windows 环境搭建（推荐）
-------------------------

Windows 版本工具默认安装到 ``%USERPROFILE%\.listenai``，目录结构需要保持如下形式：

.. code-block:: text

   %USERPROFILE%\.listenai\
   ├── gcc\
   │   └── bin\riscv64-unknown-elf-gcc.exe
   └── listenai-tools\
       ├── cmake\bin\cmake.exe
       ├── ninja\ninja.exe
       ├── kconfig\
       └── menuconfig\

1. **自动搭建**

   在 SDK 根目录执行：

   .. code-block:: powershell

      .\env.ps1

   该命令会检测 ``%USERPROFILE%\.listenai\gcc`` 和 ``%USERPROFILE%\.listenai\listenai-tools``，缺失时自动下载并解压，然后设置当前 PowerShell 会话的环境变量。

   如需重新下载工具包，可执行：

   .. code-block:: powershell

      .\env.ps1 setup

2. **手动解压（可选）**

   如需离线安装，可手动下载上述两个压缩包后解压：

   .. code-block:: powershell

      New-Item -ItemType Directory -Force "$env:USERPROFILE\.listenai" | Out-Null
      Expand-Archive .\nuclei_riscv_newlibc_prebuilt_win64_2025.10.zip "$env:USERPROFILE\.listenai" -Force
      Expand-Archive .\listenai-tools-windows-v0.0.1.zip "$env:USERPROFILE\.listenai" -Force
      .\env.ps1

   上述两个压缩包内已经带有 ``gcc`` 或 ``listenai-tools`` 顶层目录；如果使用其他来源的压缩包，请以最终目录中能直接找到上面列出的 ``*.exe`` 为准。

   ``env.ps1`` 会设置 ``ARCS_BASE``、``NUCLEI_TOOLCHAIN_PATH``、``LISTENAI_TOOLS_PATH``，并把工具链、CMake、Ninja、Kconfig、menuconfig 加入当前终端的 ``PATH``。

   .. note::
      Windows 下建议将 SDK 放在纯英文且不含空格的目录中。若 SDK 路径包含中文、空格或 OneDrive 同步目录，``env.ps1`` 会自动启用兼容路径，避免部分构建工具对非 ASCII 路径兼容不佳。

.. _quick_start:

快速开始
========

编译示例
--------

以 helloworld 工程为例，演示如何编译项目：

1. **编译命令**

   在 SDK 根目录下执行：

   Linux：

   .. code-block:: shell

      ./build.sh -C -S samples/helloworld -DBOARD=arcs_evb

   Windows PowerShell：

   .. code-block:: powershell

      .\build.ps1 -C -S samples/helloworld -DBOARD=arcs_evb

   命令参数说明（Linux / Windows 一致）：

   - ``-S``: 指定项目源码路径
   - ``-DBOARD``: 指定目标板型（必需参数，如 ``arcs_mini``、``arcs_evb`` 等）
   - ``-C``: 清理构建目录（可选）
   - ``-B``: 指定构建目录（可选，默认 ``build``）

2. **编译输出**

   编译成功后会在构建目录下生成构建产物，Linux 和 Windows 默认目录均为 ``build``，包括：
   
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
   
   - 开发板 TX 脚 (默认引脚PA2，注意查看板型文件) 连接串口板 RX
   - 开发板 RX 脚 (默认引脚PA3，注意查看板型文件) 连接串口板 TX
   - 开发板 GND 连接串口板 GND

2. **进入烧录模式**

   按住 BOOT 脚后复位开发板，进入烧录模式。

   .. note::
      每次重新烧录前，都需要执行按住 BOOT 脚后复位开发板的操作。

自动烧录（推荐）
----------------

如果希望实现自动烧录，可以连接控制引脚：

- 开发板 BOOT 脚连接串口板 RTS 脚
- 开发板 RESET 脚连接串口板 DTR 脚

这样 cskburn 工具可以自动控制进入烧录模式。

烧录命令
--------

使用 cskburn 工具进行烧录：

Linux：

.. code-block:: shell

   ./tools/burn/cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/helloworld.bin -C arcs

Windows PowerShell：

.. code-block:: powershell

   .\tools\burn\cskburn.exe -C arcs -s COM7 -b 3000000 0x0 .\build\helloworld.bin

命令参数说明：

- ``-s``: 指定烧录设备（串口设备路径）

  .. note::
     请根据实际情况选择正确的串口设备：

     - Linux 使用 ``ls /dev/ttyUSB*`` 或 ``ls /dev/ttyACM*`` 查看可用设备
     - Windows 在“设备管理器”中查看端口号，常见形式为 ``COM7``、``COM8`` 等
     - Linux 常见设备名：``/dev/ttyUSB0``、``/dev/ttyUSB1``、``/dev/ttyACM0`` 等
     - 插入串口板时可使用 ``dmesg | tail`` 查看系统分配的设备名

- ``-b``: 指定烧录波特率（推荐使用 3000000）
- ``0x0``: 烧录起始地址（基于 0x30000000 flash 起始地址的偏移）
- ``build/helloworld.bin`` 或 ``.\build\helloworld.bin``: 烧录文件路径
- ``-C arcs``: 指定芯片类型

验证运行
--------

烧录完成后复位开发板，应该可以在串口控制台看到以下输出：

.. code-block:: text

   Running on hart-id: 1
   Hello, world!

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

      source env.sh check

   Windows PowerShell 可执行：

   .. code-block:: powershell

      .\env.ps1
      riscv64-unknown-elf-gcc --version
      cmake --version
      ninja --version
