.. _custom_board_guide:

自定义板型开发指南
==================

本文档提供创建自定义板型的完整步骤，包括文件模板和接口规范。

文件结构
--------

.. code-block:: text

   my_board/
   ├── CMakeLists.txt    # 板型构建脚本（必需）
   ├── board.h           # 板级接口声明（必需）
   ├── board.c           # 板级初始化实现（必需）
   ├── pinmux.h          # 引脚配置声明（必需）
   ├── pinmux.c          # 引脚配置实现（必需）
   └── Kconfig           # 板型配置选项（必需）

开发步骤
--------

Step 1: 创建板型目录
^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

   mkdir -p /path/to/my_boards/my_board
   cd /path/to/my_boards/my_board

Step 2: 创建 CMakeLists.txt
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cmake

   # ARCS BOARD SUPPORT
   #
   # Copyright (c) 2025, LISTENAI
   # SPDX-License-Identifier: Apache-2.0

   # 创建板型库
   listenai_library_named(module_boards)

   # 添加源文件
   listenai_library_sources(
       pinmux.c
       board.c
   )

   # 添加头文件路径
   listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

Step 3: 创建 board.h 和 board.c
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

可以复制 SDK 内置板型作为起点：

.. code-block:: shell

   cp $ARCS_SDK_BASE/boards/arcs_evb/board.h .
   cp $ARCS_SDK_BASE/boards/arcs_evb/board.c .

修改 ``board_get_name()`` 返回值为您的板型名称。

**board.h 模板：**

.. code-block:: c

   // ARCS BOARD SUPPORT
   //
   // Copyright (c) 2025, LISTENAI
   // SPDX-License-Identifier: Apache-2.0

   #ifndef _BOARD_MY_BOARD_H_
   #define _BOARD_MY_BOARD_H_

   const char* board_get_name(void);

   #endif // _BOARD_MY_BOARD_H_

**board.c 模板：**

.. code-block:: c

   // ARCS BOARD SUPPORT
   //
   // Copyright (c) 2025, LISTENAI
   // SPDX-License-Identifier: Apache-2.0

   #include "board.h"

   const char* board_get_name(void)
   {
       return "my_board";
   }

Step 4: 使用 Pinmux 工具生成引脚配置
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. important::
   pinmux.h 和 pinmux.c **必须使用工具生成**，不应手动编辑。

工具地址：https://tool.listenai.com/ls-pinmux-tool/

使用步骤：

1. 访问 Pinmux 配置工具
2. 选择目标芯片型号
3. 配置项目中使用的外设（如 UART0、SPI0、I2C0 等）
4. 为每个外设配置引脚映射
5. 生成代码
6. 将生成的 ``pinmux.h`` 和 ``pinmux.c`` 保存到板型目录

生成的 pinmux 函数会被设备驱动自动调用（如 ``lisa_uart0_pinmux()`` 由 lisa_uart 驱动在 uart0 初始化时调用）。

Step 5: 创建 Kconfig 文件
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: kconfig

   # Board Configuration
   #
   # Copyright (c) 2025, LISTENAI
   # SPDX-License-Identifier: Apache-2.0

   config BOARD_NAME
       string
       default "my_board"
       prompt "Board Name"
       help
           Board Name

   config BOARD_MY_BOARD
       bool
       default y
       help
           Board is my_board

.. note::
   - 将 ``my_board`` 替换为实际板型名称
   - 第二个配置项名称格式为 ``BOARD_<板型名称大写>``
   - 两个配置项都是必需的

Step 6: 编译验证
^^^^^^^^^^^^^^^^^

.. code-block:: shell

   cd $ARCS_SDK_BASE
   ./build.sh -S samples/helloworld \
       -DBOARD=my_board \
       -DBOARD_SEARCH_PATH=/path/to/my_boards

.. important::
   外部板型文件已通过编译变量指定构建路径，无需在工程中额外添加构建。

确认构建日志中出现：

.. code-block:: text

   -- Target board: my_board
   -- Board found (custom): my_board from /path/to/my_boards
   -- Board loaded successfully: /path/to/my_boards/my_board

常见问题
--------

**Q: 是否可以不创建 Kconfig 文件？**

不可以。Kconfig 文件是必需的，必须包含 ``BOARD_NAME`` 和 ``BOARD_<名称大写>`` 两个配置项。

**Q: 可以手动编辑 pinmux.c 吗？**

不建议。应在 Pinmux 工具中修改后重新生成。

**Q: board.c 中还需要实现其他函数吗？**

当前只需实现 ``board_get_name()``。未来如有更多板级初始化需求，可能会增加其他接口。
