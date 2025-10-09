# Display 模块

## 1. 概述

`components/display` 模块负责驱动和管理显示设备，为上层应用提供统一的图形显示接口。它支持多种显示控制器（如 ST7789P3, GC9309NA, AXS15231B 等）和数据传输方式（如 QSPI, 4线SPI），并通过 Kconfig 进行灵活配置。该模块设计良好，具有清晰的分层结构，易于扩展和维护。

## 2. 模块架构

Display 模块采用分层设计，从上到下依次是：应用层 -> LISA Display API 层 -> 具体显示驱动层 -> 显示通用功能层 -> 传输上下文层 -> 物理传输驱动层 -> 硬件抽象层 (HAL)。这种分层确保了上层应用代码的稳定性和底层硬件实现的可替换性。

### 交互流程图

```mermaid
graph TD
    A[Application <br/> (e.g., samples/drivers/display/src/main.c)] -->|调用 lisa_display_* API| B(LISA Display API <br/> components/display/lisa_display.h/c <br/> 统一接口层 <br/> e.g., lisa_display_write(), lisa_display_create());
    B -->|根据 Kconfig 配置, <br/> 调用选定驱动的 API| C{Specific Display Driver <br/> (e.g., display_st7789p3.c) <br/> 实现 display_driver_api};
    C --> D[display_common <br/> components/display/display_common.h/c <br/> 通用工具, GPIO, PWM, TE, 区域计算];
    C -->|调用 display_trans_ctx_* API| E(display_trans_ctx API <br/> components/display/trans_ctx/display_trans_ctx.h/c <br/> 传输接口与旋转处理 <br/> e.g., display_trans_image(), display_trans_cmd_data());
    E -->|通过 ops 函数指针, <br/> 调用选定的物理传输驱动| F{Physical Trans Driver <br/> (e.g., qspi_trans.c or spi_4line_trans.c) <br/> 实现 display_trans_ops};
    F --> G[Hardware Abstraction Layer (HAL) <br/> (e.g., SoC提供的 Driver_SPI.h 等) <br/> 实际控制SPI/QSPI硬件];
    F -->|操作硬件| H[(Hardware <br/> Display Panel)];
```

## 3. 关键组件详解

### 3.1. LISA Display API 层 (`components/display/lisa_display.h`, `components/display/lisa_display.c`)

*   **路径**: `components/display/lisa_display.h`, `components/display/lisa_display.c`
*   **作用**: 为应用程序提供稳定、统一的显示操作接口，屏蔽底层硬件差异。这是应用层与Display模块交互的唯一入口。
*   **主要API**:
    *   `void *lisa_display_create(void)`:
        *   **功能**: 创建并初始化一个显示设备实例。
        *   **实现**: 根据Kconfig中 `CONFIG_LISA_DISPLAY_*` 的配置（如 `CONFIG_LISA_DISPLAY_ST7789P3`），选择对应的具体显示驱动（如 `display_st7789p3`），并调用其 `device_init` 函数完成初始化。返回一个 `struct display_device` 指针。
    *   `int lisa_display_blanking_on(const struct display_device *dev)`: 打开屏幕显示（通常是点亮背光，并使能显示控制器输出）。
    *   `int lisa_display_blanking_off(const struct display_device *dev)`: 关闭屏幕显示（通常是关闭背光，并禁止显示控制器输出，但帧缓冲内容保留）。
    *   `void lisa_display_get_capabilities(const struct display_device *dev, struct display_capabilities *capabilities)`: 获取显示设备的能力，如X/Y分辨率、支持的像素格式、当前像素格式、当前方向等。
    *   `int lisa_display_set_brightness(const struct display_device *dev, const uint8_t brightness)`: 设置屏幕亮度（0-100）。
    *   `int lisa_display_write(const struct display_device *dev, const uint16_t x, const uint16_t y, const struct display_buffer_descriptor *desc, const void *buf)`: 将 `buf` 指向的、由 `desc` 描述的像素数据块写入到屏幕上以 `(x,y)` 为左上角的区域。
    *   `int lisa_display_set_orientation(const struct display_device *dev, const enum display_orientation orientation)`: 设置屏幕的显示方向（正常、旋转90/180/270度）。
    *   `int lisa_display_sleep(const struct display_device *dev, const uint8_t onoff)`: 控制显示设备进入 (`onoff=1`) 或退出 (`onoff=0`) 睡眠模式。
*   **核心结构**:
    *   `struct display_device` (定义于 `lisa_display.h`):
        ```c
        struct display_device {
            const char *name; // 设备名称
            int (*device_init)(void); // 指向具体驱动的初始化函数
            const struct display_driver_api *api; // 指向具体驱动实现的API函数表
        };
        ```
    *   `struct display_driver_api` (定义于 `lisa_display.h`):
        ```c
        struct display_driver_api {
            int (*display_blanking_on)(void);
            int (*display_blanking_off)(void);
            // ... 其他API函数指针 ...
            int (*display_set_orientation)(const enum display_orientation orientation);
            int (*display_sleep)(const uint8_t onoff);
        };
        ```
    *   `enum display_pixel_format` (定义于 `lisa_display.h`): 支持的像素格式，如 `PIXEL_FORMAT_RGB_565`, `PIXEL_FORMAT_RGB_888`。
    *   `enum display_orientation` (定义于 `lisa_display.h`): 支持的显示方向。
    *   `struct display_capabilities` (定义于 `lisa_display.h`): 描述显示能力。
    *   `struct display_buffer_descriptor` (定义于 `lisa_display.h`): 描述待显示数据缓冲区的布局（宽高、pitch、总大小）。

