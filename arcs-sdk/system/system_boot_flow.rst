.. _system_boot_flow:

================
系统启动流程
================

本文档描述 ARCS SDK 在 Nuclei RISC-V 核（ARCS SoC）上从芯片复位到 ``main()`` 的完整启动流程，并说明各阶段涉及的关键源文件。

整体流程概览
============

::

    复位
      └─► [Stage 1] 汇编启动 (_start)            soc/arcs/startup/startup.S
            ├─ 禁用全局中断
            ├─ 初始化 GP / TP / SP
            ├─ 配置 ECLIC 中断入口（早期异常入口）
            └─► scatload()   — 将各段从 ROM 复制到 RAM

      └─► [Stage 2] C 入口 (system_entry)         system/sys_entry.c
            ├─ soc_pre_init()                      — 芯片预初始化钩子（弱符号）
            ├─ soc_init()                          — 芯片底层初始化
            ├─ sys_init_run_level(PRE_SYSTEM_INIT) — 串口等早期外设初始化
            ├─ boot_banner()                       — 打印 SDK 版本横幅
            ├─ sysheap_init()                      — 系统堆初始化
            ├─ console_init()                      — Console 初始化
            ├─ lisa_log_init()                     — 日志系统初始化
            ├─ sys_init_run_level(PRE_DEVICES_INIT)— 设备驱动早期初始化
            ├─ sys_init_run_level(PRE_KERNEL)      — RTOS 启动前初始化
            ├─ cpp_init()                          — C++ 全局构造函数
            └─► [FreeRTOS] vTaskStartScheduler()
                  ├─ sys_init_run_level(POST_KERNEL)
                  ├─ sys_init_run_level(PRE_APPLICATION)
                  └─► main()

               [裸机]  直接调用 main()


Stage 1 — 汇编启动
==================

**源文件：** ``soc/arcs/startup/startup.S``

芯片复位后，PC 跳转到 ``.vtable`` 段第 0 项（ ``j _start`` ），进入汇编启动代码。

.. list-table::
   :widths: 5 35 60
   :header-rows: 1

   * - 步骤
     - 操作
     - 说明
   * - 1
     - ``csrc MSTATUS, MIE``
     - 禁用全局中断，防止初始化期间误触发
   * - 2
     - 初始化 GP / TP
     - 使用 ``.option norelax`` 确保 ``la gp, __global_pointer$`` 正确
   * - 3
     - 初始化 SP
     - ``la sp, __StackTop`` ，指向主栈顶
   * - 4
     - 配置 NMI 共享 mtvec
     - 设置 ``MMISC_CTL.NMI_CAUSE_FFF`` ，NMI 复用异常入口
   * - 5
     - 配置 ECLIC 向量基址
     - mtvt = vector_base，mtvt2 = irq_entry（非向量中断入口）
   * - 6
     - 设置早期异常入口
     - ``mtvec = early_exc_entry`` ，此阶段触发异常直接 WFI 死循环
   * - 7
     - 开启 FPU / Vector
     - 按 ``march`` 编译选项条件使能
   * - 8
     - 使能性能计数器
     - 清除 ``MCOUNTINHIBIT`` 的 CY/IR 位，使能 cycle/instret 计数
   * - 9
     - ``call scatload``
     - 将 LMA≠VMA 的段（代码/数据）从 ROM 复制到 RAM
   * - 10
     - ``call system_entry``
     - 进入 C 初始化入口，**不返回**

向量表结构
----------

- 偏移 0： ``j _start`` （复位跳转入口）
- 偏移 4 ~ N：各中断处理入口（共 78 个）
- 偏移 80 ~ 143：Image Header，含魔数 ``Hr`` 、版本、VMA、镜像名 ``ARCS_APP``


Stage 2 — SoC 底层初始化
=========================

**源文件：** ``soc/arcs/startup/soc.c``

``system_entry`` 首先调用 ``soc_init()`` ，完成芯片底层硬件初始化：

