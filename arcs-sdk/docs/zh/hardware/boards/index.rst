.. _hw_boards:

开发板
======

ARCS SDK 当前支持以下开发板：

.. list-table::
   :header-rows: 1
   :widths: 15 15 30

   * - 板型
     - SoC
     - 说明
   * - ``arcs_evb``
     - :ref:`LS2684L0U <soc_ls26xx>`
     - EVB 评估板，功能丰富，适用于产品原型开发和功能评估
   * - ``arcs_mini``
     - :ref:`LS2684L0U <soc_ls26xx>`
     - Mini 开发板，紧凑设计，适用于快速原型开发

板型硬件规格
------------

.. toctree::
   :maxdepth: 1

   arcs_evb
   arcs_mini

板型使用指南
------------

板型系统简介
^^^^^^^^^^^^

板型支持系统负责管理不同硬件板卡的配置和初始化。系统采用模块化设计，将引脚复用（pinmux）配置与板级初始化逻辑分离，便于维护和扩展。

主要特性：

- **模块化设计**：板级代码（board.c/h）与引脚配置（pinmux.c/h）分离
- **外部板型支持**：支持在 SDK 外部添加自定义板型，无需修改 SDK 源码
- **两级搜索机制**：优先搜索自定义路径，然后搜索 SDK 内置板型
- **配置管理**：通过 Kconfig 管理板型配置选项

使用内置板型
^^^^^^^^^^^^

通过 ``-DBOARD`` 参数指定板型：

.. code-block:: shell

   # 使用 arcs_mini 板型
   ./build.sh -S samples/helloworld -DBOARD=arcs_mini

   # 使用 arcs_evb 板型
   ./build.sh -S samples/helloworld -DBOARD=arcs_evb

使用外部自定义板型
^^^^^^^^^^^^^^^^^^

使用 ``-DBOARD`` 和 ``-DBOARD_SEARCH_PATH`` 参数：

.. code-block:: shell

   ./build.sh -S samples/helloworld \
       -DBOARD=my_custom_board \
       -DBOARD_SEARCH_PATH=/path/to/my_boards

.. warning::
   ``BOARD_SEARCH_PATH`` 必须使用绝对路径（如 ``/home/user/my_boards`` ），不能使用相对路径。

板型搜索机制
^^^^^^^^^^^^

构建系统按以下优先级搜索板型：

1. 外部自定义路径（如果指定了 ``BOARD_SEARCH_PATH`` ）： ``${BOARD_SEARCH_PATH}/${BOARD}/``
2. SDK 内置路径（备用）： ``${ARCS_SDK_BASE}/boards/${BOARD}/``

板型目录必须存在且包含 ``CMakeLists.txt`` 文件。如果搜索失败，构建系统会给出详细的错误提示。

板型文件结构
^^^^^^^^^^^^

每个板型目录必须包含以下文件：

.. code-block:: text

   board_name/
   ├── CMakeLists.txt    # 板型构建脚本（必需）
   ├── board.h           # 板级接口声明（必需）
   ├── board.c           # 板级初始化实现（必需）
   ├── pinmux.h          # 引脚配置声明（必需）
   ├── pinmux.c          # 引脚配置实现（必需）
   ├── Kconfig           # 板型配置选项（必需）
   └── README.md         # 板型说明文档（推荐）

创建自定义板型的详细步骤请参考：

.. toctree::
   :maxdepth: 1

   custom_board

故障排查
^^^^^^^^

**BOARD 未指定**

.. code-block:: text

   BOARD is not specified!

必须使用 ``-DBOARD`` 参数指定板型。

**板型未找到**

.. code-block:: text

   Board 'xxx' not found!

检查板型名称拼写、板型目录是否存在、``BOARD_SEARCH_PATH`` 路径是否正确。

**板型目标重复定义**

.. code-block:: text

   add_library cannot create target "module_boards" because another target
   with the same name already exists.

使用外部板型时，SDK 已自动将板型目录添加到构建系统，外部工程 CMakeLists.txt 中无需额外添加。