### 3.2. 具体显示驱动层 (例如 `components/display/display_st7789p3.c`)

*   **路径**: `components/display/display_*.c` 和 `components/display/display_*.h` (例如 `display_st7789p3.c`, `display_st7789p3.h`)
*   **作用**: 实现特定显示控制器的驱动逻辑，适配LISA Display API。
*   **主要职责**:
    1.  **提供 `display_device` 实例**: 在其 `.c` 文件中定义一个 `const struct display_device` 类型的全局实例，例如 `display_st7789p3`，并将其 `device_init` 指向自身的初始化函数 (如 `st7789p3_display_init`)，`api` 指向自身实现的 `display_driver_api` 实例 (如 `st7789p3_driver_api`)。
    2.  **实现 `display_driver_api`**: 实现 `display_driver_api` 结构中定义的所有函数。
        *   初始化函数 (如 `st7789p3_display_init`):
            *   创建同步对象（互斥锁，可选的TE信号量）。
            *   调用 `disp_comm_rst_init()` 和 `disp_comm_pwm_init()` (来自 `display_common`)。
            *   若使用TE同步，调用 `disp_comm_te_init()`。
            *   调用 `display_trans_ctx_init()` 配置传输层（如像素位数16，命令位数8）。
            *   执行控制器特有的硬件复位和初始化命令序列（通过 `display_trans_cmd_data()` 发送）。
            *   设置默认方向，标记初始化完成。
        *   `write` 函数 (如 `st7789p3_display_write`):
            *   获取互斥锁。
            *   可选的TE同步等待 (`disp_comm_te_wait`)。
            *   Cache维护（如 `dcache_clean_range`）。
            *   使用 `disp_comm_set_mem_area()` (来自 `display_common`) 计算并获取实际的列/页地址参数。
            *   通过 `display_trans_cmd_data()` 发送列地址设置 (CASET) 和页地址设置 (RASET) 命令。
            *   将 `lisa_display` 的方向转换为 `display_trans_ctx` 的方向。
            *   通过 `display_trans_image()` 发送写RAM命令和像素数据，由 `display_trans_ctx` 处理旋转。
            *   释放互斥锁。
        *   `set_orientation` 函数 (如 `st7789p3_display_set_orientation`):
            *   更新内部存储的当前方向。
            *   发送控制器特定的命令（如ST7789P3的MADCTL命令0x36）来改变硬件的扫描方向、行列交换等，以匹配新的显示方向。
    3.  **使用 `display_common`**: 调用 `display_common.c` 提供的通用功能。
    4.  **使用 `display_trans_ctx`**: 所有对显示控制器的命令和数据传输都通过 `display_trans_ctx.c` 提供的接口进行。

### 3.3. 显示通用功能层 (`components/display/display_common.h`, `components/display/display_common.c`)

*   **路径**: `components/display/display_common.h`, `components/display/display_common.c`
*   **作用**: 提供被各个具体显示驱动共享的通用代码、数据结构和宏定义，以减少代码冗余并统一常用操作。
*   **主要功能**:
    *   **GPIO控制**:
        *   `disp_comm_rst_init()`, `disp_comm_rst_set()`, `disp_comm_rst_clr()`: 显示屏复位引脚的初始化和控制。
        *   `disp_comm_te_init()`, `disp_comm_te_wait()`: 撕裂效应（TE）信号引脚的初始化、中断配置及基于信号量的等待同步（受 `CONFIG_LISA_DISPLAY_TE_SYNC` Kconfig选项控制）。
    *   **PWM背光控制**:
        *   `disp_comm_pwm_init()`: 初始化用于背光控制的PWM通道。
        *   `disp_comm_set_pwm_duty()`: 设置PWM占空比以调整屏幕亮度。
    *   **显示区域计算**:
        *   `void disp_comm_set_mem_area(struct disp_mem_area_input *input, disp_mem_coord x_output, disp_mem_coord y_output)`: 核心函数。输入参数包括面板总宽高、绘制区域的左上角坐标(x,y)和宽高(w,h)、以及当前显示方向。该函数会根据显示方向进行坐标变换，并计算出发送给显示控制器CASET（列地址设置）和RASET（行地址设置）命令所需的实际起始/结束行列地址（通常是4字节参数，高低位分开）。
    *   **通用数据结构**:
        *   `struct display_obj`: 用于具体驱动存储其内部状态，如 `initialized` 标志, `orientation`, `mutex` (FreeRTOS互斥锁), `te_sem` (FreeRTOS二值信号量)。
        *   `struct disp_init_cmd`: 用于定义初始化命令序列中的单个命令项（命令、数据、数据长度、执行后延时）。
        *   `struct disp_mem_area_input`, `struct disp_mem_area_output`: `disp_comm_set_mem_area` 使用的输入输出结构。
        *   `disp_mem_coord`: `uint8_t[4]` 类型，用于存储CASET/RASET的4字节参数。
    *   **常用显示命令宏定义**: 如 `DISPLAY_COMM_CMD_SLEEP_IN`, `DISPLAY_COMM_CMD_CASET`, `DISPLAY_COMM_CMD_RAMWR` 等。
    *   **其他工具宏**: 如 `delay_ms` (通常是 `SysTick_Delay_Ms`)。

