.. _system_boot_flow:

================
系统启动流程
================

本文档描述 ARCS SDK 从芯片复位到 ``main()`` 的通用启动流程。SDK 将启动过程
拆分为“SoC/芯片族适配层”和“系统通用启动框架”：前者负责完成进入 C 入口前
必须依赖芯片的工作，后者从 ``system_entry()`` 开始，按固定顺序初始化 SDK
基础设施、驱动和应用。

适用范围
========

SDK 启动流程采用分层结构：

- **SoC/芯片族适配层**：由当前选择的 ``soc/<chip>/`` 目录提供，负责复位入口、
  早期运行环境、内存搬移和芯片底层初始化。
- **SDK 通用框架**：由 ``system/sys_entry.c`` 和 ``system/init/`` 提供，负责
  ``SYS_INIT`` 分级初始化、heap/console/log 初始化、RTOS/裸机分流以及进入
  ``main()``。
- **应用入口**：用户代码只需要提供 ``main(int argc, char *argv[])``，无需关心
  复位入口和早期芯片初始化细节。

不同芯片系列可以有不同的汇编启动文件和 ``soc_init()`` 实现，但必须最终调用
``system_entry()`` 接入 SDK 通用启动框架。

整体流程概览
============

::

    复位
      └─► [Stage 1] SoC 启动适配层
            ├─ 建立最小运行环境（栈、全局指针、早期异常入口等）
            ├─ 完成运行前必须的段搬移/清零
            └─► system_entry()

      └─► [Stage 2] SDK 通用系统初始化              system/sys_entry.c
            ├─ soc_pre_init()                      — SoC 最早期扩展钩子（弱符号）
            ├─ soc_init()                          — SoC 底层初始化入口
            ├─ sys_reset_reason_snapshot()         — 快照复位原因
            ├─ sys_init_run_level(PRE_SYSTEM_INIT) — 串口等早期外设初始化
            ├─ boot_banner()                       — 打印 SDK 版本横幅
            ├─ sysheap_init()                      — 系统堆初始化
            ├─ console_init()                      — Console 初始化
            ├─ lisa_log_init()                     — 日志系统初始化
            ├─ sys_init_run_level(PRE_DEVICES_INIT)— 设备驱动早期初始化
            ├─ pre_main_hook()                     — 应用进入前扩展钩子（弱符号）
            ├─ sys_init_run_level(PRE_KERNEL)      — RTOS 启动前初始化
            ├─ cpp_init()                          — C++ 全局构造函数
            └─► [FreeRTOS] vTaskStartScheduler()
                  ├─ sys_init_run_level(POST_KERNEL)
                  ├─ sys_init_run_level(PRE_APPLICATION)
                  └─► main()

               [裸机]  直接调用 main()


Stage 1 — SoC 启动适配层
=========================

**典型源文件：** ``soc/<chip>/startup.S`` 或 ``soc/<chip>/common/startup/startup.S``

芯片复位后首先进入 SoC/芯片族提供的启动适配层。该阶段的实现允许因芯片架构、
核数量、BootROM 协议、内存布局和中断控制器不同而变化，SDK 对它的核心要求是：
在进入 ``system_entry()`` 前建立 C 代码可运行的最小环境。

常见职责包括：

- 关闭或屏蔽早期中断，避免初始化期间误触发。
- 初始化栈、全局指针、线程指针等基础运行环境。
- 设置早期异常入口，使启动早期异常可被捕获或停机。
- 根据链接脚本生成的信息完成 ROM 到 RAM 的段搬移，以及 BSS 清零。
- 按需准备中断向量表、镜像头或 BootROM 约定的元数据。
- 调用 ``system_entry()``，把控制权交给 SDK 通用启动框架。

该阶段不要求所有 SoC 文件结构完全一致；只要最终完成上述职责并进入
``system_entry()``，即可接入通用流程。


Stage 2 — SDK 通用系统初始化
=============================

**源文件：** ``system/sys_entry.c``

``system_entry()`` 是 SDK 通用 C 入口。它先调用 SoC 钩子完成芯片底层准备，随后
按固定顺序初始化 SDK 基础设施和分级初始化项。

