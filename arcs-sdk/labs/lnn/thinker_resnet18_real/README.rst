Thinker ResNet18 实时识别示例
=============================

功能说明
--------

本示例演示在 ARCS 双核系统中实现摄像头实时预览和 ResNet18 分类识别：

- CP Core 负责摄像头采集、LVGL 界面、LCD 显示和触摸按钮。
- AP Core 负责初始化 Luna、Thinker，并运行 ``resnet18_arcs.bin`` 模型。
- CP 和 AP 通过 mailbox 做识别请求/结果的轻量通知。
- 图像输入 tensor 和识别结果通过共享 RAM 中的控制块传递。

运行时，CP 从摄像头采集 RGB565 图像，取中心 ``480x480`` 窗口并缩放为
``240x240`` 显示到 LCD；点击界面上的 ``RUN`` 按钮后，CP 冻结用于识别的
这一帧，直到点击 ``PREVIEW`` 按钮重新预览。CP 将同一个 ``480x480`` 窗口
缩放并量化为模型输入 ``int8 [1, 3, 32, 32]``，
写入共享内存并通知 AP。AP 完成 ResNet18 推理后把分类标签、score 和状态写回
共享内存，再通过 mailbox 通知 CP 刷新 LCD 显示。

双核与 IPC 规划
---------------

默认内存和 Flash 规划如下：

.. code-block:: text

   AP PSRAM:      0x28000000 .. 0x28700000
   CP 私有 PSRAM: 0x28130000 .. 0x28590000
   LUNA shared:   0x20048000 .. 0x2004D000
   IPC RAM:       0x2004D000 .. 0x20050000
   Thinker APRAM: 0x20051000 .. 0x200A9000
   AP 固件:      0x30000000, flash offset 0x000000
   CP 固件:      0x30200000, flash offset 0x200000
   ResNet18 模型: 0x30500000, flash offset 0x500000

``lnn_resnet18_real_ipc_t`` 通过 ``.ipc.lnn`` 段链接进 SDK 使用的
``.ipc.shared`` 区域，CP/AP 双方访问同一个对象。
mailbox 只传递 ``seq`` 和状态，不承载图像数据；图像 tensor 和结果字段都在
``lnn_resnet18_real_ipc_t`` 中传递。

AP 侧运行期日志参考 ``samples/subsys/logger/dual_core_ipc`` 通过 HAL IPC
转发到 CP 端打印，CP 串口中会带 ``[AP]`` 前缀。

.. include:: /sample_build.rst

烧录说明
--------

工程构建时会把 AP 固件、CP 固件和 ResNet18 模型合并为
``build/merged.bin``。默认可直接烧录合并镜像：

.. code-block:: shell

   cd labs/lnn/thinker_resnet18_real
   cskburn -C arcs -s /dev/ttyACM0 -b 3000000 0x0 build/merged.bin

如果需要分开烧录，可按以下偏移写入：

.. code-block:: shell

   cskburn -C arcs -s /dev/ttyACM0 -b 3000000 0x0      build/remote/thinker_resnet18_real_ap.bin
   cskburn -C arcs -s /dev/ttyACM0 -b 3000000 0x200000 build/thinker_resnet18_real_cp.bin
   cskburn -C arcs -s /dev/ttyACM0 -b 3000000 0x500000 ../thinker_resnet18/resources/resnet18_arcs.bin

运行流程
--------

1. AP Core 从 ``CONFIG_THINKER_RESNET18_REAL_CP_FLASH_BASE`` 启动 CP Core。
2. AP/CP Core 初始化共享 IPC 控制块，并打开 LNN 专用 mailbox channel。
3. CP Core 初始化 LCD、触摸、LVGL 和摄像头，并开始中心 ``480x480`` 窗口实时预览。
4. AP Core 初始化 Luna、Thinker、DMA copy 和 ResNet18 模型。
5. 用户点击 ``RUN`` 按钮后，CP Core 冻结当前预览帧，预处理同一帧并通知 AP Core。
6. AP Core 推理完成后写回 ``label``、``score`` 和 ``index``，并通知 CP Core。
7. CP Core 在 LCD 上显示识别结果；点击 ``PREVIEW`` 可恢复实时预览。

预期输出
--------

串口日志中可观察到 AP 和 CP 的关键状态：

.. code-block:: text

   [AP] [I][lnn_real_ap] boot CP from 0x30200000
   [AP] [I][resnet18_ap] memory plan: entries=...
   [AP] [I][resnet18_ap] tCreateExecutor ok
   [AP] [I][lnn_real_ap] AP ready for request
   [I][lnn_real_cp] camera started: ... format=...
   [I][lnn_real_cp] ap shared status seq=0 state=1 message=AP ready
   [I][lnn_real_cp] submit frame seq=... prev_state=... window=480x480+80+0 ... notify=mbox:0
   [I][lnn_real_cp] ap shared status seq=... state=2 message=CP request seq=...
   [AP] [I][lnn_real_ap] request accepted seq=... input_bytes=3072
   [AP] [I][resnet18_ap] forward start
   [I][lnn_real_cp] ap shared status seq=... state=4 message=done
   [AP] [I][resnet18_ap] best score: ..., index: ..., label: ...
   [AP] [I][lnn_real_ap] heartbeat phase=wait ... mbox_msgs=1 msg=1/2 ... state=4 ...

核心文件
--------

.. list-table::
   :header-rows: 1

   * - 文件
     - 说明
   * - ``src/main.c``
     - CP Core 侧入口和任务创建
   * - ``src/app.c``
     - CP Core 侧摄像头、LVGL、LCD 和触摸逻辑
   * - ``src/ap_client.c``
     - CP Core 侧模型输入预处理、共享 IPC 读写和 AP 请求封装
   * - ``src/ipc_client.c``
     - CP Core 侧 mailbox 通知和共享 IPC cache/lock 封装
   * - ``remote/src/main.c``
     - AP Core 侧 CP 启动、mailbox 通知和推理任务
   * - ``remote/src/resnet18_real_ap.c``
     - AP Core 侧 Thinker ResNet18 初始化和推理封装
   * - ``include/lnn_resnet18_real_ipc.h``
     - CP/AP 共享的 IPC 控制块和状态定义

注意事项
--------

1. CP 固件需要从 AP 固件启动，因此本示例默认烧录 ``merged.bin``。
2. CP 使用摄像头 DVP、LCD SPI、触摸 I2C 和 mailbox；和其他外设组合时需要确认引脚、DMA channel 和 mailbox channel 不冲突。
3. AP 侧运行 Thinker/Luna，模型资源必须与 ``CONFIG_THINKER_RESNET18_REAL_MODEL_ADDR`` 和 ``CONFIG_THINKER_RESNET18_REAL_MODEL_SIZE`` 保持一致。
4. IPC 共享块链接在 ``.ipc.lnn``，需要 CP/AP 两侧 ``CONFIG_MEM_IPC_BASE`` 和 ``CONFIG_MEM_IPC_SIZE`` 保持一致；当前 IPC RAM 使用 ``0x2004D000..0x20050000``，AP Thinker SHARE_MEM 池从 ``0x20051000`` 开始，避免互相覆盖。