### 3.4. 传输上下文层 (`components/display/trans_ctx/display_trans_ctx.h`, `components/display/trans_ctx/display_trans_ctx.c`)

*   **路径**: `components/display/trans_ctx/display_trans_ctx.h`, `components/display/trans_ctx/display_trans_ctx.c`
*   **作用**: 抽象底层数据传输的复杂性，为具体显示驱动提供统一的数据和命令发送接口。核心功能之一是处理像素数据的动态屏幕旋转。
*   **主要API**:
    *   `int display_trans_ctx_init(uint32_t bpp, uint32_t cmd_bits, uint8_t dc_cmd_level)`:
        *   **功能**: 初始化传输上下文。
        *   **参数**: `bpp` (bits per pixel), `cmd_bits` (LCD命令的位数，通常8或16), `dc_cmd_level` (D/C信号线在发送命令时的有效电平，通常0)。
        *   **实现**: 保存配置参数；若配置了DMA旋转 (`CONFIG_LISA_DISPLAY_CPDMA_ROTATE`)，则初始化DMA并创建信号量；调用 `display_trans_ops_get()` 获取底层物理传输驱动的操作函数表；调用底层驱动的 `trans_init()`。
    *   `int display_trans_cmd_data(int cmd, const void *data, size_t data_len)`:
        *   **功能**: 发送一个命令以及可选的命令参数。
        *   **实现**: 准备命令（可能进行字节序转换）；通过 `ops->trans_dc_trig()` 将D/C线设为命令模式；通过 `ops->trans_cmd()` 发送命令；若有数据，则将D/C线设为数据模式并发送数据。
    *   `int display_trans_image(int cmd, void *buf, uint32_t w, uint32_t h, enum display_trans_orient orient)`:
        *   **功能**: 发送一个写图像命令（如RAMWR）以及后续的像素数据。
        *   **参数**: `cmd` (写RAM命令)，`buf` (原始像素数据缓冲区)，`w`, `h` (原始图像宽高)，`orient` (目标显示方向)。
        *   **实现**:
            1.  发送 `cmd` 命令（通过 `display_trans_cmd_data`）。
            2.  将D/C线设为数据模式。
            3.  **处理旋转**:
                *   若 `orient` 为 `DISPLAY_TRANS_ORIENT_NORMAL` 或 `DISPLAY_TRANS_ORIENT_ROTATED_180` (180度旋转不在此层处理，假定由驱动或硬件处理)，则直接调用 `ops->trans_image()` 发送 `buf` 中的数据。
                *   若 `orient` 为 `DISPLAY_TRANS_ORIENT_ROTATED_90` 或 `DISPLAY_TRANS_ORIENT_ROTATED_270`，则调用内部函数 `display_send_data_with_sram_rotate()`。此函数使用Ping-Pong SRAM缓冲区 (`rotate_buf_ping`/`pong`)，分块从 `buf` 读取数据，调用 `display_buf_rotate_90()` 或 `display_buf_rotate_270()` (DMA或软件实现)进行旋转，然后调用 `ops->trans_image()` 发送旋转后的分块数据。
            4.  调用 `ops->trans_image_wait()` 等待传输完成。
*   **核心机制**:
    *   `struct display_trans_ops` (定义于 `display_trans_ctx.h`):
        ```c
        struct display_trans_ops {
            int  (*trans_init)(void); // 初始化物理传输层
            int  (*trans_image_wait)(uint32_t timeout); // 等待图像数据发送完成
            int  (*trans_image)(void *buf, uint32_t size); // 发送一块图像数据
            int  (*trans_cmd)(uint8_t *data, uint32_t data_len); // 发送命令或命令参数
            void (*trans_cs_trig)(uint8_t level); // 控制片选(CS)信号 (此模块内未直接调用，由物理层管理)
            void (*trans_dc_trig)(uint8_t level); // 控制数据/命令(D/C)信号
        };
        ```
    *   `display_trans_ops_get()`: 这是一个 `__attribute__((weak))` 声明的函数。具体的物理传输驱动（如 `qspi_trans.c`）必须提供此函数的强符号版本，用于返回其实现的 `display_trans_ops` 实例。这是实现传输层与物理层解耦的关键。
    *   **屏幕旋转**:
        *   `display_buf_rotate_90()`, `display_buf_rotate_270()`: 实现像素块的90/270度旋转。
        *   如果 `CONFIG_LISA_DISPLAY_CPDMA_ROTATE` Kconfig选项使能，则使用DMA的链式传输（LLP）进行硬件加速旋转，并通过DMA中断和信号量 (`cpdma_done_sem`) 进行同步。
        *   否则，使用纯软件算法在SRAM中进行像素数据的搬移和旋转。
        *   旋转操作在固定大小的SRAM缓冲区 (`rotate_buf_ping`/`pong`，大小由 `ROTATE_BUF_MAX_HEIGHT` 和 `ROTATE_BUF_WIDTH` Kconfig宏定义) 中进行。