.. list-table::
   :widths: 5 35 60
   :header-rows: 1

   * - 顺序
     - 函数/阶段
     - 说明
   * - 1
     - ``soc_pre_init()``
     - SoC 级最早期扩展钩子，默认弱符号为空实现
   * - 2
     - ``soc_init()``
     - SoC 底层初始化入口，由具体 SoC/芯片族提供强符号实现
   * - 3
     - ``sys_reset_reason_snapshot()``
     - 快照本次复位原因，并由系统框架统一管理清除时机
   * - 4
     - ``sys_init_run_level(PRE_SYSTEM_INIT)``
     - 早期系统初始化，串口等日志前置外设通常在此阶段注册
   * - 5
     - ``boot_banner()``
     - 打印 SDK 版本、构建信息和复位原因
   * - 6
     - ``sysheap_init()``
     - 初始化系统堆，供后续组件创建对象或同步原语
   * - 7
     - ``console_init()``
     - 初始化 Console 子系统
   * - 8
     - ``lisa_log_init()``
     - 初始化日志系统，并可按配置接管 ``printf``/``printk`` 输出
   * - 9
     - ``sys_init_run_level(PRE_DEVICES_INIT)``
     - 设备驱动和组件的早期初始化
   * - 10
     - ``pre_main_hook()``
     - 应用进入前扩展钩子，默认弱符号为空实现
   * - 11
     - ``sys_init_run_level(PRE_KERNEL)``
     - RTOS 调度器启动前的组件初始化
   * - 12
     - ``cpp_init()``
     - 执行 C++ 全局构造函数（未配置延迟初始化时）


Stage 3 — 进入 main()
======================

``system_entry()`` 完成通用初始化后，根据是否启用 FreeRTOS 进入不同路径。

FreeRTOS 模式
-------------

.. code-block:: none

    xTaskCreate(main_task, ...)       ← 创建主任务
    vTaskStartScheduler()             ← 启动调度器

    ── 调度器运行后（main_task 中）──
    sys_init_run_level(POST_KERNEL)       ← RTOS 运行后的组件初始化
    sys_init_run_level(PRE_APPLICATION)   ← 应用级初始化
    main()                                ← 用户应用入口

裸机模式
--------

.. code-block:: none

    main()         ← 直接进入用户应用
    while(1) {}    ← main() 返回后死循环


sys_init 分级初始化机制
========================

**源文件：** ``system/init/sys_init.c`` ， ``system/init/sys_init.h`` ，
``system/init/sys-init-sections.ld``

SDK 提供分级初始化机制，模块可通过宏在指定级别注册初始化函数。系统启动到
对应阶段时， ``sys_init_run_level()`` 会遍历该级别的 linker section 并执行所有
已注册函数。

.. list-table::
   :widths: 30 10 60
   :header-rows: 1

   * - 级别
     - 枚举值
     - 触发时机
   * - ``PRE_SYSTEM_INIT``
     - 0
     - ``soc_init()`` 之后，banner 之前。串口等日志前置外设通常在此级别注册
   * - ``PRE_DEVICES_INIT``
     - 1
     - console/log 初始化之后， ``pre_main_hook`` 之前
   * - ``PRE_KERNEL``
     - 2
     - RTOS 调度器启动之前
   * - ``POST_KERNEL``
     - 3
     - RTOS 调度器启动之后（仅 FreeRTOS 模式）
   * - ``PRE_APPLICATION``
     - 4
     - ``main()`` 调用之前（仅 FreeRTOS 模式）

注册方式示例：

.. code-block:: c

    /* 在 PRE_KERNEL 阶段，子优先级 10 执行 my_driver_init */
    SYS_INIT(my_driver_init, PRE_KERNEL, 10);

初始化函数通过 ``system/init/sys-init-sections.ld`` 放入对应的
``__sys_init_<level>_start/end`` 段，``sys_init_run_level()`` 在对应阶段遍历执行
所有已注册函数。


SoC 适配边界
============

SDK 通用启动框架只依赖少量 SoC 入口和弱符号钩子：

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - 接口/职责
     - 说明
   * - ``system_entry()``
     - SDK 通用入口；SoC 启动适配层必须最终跳转或调用到这里
   * - ``soc_pre_init()``
     - SoC 最早期扩展点；默认弱符号为空实现，SoC 可按需覆盖
   * - ``soc_init()``
     - SoC 底层初始化入口；具体 SoC 通常在这里完成时钟、内存、中断控制器、cache 等初始化
   * - ``soc_cpu_id_get()``
     - 可选 CPU/hart id 查询；默认弱符号返回不支持
   * - 链接脚本和段搬移
     - SoC 适配层和通用 linker fragment 共同决定镜像布局、段搬移和 ``SYS_INIT`` 段边界
   * - 早期/正式异常入口
     - 通常由 SoC 适配层安装；通用框架不假设所有芯片使用相同异常控制器


关键源文件索引
==============

.. list-table::
   :widths: 45 55
   :header-rows: 1

   * - 文件
     - 说明
   * - ``system/sys_entry.c``
     - ``system_entry()`` ，SDK 通用系统初始化主流程
   * - ``system/init/sys_init.c``
     - 分级初始化框架实现
   * - ``system/init/sys_init.h``
     - 分级初始化宏定义（ ``SYS_INIT`` 等）
   * - ``system/init/sys-init-sections.ld``
     - ``SYS_INIT`` 的 linker fragment，导出各级 ``__sys_init_*`` 段边界符号
   * - ``soc/common/system.ld``
     - SDK 通用链接脚本模板
   * - ``soc/<chip>/``
     - SoC/芯片族启动适配、底层初始化和芯片相关链接片段所在目录
