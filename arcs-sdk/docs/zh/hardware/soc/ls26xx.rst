.. _soc_ls26xx:

LS26xx 系列芯片
===============

概述
----

LS26xx 是聆思科技推出的 ARCS 架构 SoC 系列，采用 22nm 工艺，集成双核 RISC-V 处理器（最高 300MHz），适用于 AIoT、智能语音、多媒体等应用场景。

全系列共享相同的外设资源和封装，仅 PSRAM 容量和是否集成 NPU 有差异。

芯片选型
--------

.. list-table::
   :header-rows: 1
   :widths: 12 6 6 6 6 6 6 6 6 6 6 6 6 6 6 6 6

   * - 型号
     - 内核
     - 工艺
     - SRAM
     - PSRAM
     - Flash
     - NPU
     - GPIO
     - UART
     - SPI
     - QSPI
     - I2C
     - I2S
     - SDIO
     - DVP
     - USB
     - PWM
   * - LS2662L0U
     - Dual RISC-V 300MHz
     - 22nm
     - 704KB
     - 4MB
     - 外挂 NOR
     - \-
     - A×32 + B×10
     - 3
     - 2
     - 1
     - 2
     - 1
     - 1
     - 1
     - 1
     - 多路
   * - LS2663L0U
     - Dual RISC-V 300MHz
     - 22nm
     - 704KB
     - 8MB
     - 外挂 NOR
     - \-
     - A×32 + B×10
     - 3
     - 2
     - 1
     - 2
     - 1
     - 1
     - 1
     - 1
     - 多路
   * - LS2664L0U
     - Dual RISC-V 300MHz
     - 22nm
     - 704KB
     - 16MB
     - 外挂 NOR
     - \-
     - A×32 + B×10
     - 3
     - 2
     - 1
     - 2
     - 1
     - 1
     - 1
     - 1
     - 多路
   * - LS2682L0U
     - Dual RISC-V 300MHz
     - 22nm
     - 704KB
     - 4MB
     - 外挂 NOR
     - 64G ops
     - A×32 + B×10
     - 3
     - 2
     - 1
     - 2
     - 1
     - 1
     - 1
     - 1
     - 多路
   * - LS2683L0U
     - Dual RISC-V 300MHz
     - 22nm
     - 704KB
     - 8MB
     - 外挂 NOR
     - 64G ops
     - A×32 + B×10
     - 3
     - 2
     - 1
     - 2
     - 1
     - 1
     - 1
     - 1
     - 多路
   * - LS2684L0U
     - Dual RISC-V 300MHz
     - 22nm
     - 704KB
     - 16MB
     - 外挂 NOR
     - 64G ops
     - A×32 + B×10
     - 3
     - 2
     - 1
     - 2
     - 1
     - 1
     - 1
     - 1
     - 多路

.. tip::
   LS266x 与 LS268x 的区别仅在于是否集成 NPU，其余完全一致。全系列封装均为 QFN76。

处理器架构
----------

LS26xx 采用双核异构设计：

- **AP 核（应用处理器）**：运行 FreeRTOS 及用户应用
- **CP 核（通信处理器）**：运行 WiFi/BT 协议栈

两核通过共享内存和硬件邮箱进行通信。

外设说明
--------

.. list-table::
   :header-rows: 1
   :widths: 15 45

   * - 外设
     - 说明
   * - GPIO
     - GPIOA（32 引脚）+ GPIOB（10 引脚），支持中断、唤醒
   * - UART
     - 3 路（UART0/1/2）
   * - SPI
     - 2 路（SPI0/SPI1）
   * - QSPI
     - 1 路，用于 LCD 或 Flash
   * - I2C
     - 2 路 I2C 主机
   * - I2S
     - 音频数字接口
   * - SDIO
     - 1 路，SD 卡 / eMMC
   * - DVP
     - 数字视频接口（摄像头）
   * - USB
     - USB 2.0（通过 TinyUSB 协议栈）
   * - ADC
     - GPADC，支持 AON 域采集
   * - PWM
     - 多路输出
   * - 模拟音频
     - 内置 ADC/DAC，支持麦克风输入和差分线路输出
   * - WiFi
     - 802.11 b/g/n
   * - Bluetooth
     - BLE 5.0 + Classic（A2DP）