### 3.5. 物理传输驱动层 (例如 `components/display/trans_ctx/qspi_trans.c`)

*   **路径**: `components/display/trans_ctx/qspi_trans.c`, `components/display/trans_ctx/spi_4line_trans.c` 等。
*   **作用**: 实现 `display_trans_ops` 接口，直接与硬件SPI/QSPI控制器等通信接口交互，完成实际的数据传输。
*   **主要职责**:
    1.  **提供 `display_trans_ops_get()` 实现**: 返回一个指向静态 `display_trans_ops` 实例的指针，该实例的函数指针指向本文件内实现的具体传输函数。
    2.  **实现 `display_trans_ops` 接口**:
        *   `trans_init()`: 初始化SPI/QSPI硬件控制器，配置时钟、模式、引脚等。
        *   `trans_image_wait()`: 如果传输是异步的（如DMA），则等待传输完成的信号/事件。
        *   `trans_image()`: 将指定大小的图像数据通过SPI/QSPI发送出去。可能直接轮询发送或启动DMA。
        *   `trans_cmd()`: 将命令或其参数通过SPI/QSPI发送出去。
        *   `trans_cs_trig()`: 控制片选(CS)信号的电平（如果不由SPI/QSPI控制器自动管理）。
        *   `trans_dc_trig()`: 控制数据/命令(D/C)信号的电平。
    3.  **与HAL交互**: 调用SoC厂商提供的硬件抽象层API（如 `Driver_SPI_Send`, `Driver_QSPI_CommandTransfer` 等）来操作硬件。

## 4. 配置

Display模块高度依赖Kconfig进行编译时配置。

*   **顶层显示选择**:
    *   文件: `components/display/Kconfig`
    *   选项: `choice` `LISA_DISPLAY_CONTROLLER`，用于选择激活哪个具体的显示控制器驱动，例如 `config LISA_DISPLAY_ST7789P3`。
*   **具体控制器配置**:
    *   文件: `components/display/Kconfig.st7789p3` (或其他对应控制器的Kconfig文件)
    *   选项:
        *   `CONFIG_DISPLAY_ST7789P3_WIDTH`, `CONFIG_DISPLAY_ST7789P3_HEIGHT`: LCD面板的物理分辨率。
        *   `CONFIG_DISPLAY_ST7789P3_X_OFFSET`, `CONFIG_DISPLAY_ST7789P3_Y_OFFSET`: LCD面板可能存在的显示区域偏移。
        *   `CONFIG_DISPLAY_ST7789P3_DEFAULT_ORIENTATION`: 默认显示方向。
*   **通用显示配置**:
    *   文件: `components/display/Kconfig` (也可能分散在各处)
    *   选项: `CONFIG_LISA_DISPLAY_TE_SYNC`: 是否启用撕裂效应（TE）同步。
*   **传输上下文配置**:
    *   文件: `components/display/trans_ctx/Kconfig.trans_ctx`
    *   选项:
        *   `choice` `LISA_DISPLAY_TRANS_MODE`: 选择传输模式，如 `CONFIG_LISA_DISPLAY_TRANS_MODE_QSPI` 或 `CONFIG_LISA_DISPLAY_TRANS_MODE_SPI_4LINE`。这将决定哪个物理传输驱动 (`qspi_trans.c` 或 `spi_4line_trans.c`) 被编译和链接。
        *   `CONFIG_LISA_DISPLAY_CPDMA_ROTATE`: 是否使用DMA进行屏幕旋转加速。
        *   `CONFIG_LISA_DISPLAY_CPDMA_CH`: 若使用DMA旋转，指定的DMA通道。
        *   `CONFIG_ROTATE_BUF_MAX_HEIGHT`, `CONFIG_ROATE_BUF_WIDTH`: 配置用于SRAM中旋转操作的缓冲区大小。
*   **项目级配置 (`prj.conf`)**: 示例项目中 `prj.conf` 会包含类似以下配置来使能特定的驱动和功能：
    ```conf
    CONFIG_LISA_DISPLAY=y
    CONFIG_LISA_DISPLAY_ST7789P3=y // 或其他选择的控制器
    CONFIG_LISA_DISPLAY_TRANS_MODE_QSPI=y // 或其他选择的传输模式
    CONFIG_LISA_DISPLAY_TE_SYNC=y // 可选
    CONFIG_LISA_DISPLAY_CPDMA_ROTATE=y // 可选
    ```

## 5. 使用示例 (`samples/drivers/display/src/main.c`)

应用程序通过 `lisa_display.h` 提供的API与Display模块交互。典型使用流程如下：

1.  **包含头文件**: `#include "lisa_display.h"`
2.  **创建/获取显示设备实例**:
    ```c
    void *display_dev = lisa_display_create();
    if (!display_dev) { /* handle error */ }
    ```
