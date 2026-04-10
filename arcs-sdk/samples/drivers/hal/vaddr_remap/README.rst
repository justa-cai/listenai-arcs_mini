Virtual Address Remap 示例
===========================

功能说明
--------

本示例演示如何使用 ``vaddr_remap`` HAL 驱动将 Region A 虚拟地址映射到 Flash 物理偏移，并通过该虚拟地址启动固件。

示例包含两个固件，源码分别位于当前示例目录及其 ``fw/`` 子目录：

- **vaddr_remap_loader** — 加载器，源码位于当前目录，负责配置 Region A 映射并跳转到 fw_a
- **vaddr_remap_fw_a** — 目标固件（fw_a），源码位于 ``fw/`` 子目录，链接在 Region A 虚拟地址（0x08000000），通过 ExternalProject 自动构建

硬件连接
--------

无需外部连接，仅需要开发板正常工作即可。

Flash 布局
----------

.. code-block:: text

   0x00000 ┌──────────────────────┐
           │  vaddr_remap_loader  │  链接在 0x30000000 (XIP)
           │  (max 128KB)         │
   0x20000 ├──────────────────────┤
           │  vaddr_remap_fw_a    │  链接在 0x08000000 (Region A)
           │                      │
           └──────────────────────┘

工作原理
--------

1. Loader 启动后，通过 ``vaddr_remap`` API 将 Region A 虚拟地址映射到 fw_a 所在的 Flash 偏移（0x20000）
2. 直接跳转到 Region A 虚拟地址执行 fw_a

构建与烧录
----------

在 SDK 根目录下执行：

.. code-block:: bash

   ./build.sh -S samples/drivers/hal/vaddr_remap -C -DBOARD=arcs_evb

当前示例的 CMakeLists.txt 会通过 ``ExternalProject_Add`` 自动编译 fw_a，在默认 ``build/`` 输出目录下产出两个 bin 文件：

- ``vaddr_remap_loader.bin`` — 烧录到 0x0
- ``fw_a/vaddr_remap_fw_a.bin`` — 烧录到 0x20000

烧录：

.. code-block:: bash

   ./tools/burn/cskburn -C arcs -s /dev/ttyACM0 -b 3000000 \
       0x0 build/vaddr_remap_loader.bin \
       0x20000 build/fw_a/vaddr_remap_fw_a.bin

预期输出
--------

.. code-block:: text

   === vaddr_remap sample: loader ===
   I/fw_a  >>> Firmware A running! <<<
   I/fw_a  Region A: vaddr 0x08000000
   I/fw_a  Flash offset: 0x20000

核心 API
--------

.. list-table::
   :header-rows: 1

   * - API
     - 说明
   * - ``vaddr_remap_init()``
     - 初始化虚拟地址映射，本示例选择 Flash 作为目标设备
   * - ``vaddr_remap_map()``
     - 设置指定 cipher region 的物理偏移
   * - ``vaddr_remap_apply()``
     - 将映射配置写入硬件寄存器

注意事项
--------

1. **纯 Flash 启动路径**: 本示例只验证从 Flash 取指启动 fw_a，不依赖额外硬件预热或软件延时
2. **最小启动路径**: 当前示例的最小成功路径就是完成 Region A remap 后直接跳转到 fw_a
3. **Loader 大小限制**: Loader 必须小于 128KB（0x20000），否则会覆盖 fw_a 区域
4. **自定义链接地址**: fw_a 使用 ``CONFIG_ARCS_STARTUP_CUSTOM_LINKER_FILE=y`` 将代码链接到 Region A 虚拟地址（0x08000000），而非默认的 XIP 地址