.. list-table::
   :widths: 5 30 65
   :header-rows: 1

   * - 顺序
     - 函数/操作
     - 说明
   * - 1
     - 读取 HARTID
     - ``__RV_CSR_READ(CSR_MHARTID) & 0xff`` ，存入全局变量
   * - 2
     - ``_get_iregion_info``
     - 读取 ``MCFG_INFO`` ，确定 ECLIC / SysTimer 基址
   * - 3
     - ``irq_vectors_init``
     - 初始化 OS 中断向量表，注册 MSIP / MTIP 默认处理函数
   * - 4
     - ``ClockInit``
     - 调用 ``BootClock_Init()`` ，更新 ``SystemCoreClock`` ，使能 MTIME 时钟
   * - 5
     - ICache / DCache
     - 按 Kconfig ``CONFIG_ICACHE_ENABLE`` / ``CONFIG_DCACHE_ENABLE`` 配置
   * - 6
     - ``non_cacheable_region_enable``
     - 将 WiFi RAM 区域配置为非缓存区
   * - 7
     - ``PMP_Init``
     - 配置物理内存保护（ILIM/DLIM/SRAM/PSRAM/FLASH 各分区权限）
   * - 8
     - ``Exception_Init``
     - 将所有异常处理函数初始化为默认处理器
   * - 9
     - ``ECLIC_Init``
     - 设置 MTH=0，配置 NLBits
   * - 10
     - 切换正式异常入口
     - ``mtvec = exc_entry`` ，切换到 ECLIC 模式（ ``mtvec[1:0] = 0b11`` ）
   * - 11
     - 使能 BPU
     - 设置 ``MMISC_CTL.BPU`` ，启用分支预测
   * - 12
     - 设置 MSCRATCH
     - 配置异常回溯（backtrace）使用的栈顶地址
   * - 13
     - PSRAM 初始化
     - ``PSRAM_Initialize()`` ，随后 DCache invalidate，加载 PSRAM 段
   * - 14
     - Watchdog 初始化
     - 按 ``CONFIG_WATCHDOG_ENABLE`` / ``CONFIG_BOOT_WITH_WATCHDOG`` 配置


Stage 3 — 系统框架初始化
=========================

**源文件：** ``system/sys_entry.c``

``soc_init()`` 返回后， ``system_entry`` 按以下顺序初始化系统框架：

.. code-block:: none

    soc_pre_init()                         ← 弱符号，SoC 级最早钩子（当前为空实现）
    soc_init()                             ← 见 Stage 2
    sys_init_run_level(PRE_SYSTEM_INIT)    ← 串口/早期外设，console 之前必须完成
    boot_banner()                          ← 打印 SDK 版本信息
    sysheap_init()                         ← 系统堆初始化（须在其他组件之前）
    console_init()                         ← Console 初始化（依赖 heap）
    lisa_log_init()                        ← 日志系统初始化
    printf_log_redirect_enable()           ← printf/printk 重定向到日志系统
    sys_init_run_level(PRE_DEVICES_INIT)   ← 设备驱动早期注册
    pre_main_hook()                        ← 弱符号，用户可覆盖的扩展点
    sys_init_run_level(PRE_KERNEL)         ← RTOS 启动前组件初始化
    cpp_init()                             ← C++ 全局构造函数


Stage 4 — 进入 main()
======================

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

**源文件：** ``system/init/sys_init.c`` ， ``system/init/sys_init.h``

SDK 提供了分级初始化机制，模块可通过宏在指定级别注册初始化函数：

.. list-table::
   :widths: 30 10 60
   :header-rows: 1

   * - 级别
     - 枚举值
     - 触发时机
   * - ``PRE_SYSTEM_INIT``
     - 0
     - ``soc_init()`` 之后，banner 之前。**串口必须在此级别注册**
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

初始化函数通过 linker script 放入对应的 ``__sys_init_<level>_start/end`` 段，
``sys_init_run_level()`` 在对应阶段遍历执行所有已注册函数。


异常处理机制
============

**源文件：** ``soc/arcs/startup/system.c``

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - 阶段
     - 处理方式
   * - 早期启动（scatload 之前）
     - ``mtvec = early_exc_entry`` ，触发异常后执行 WFI 死循环
   * - 正式运行（soc_init 完成后）
     - ``mtvec = exc_entry`` ，进入 ECLIC 非向量模式
   * - 异常分发
     - ``core_exception_handler(mcause, sp)`` → ``SystemExceptionHandlers[EXCn]``
   * - 默认处理
     - 打印 MCAUSE / MEPC / 寄存器现场，可选 coredump 或软件复位
   * - NMI
     - 复用 mtvec，通过 ``MMISC_CTL.NMI_CAUSE_FFF`` 路由到异常处理器


关键源文件索引
==============

.. list-table::
   :widths: 45 55
   :header-rows: 1

   * - 文件
     - 说明
   * - ``soc/arcs/startup/startup.S``
     - 复位入口、中断向量表、Image Header
   * - ``soc/arcs/startup/soc.c``
     - SoC 底层初始化（Clock/Cache/PMP/ECLIC/PSRAM）
   * - ``soc/arcs/startup/system.c``
     - 异常框架、中断向量分发、系统复位 API
   * - ``system/sys_entry.c``
     - ``system_entry()`` ，系统框架初始化主流程
   * - ``system/init/sys_init.c``
     - 分级初始化框架实现
   * - ``system/init/sys_init.h``
     - 分级初始化宏定义（ ``SYS_INIT`` 等）