3.  **获取设备能力**:
    ```c
    struct display_capabilities caps;
    lisa_display_get_capabilities(display_dev, &caps);
    // 使用 caps.x_resolution, caps.y_resolution, caps.current_pixel_format 等
    ```
4.  **准备帧缓冲区描述符**:
    ```c
    struct display_buffer_descriptor buf_desc;
    buf_desc.width = caps.x_resolution; // 或部分窗口的宽度
    buf_desc.height = caps.y_resolution; // 或部分窗口的高度
    buf_desc.pitch = buf_desc.width * (caps.current_pixel_format == PIXEL_FORMAT_RGB_565 ? 2 : 3); // 假设RGB565为2字节/像素
    buf_desc.buf_size = buf_desc.pitch * buf_desc.height;
    ```
5.  **分配并填充帧缓冲区**: (应用层负责)
    ```c
    uint8_t *frame_buffer = malloc(buf_desc.buf_size);
    // ... 用像素数据填充 frame_buffer ...
    // 注意像素格式和字节序，如示例中 fill_buffer_rgb565 将uint16_t颜色拆分为小端字节序
    ```
6.  **使能显示输出 (点亮屏幕)**:
    ```c
    lisa_display_blanking_off(display_dev);
    ```
7.  **写入数据到屏幕**:
    ```c
    lisa_display_write(display_dev, 0, 0, &buf_desc, frame_buffer); // 从(0,0)开始写整个缓冲区
    ```
8.  **其他可选操作**:
    ```c
    lisa_display_set_brightness(display_dev, 80); // 设置亮度为80%
    lisa_display_set_orientation(display_dev, DISPLAY_ORIENTATION_ROTATED_90); // 设置为90度旋转
    // 再次调用 lisa_display_write() 来显示旋转后的内容
    ```

## 6. 关键数据结构总结

*   **`lisa_display.h`**:
    *   `struct display_device`: 代表显示设备实例。
    *   `struct display_driver_api`: 具体驱动需实现的API函数表。
    *   `struct display_capabilities`: 描述显示能力（分辨率、像素格式等）。
    *   `struct display_buffer_descriptor`: 描述帧缓冲区布局。
    *   `enum display_pixel_format`: 定义像素格式。
    *   `enum display_orientation`: 定义显示方向。
*   **`display_common.h`**:
    *   `struct display_obj`: 具体驱动用于存储内部状态（初始化标志、方向、同步对象）。
    *   `struct disp_init_cmd`: 初始化命令项。
    *   `struct disp_mem_area_input`/`output`: 显示区域计算参数。
    *   `disp_mem_coord`: 4字节数组，用于CASET/RASET命令参数。
*   **`display_trans_ctx.h`**:
    *   `struct display_trans_ops`: 物理传输驱动需实现的API函数表。
    *   `enum display_trans_orient`: 传输层内部使用的方向枚举。
    *   `transaction_cb_t`: 回调函数类型（当前版本代码中未显著使用）。
*   **`display_trans_ctx.c` (内部)**:
    *   `struct display_trans_ctx`: 存储传输上下文的内部状态（像素位数、命令位数、方向、底层操作ops指针、旋转缓冲区等）。

## 7. 移植新的显示驱动指南

本指南将详细介绍如何在 `components/display` 模块中移植和集成一个新的显示设备驱动。我们将以 NV3030B 显示驱动的集成过程为例进行说明。

### 7.1. 引言

移植新显示驱动的主要目标是使新的显示硬件能够在现有系统框架下正常工作，并向上层应用提供统一的 `LISA Display API`。基本思路是实现特定芯片的控制逻辑，并将其无缝对接到模块的Kconfig配置系统、CMake构建系统以及 `lisa_display.c` 的驱动分发机制中。

### 7.2. 准备工作

在开始移植之前，请确保：

1.  **熟悉 `components/display/` 目录结构**: 理解各子目录和核心文件的作用，例如：
    *   `lisa_display.c/h`: 顶层API接口。
    *   `display_common.c/h`: 通用功能，如复位、背光、TE同步、区域计算。
    *   `trans_ctx/`: 传输上下文层，处理数据旋转和抽象物理传输。
    *   `Kconfig*`: Kconfig配置文件。
    *   `CMakeLists.txt`: 构建脚本。
2.  **理解核心数据结构和API**: 特别是 `struct display_device`, `struct display_driver_api` (定义于 `lisa_display.h`)，以及 `struct display_trans_ops` (定义于 `trans_ctx/display_trans_ctx.h`)。
3.  **参考现有驱动**: 研究已有的显示驱动代码（如 `display_st7789p3.c`，`display_nv3030b.c`）是如何实现的，这将为新驱动的开发提供重要参考。
4.  **获取新硬件的数据手册**: 详细阅读显示控制器的数据手册，了解其初始化序列、命令集、寄存器定义以及电气特性。

### 7.3. 驱动代码实现 (`display_<chip_name>.c/h`)

为新的显示驱动创建源文件 `display_<chip_name>.c` 和可选的头文件 `display_<chip_name>.h` (如果需要导出特定宏或结构)。以下是主要步骤：

#### 7.3.1. 定义全局驱动对象和内部状态

在驱动的 `.c` 文件中，通常需要定义一个静态的 `struct display_obj` 实例来存储驱动的内部状态：

```c
// In display_nv3030b.c
static struct display_obj g_display_obj;
```
`struct display_obj` (定义于 `display_common.h`) 通常包含初始化标志、当前方向、以及用于同步的互斥锁和信号量等。

#### 7.3.2. 实现芯片初始化命令序列

根据芯片数据手册，定义初始化所需的命令序列。这通常是一个 `struct disp_init_cmd` 数组：

```c
// Example from display_nv3030b.c
static const struct disp_init_cmd init_items[] = {
    {0xFE, NULL, 0, 0}, // Inter Register Enable1
    {0xEF, NULL, 0, 0}, // Inter Register Enable2
    {0xB3, (const uint8_t[]) {0x11}, 1, 0}, // Set Gate output VGH/VGL
    // ...更多初始化命令...
    {DISPLAY_COMM_CMD_SLPOUT, NULL, 0, 120}, // Sleep Out & Delay
    {DISPLAY_COMM_CMD_DISPON, NULL, 0, 120}, // Display On & Delay
};

static int _nv3030b_panel_init(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(init_items); i++) {
        if (display_trans_cmd_data(init_items[i].cmd, init_items[i].data, init_items[i].data_bytes) != 0) {
            LOGE("Failed to send init cmd 0x%02X", init_items[i].cmd);
            return -1;
        }
        if (init_items[i].delay > 0) {
            SysTick_Delay_Ms(init_items[i].delay);
        }
    }
    return 0;
}
```

#### 7.3.3. 实现 `display_driver_api` 接口函数

核心工作是实现 `struct display_driver_api` 中定义的各个函数。以下是一些关键函数的实现要点：

*   **`_init()` 函数 (例如 `nv3030b_display_init`)**: 此函数由 `lisa_display_create()` 调用。
    1.  **创建同步对象**: `g_display_obj.mutex = xSemaphoreCreateMutex();` (如果使用FreeRTOS)。
    2.  **初始化 `display_common` 组件**: 
        *   `disp_comm_rst_init()` (复位引脚)。
        *   `disp_comm_pwm_init()` (背光PWM)。
        *   如果使用TE同步 (`CONFIG_LISA_DISPLAY_TE_SYNC`)，则 `disp_comm_te_init()`。
    3.  **初始化 `display_trans_ctx`**: `display_trans_ctx_init(16, 8, 0);` (参数：像素位数，命令位数，D/C命令电平)。
    4.  **硬件复位**: 使用 `disp_comm_rst_clr()`, `SysTick_Delay_Ms()`, `disp_comm_rst_set()` 执行硬件复位序列。确保延时符合芯片手册要求。
    5.  **执行面板初始化序列**: 调用前面定义的面板初始化函数，如 `_nv3030b_panel_init()`。
    6.  **设置默认方向**: `g_display_obj.orientation = DISPLAY_ORIENTATION_NORMAL;`
    7.  **标记初始化完成**: `g_display_obj.initialized = true;`

*   **`_write()` 函数 (例如 `nv3030b_display_write`)**: 此函数由 `lisa_display_write()` 调用，用于将像素数据写入屏幕。
    1.  **获取互斥锁**: `xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);`
    2.  **TE同步 (可选)**: 如果启用 `CONFIG_LISA_DISPLAY_TE_SYNC`，调用 `disp_comm_te_wait()`。
    3.  **Cache维护 (如果启用DCACHE)**: `dcache_clean_range((uint32_t)buf, (uint32_t)buf + desc->height * desc->pitch);`
    4.  **计算内存区域**: 
        ```c
        struct disp_mem_area_input area_input = {
            .panel_w = CONFIG_DISPLAY_NV3030B_WIDTH, // 从Kconfig获取
            .panel_h = CONFIG_DISPLAY_NV3030B_HEIGHT, // 从Kconfig获取
            .x_offset = CONFIG_DISPLAY_NV3030B_X_OFFSET, // 从Kconfig获取
            .y_offset = CONFIG_DISPLAY_NV3030B_Y_OFFSET, // 从Kconfig获取
            .x = x, .y = y, .w = desc->width, .h = desc->height,
            .orient = g_display_obj.orientation,
        };
        disp_mem_coord x_area, y_area;
        disp_comm_set_mem_area(&area_input, x_area, y_area); // 来自 display_common.c
        ```
    5.  **发送列地址设置 (CASET) 和页地址设置 (RASET) 命令**: 
        `display_trans_cmd_data(DISPLAY_COMM_CMD_CASET, x_area, 4);`
        `display_trans_cmd_data(DISPLAY_COMM_CMD_RASET, y_area, 4);`
    6.  **调用 `display_trans_image()` 发送写RAM命令和像素数据**: 
        `enum display_trans_orient trans_orient = display_orient_to_trans_orient(g_display_obj.orientation);`
        `display_trans_image(DISPLAY_COMM_CMD_RAMWR, (void *)buf, desc->width, desc->height, trans_orient);`
    7.  **释放互斥锁**: `xSemaphoreGive(g_display_obj.mutex);`

*   **`_set_orientation()` 函数 (例如 `nv3030b_display_set_orientation`)**: 
    1.  **获取互斥锁**。
    2.  **更新内部存储的方向**: `g_display_obj.orientation = orientation;`
    3.  **发送控制器特定的命令来改变扫描方向**: 例如，对于ST7789或NV3030B，通常是修改内存访问控制 (MADCTL, 命令0x36) 寄存器。需要根据目标 `orientation` 计算出正确的MADCTL值。
        ```c
        // Example for MADCTL calculation
        uint8_t madctl_val = 0;
        switch (orientation) {
            case DISPLAY_ORIENTATION_NORMAL: madctl_val = 0x00; break;
            case DISPLAY_ORIENTATION_ROTATED_90: madctl_val = 0x60; break; // MX, MV
            // ... other cases ...
        }
        display_trans_cmd_data(DISPLAY_COMM_CMD_MADCTL, &madctl_val, 1);
        ```
    4.  **释放互斥锁**。

*   **其他API函数**: 如 `_blanking_on/off`, `_get_capabilities`, `_sleep` 等，根据芯片特性和需求实现。

#### 7.3.4. 定义驱动API实例

在驱动的 `.c` 文件中，定义一个 `const struct display_driver_api` 实例，并将其函数指针指向上面实现的函数：

```c
// In display_nv3030b.c
static const struct display_driver_api nv3030b_driver_api = {
    .display_init = nv3030b_display_init,
    .display_blanking_on = nv3030b_display_blanking_on,
    .display_blanking_off = nv3030b_display_blanking_off,
    .display_write = nv3030b_display_write,
    .display_get_capabilities = nv3030b_display_get_capabilities,
    .display_set_orientation = nv3030b_display_set_orientation,
    .display_sleep = nv3030b_display_sleep,
};
```

### 7.4. Kconfig 集成

为了让构建系统能够识别和配置新驱动，需要进行Kconfig集成。

#### 7.4.1. 创建驱动专属 Kconfig 文件 (`components/display/Kconfig.<chip_name>`)

例如，为NV3030B创建 `components/display/Kconfig.nv3030b`：

```kconfig
# components/display/Kconfig.nv3030b
config LISA_DISPLAY_NV3030B
    bool "NV3030B display driver"
    select USE_TRANS_CTX if LISA_DISPLAY_NV3030B
    # select other dependencies if needed, e.g., select LISA_SPI_1 if using SPI1
    help
      Enable NV3030B display driver with 170x320 resolution.

if LISA_DISPLAY_NV3030B

config DISPLAY_NV3030B_WIDTH
    int "Display width for NV3030B"
    default 170

config DISPLAY_NV3030B_HEIGHT
    int "Display height for NV3030B"
    default 320

config DISPLAY_NV3030B_X_OFFSET
    int "Display X offset for NV3030B"
    default 35

config DISPLAY_NV3030B_Y_OFFSET
    int "Display Y offset for NV3030B"
    default 0

config ROTATE_BUF_MAX_HEIGHT
    int "Rotate buffer max height for display driver"
    default DISPLAY_NV3030B_WIDTH # Default to panel width if 90/270 rotation common
    depends on USE_TRANS_CTX
    help
      Maximum height of the line buffer used for 90/270 degree rotation.
      This influences SRAM usage.

endif # LISA_DISPLAY_NV3030B
```
*   `LISA_DISPLAY_NV3030B`: 驱动的使能开关。
*   `select USE_TRANS_CTX`: 依赖传输上下文层。
*   定义驱动相关的配置参数，如屏幕宽高、偏移、旋转缓冲区大小等，并提供合理的 `default` 值。

#### 7.4.2. 更新主 Kconfig 文件 (`components/display/Kconfig`)

1.  **在 `choice LISA_DISPLAY_CONTROLLER` 中添加新驱动选项**:
    ```kconfig
    choice LISA_DISPLAY_CONTROLLER
        prompt "Display Controller"
        default LISA_DISPLAY_NONE
        ...
        config LISA_DISPLAY_NV3030B
            bool "NV3030B Display Controller"
        ...
    endchoice
    ```
2.  **`source` 新驱动的Kconfig文件**: 确保在 `LISA_DISPLAY_CONTROLLER` choice 外部，但在相关 `if` 条件（如有）内部 `source` 对应的Kconfig文件，以便在选中时其配置项可见。
    ```kconfig
    if LISA_DISPLAY_CONTROLLER != "LISA_DISPLAY_NONE"
        source "components/display/Kconfig.st7789p3"
        source "components/display/Kconfig.gc9309na"
        source "components/display/Kconfig.axs15231b"
        source "components/display/Kconfig.nv3030b"
        # ... add other drivers here ...
    endif
    ```
    (注意：实际 `components/display/Kconfig` 的组织方式可能略有不同，需根据其现有结构调整，但核心是让新的Kconfig文件被包含进来，并且新的驱动选项出现在选择列表中。)

### 7.5. CMake 构建集成 (`components/display/CMakeLists.txt`)

修改 `components/display/CMakeLists.txt` 文件，根据Kconfig选项条件编译新驱动的源文件：

```cmake
# In components/display/CMakeLists.txt
if(CONFIG_LISA_DISPLAY_NV3030B)
    list(APPEND LISA_DISPLAY_SRCS
        ${CMAKE_CURRENT_LIST_DIR}/display_nv3030b.c
    )
endif()
```
确保 `LISA_DISPLAY_SRCS` 变量包含了所有被使能驱动的源文件，并被正确添加到库或可执行文件的目标源列表中。

### 7.6. 上层模块集成 (`components/display/lisa_display.c`)

在 `lisa_display.c` 的 `lisa_display_create()` 函数中，需要添加一个分支来处理新驱动的实例化。

1.  **在文件顶部 `extern` 声明新驱动的 `display_device` 实例**:
    ```c
    // In lisa_display.c, near other extern declarations
    #if defined(CONFIG_LISA_DISPLAY_NV3030B)
    extern const struct display_device display_nv3030b;
    #endif
    ```
2.  **在 `lisa_display_create()` 函数中添加 `else if` 分支**:
    ```c
    void *lisa_display_create(void)
    {
        const struct display_device *dev = NULL;
        // ... other driver checks ...
    #if defined(CONFIG_LISA_DISPLAY_NV3030B)
        else if (CONFIG_LISA_DISPLAY_NV3030B) { // Check specific Kconfig
            dev = &display_nv3030b;
        }
    #endif
        // ... rest of the function ...
        if (dev && dev->device_init) {
            if (dev->device_init() != 0) {
                LOGE("Failed to init %s", dev->name);
                return NULL;
            }
        }
        return (void *)dev;
    }
    ```
3.  **在新驱动的 `.c` 文件中定义并初始化 `display_device` 实例**: 
    ```c
    // In display_nv3030b.c
    const struct display_device display_nv3030b = {
        .name = "nv3030b",
        .device_init = nv3030b_display_init, // 指向驱动的初始化函数
        .api = &nv3030b_driver_api,      // 指向驱动的API实现表
    };
    ```

### 7.7. 项目级配置 (`prj.conf`)

当需要在某个具体项目或示例中使用新驱动时，修改其 `prj.conf` 文件：

```conf
# Sample prj.conf entries for NV3030B
CONFIG_LISA_DISPLAY=y
CONFIG_LISA_DISPLAY_NV3030B=y  # 使能NV3030B驱动

# 配置传输接口 (根据硬件连接选择)
# CONFIG_LISA_DISPLAY_INTERFACE_QSPI is not set
CONFIG_LISA_DISPLAY_INTERFACE_SPI_4_LINE=y # 假设使用4线SPI
# CONFIG_LISA_DISPLAY_INTERFACE_SPI_3_LINE is not set
# CONFIG_LISA_DISPLAY_INTERFACE_RGB is not set

# 其他可选配置
CONFIG_LISA_DISPLAY_TE_SYNC=n
# CONFIG_LISA_DISPLAY_CPDMA_ROTATE is not set
```
确保所选的 `CONFIG_LISA_DISPLAY_INTERFACE_*` 与硬件连接和驱动能力匹配。有些驱动可能只支持特定接口。

### 7.8. 调试与验证

1.  **编译检查**: 完整编译项目，解决所有编译错误和链接错误。
2.  **Kconfig配置确认**: 构建后，检查生成的 `build/.config` 文件（如果可访问），确认新驱动相关的Kconfig选项是否按预期被设置。
3.  **硬件测试**: 将固件烧录到目标硬件。
    *   观察初始化序列是否正确执行（可以通过日志或示波器观察复位、SPI信号等）。
    *   测试基本显示功能，如清屏、绘制色块、显示图像。
    *   测试屏幕旋转、亮度调节等功能。
4.  **日志分析**: 仔细查看系统日志，特别是与Display模块相关的错误或警告信息。
5.  **常见问题**: 
    *   **依赖缺失**: 忘记在驱动的Kconfig中 `select` 必要的依赖 (如 `USE_TRANS_CTX` 或特定SPI总线)。
    *   **接口配置错误**: `prj.conf` 中选择的显示接口与硬件连接或驱动实现不符。
    *   **初始化序列错误**: 命令或延时与芯片手册不符。
    *   **MADCTL值错误**: 屏幕方向不正确，或图像镜像/翻转。
    *   **传输参数错误**: `display_trans_ctx_init` 中的像素位数或命令位数配置错误。

### 7.9. 总结与最佳实践

*   **代码风格一致性**: 尽量与现有驱动代码风格保持一致。
*   **Kconfig依赖管理**: 正确使用 `select` 和 `depends on` 来管理Kconfig依赖。
*   **参数化配置**: 将关键参数（如屏幕尺寸、偏移、特定时序）通过Kconfig暴露出来，而不是硬编码。
*   **清晰的日志**: 在关键步骤和错误路径添加清晰的日志输出，便于调试。
*   **充分测试**: 测试所有支持的显示方向和功能。

通过遵循以上步骤，您可以将新的显示驱动成功集成到 `components/display` 模块中。
