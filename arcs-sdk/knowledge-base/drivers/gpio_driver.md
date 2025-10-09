# ARCS SDK GPIO 驱动程序知识库

本文档旨在提供 ARCS SDK 中 GPIO (General Purpose Input/Output) 驱动程序的全面概览，以便于 AI 理解和应用。

## 1. 概述 (Overview)

GPIO 驱动程序提供了一套标准的 API 接口，用于控制芯片上的通用输入输出引脚。通过这些接口，可以将引脚配置为输入或输出，读取引脚电平，设置引脚电平，以及配置和处理引脚中断。

主要特性包括：
- 支持多个 GPIO 端口 (例如 GPIOA, GPIOB)。
- 可配置的引脚方向 (输入/输出)。
- 可配置的引脚上下拉模式。
- 可配置的引脚中断触发方式 (上升沿、下降沿、双边沿、高电平、低电平)。
- 支持引脚去抖动功能。
- 支持为每个引脚或整个端口注册中断回调函数。

## 2. 文件结构 (File Structure)

### 2.1. 驱动核心文件

-   **`arcs-base/hal/chip/arcs/driver/gpio/gpio.h`**: GPIO 驱动的私有头文件，定义了特定于此芯片实现的 GPIO 结构体和常量。
-   **`arcs-base/hal/chip/arcs/driver/gpio/gpio.c`**: GPIO 驱动的源文件，实现了 `Driver_GPIO.h` 中定义的 API 接口。
-   **`arcs-base/hal/chip/arcs/include/Driver_GPIO.h`**: 通用的 GPIO 驱动接口定义，定义了标准的 API 函数原型、宏和回调类型。它包含了 `Driver_Common.h`。
-   **`arcs-base/hal/chip/arcs/include/Driver_Common.h`**: 通用驱动定义，可能包含驱动版本、电源状态等通用结构和宏。

### 2.2. 示例代码路径

-   **`samples/drivers/gpio/output/`**: GPIO 输出功能示例。
-   **`samples/drivers/gpio/input/`**: GPIO 输入功能示例。
-   **`samples/drivers/gpio/input_interrupt/`**: GPIO 输入中断功能示例。

## 3. 核心数据结构 (Core Data Structures)

### 3.1. `GPIO_RESOURCES` (定义于 `gpio.h`)

此结构体封装了一个 GPIO 端口所需的全部资源。

```c
typedef struct {
    GPIO_RegDef* reg;             // GPIO 寄存器基地址指针
    uint32_t irq_num;             // GPIO 端口的 IRQ 号码
    void (*irq_handler)(void);   // GPIO 端口的 IRQ 处理函数指针
    uint32_t max_num;             // 该端口支持的最大引脚数
    _GPIO_INFO *info;             // 指向 _GPIO_INFO 结构体的指针
} const GPIO_RESOURCES;
```

在 `gpio.c` 中，为 GPIOA 和 GPIOB 分别定义了 `gpioa_resources` 和 `gpiob_resources` 实例。

### 3.2. `_GPIO_INFO` (定义于 `gpio.h`)

存储与 GPIO 端口相关的回调和引脚状态信息数组。

```c
typedef struct {
    CSK_GPIO_SignalEvent_t cb_event;  // 整个端口的事件回调函数
    void* workspace;                  // 传递给端口回调函数的用户数据
    _GPIO_ *gpio_info;                // 指向存储各个引脚状态的 _GPIO_ 数组
} _GPIO_INFO;
```

### 3.3. `_GPIO_` (定义于 `Driver_GPIO.h`)

存储单个 GPIO 引脚的状态和配置信息。

```c
typedef struct {
    _DIR_ dir;                        // 引脚方向 (csk_gpio_dir_input, csk_gpio_dir_output)
    _MODE_ mode;                      // 引脚上下拉模式 (csk_gpio_mode_none_pull, csk_gpio_mode_pull_up, csk_gpio_mode_pull_down)
    _INT_MODE_ int_mode;              // 引脚中断模式 (_csk_gpio_int_mode_none_, csk_gpio_int_mode_high_level, etc.)
    CSK_GPIO_SignalEvent_t cb;      // 单个引脚的中断回调函数
    void* usr;                        // 传递给单个引脚回调函数的用户数据
} _GPIO_;
```

## 4. 关键宏定义 (Key Macros - 定义于 `Driver_GPIO.h`)

### 4.1. 引脚方向 (`CSK_GPIO_DIR_*`)
-   `CSK_GPIO_DIR_INPUT`: 输入模式 (0x0)
-   `CSK_GPIO_DIR_OUTPUT`: 输出模式 (0x1)

### 4.2. 引脚号 (`CSK_GPIO_PIN*`)
-   `CSK_GPIO_PIN0` 到 `CSK_GPIO_PIN31`: 位掩码，用于指定操作的引脚，例如 `(1UL << 0)`。

### 4.3. 控制参数 (用于 `GPIO_Control` 函数的 `control` 参数)
这些宏通过位移和掩码定义，可以组合使用。

-   **中断使能/禁止 (`CSK_GPIO_INTR_*`)**
    -   `CSK_GPIO_INTR_ENABLE`: 使能中断
    -   `CSK_GPIO_INTR_DISABLE`: 禁止中断
-   **中断模式设置 (`CSK_GPIO_SET_INTR_*`)**
    -   `CSK_GPIO_SET_INTR_LOW_LEVEL`: 低电平触发
    -   `CSK_GPIO_SET_INTR_HIGH_LEVEL`: 高电平触发
    -   `CSK_GPIO_SET_INTR_NEGATIVE_EDGE`: 下降沿触发
    -   `CSK_GPIO_SET_INTR_POSITIVE_EDGE`: 上升沿触发
    -   `CSK_GPIO_SET_INTR_DUAL_EDGE`: 双边沿触发
-   **上下拉模式 (`CSK_GPIO_MODE_*`)**
    -   `CSK_GPIO_MODE_PULL_UP`: 上拉
    -   `CSK_GPIO_MODE_PULL_DOWN`: 下拉
    -   `CSK_GPIO_MODE_PULL_NONE`: 无上下拉
-   **去抖动时钟源 (`CSK_GPIO_DEBOUNCE_CLK_*`)**
    -   `CSK_GPIO_DEBOUNCE_CLK_EXT`: 外部时钟
    -   `CSK_GPIO_DEBOUNCE_CLK_PCLK`: PCLK
-   **去抖动使能/禁止 (`CSK_GPIO_DEBOUNCE_*`)**
    -   `CSK_GPIO_DEBOUNCE_ENABLE`: 使能去抖动
    -   `CSK_GPIO_DEBOUNCE_DISABLE`: 禁止去抖动
-   **其他控制 (`CSK_GPIO_CONTROL_*`)**
    -   `CSK_GPIO_DEBOUNCE_SCALE`: 设置去抖动预分频值 (通过 `arg` 参数传递)

## 5. API 函数详解 (API Functions)

所有 API 函数的第一个参数 `void* res` 都是指向特定 GPIO 端口资源 (例如 `gpioa_resources` 或 `gpiob_resources`) 的指针。

### 5.1. `void* GPIOA(void)` / `void* GPIOB(void)`
-   **功能**: 获取 GPIOA 或 GPIOB 端口的资源句柄 (即指向 `gpioa_resources` 或 `gpiob_resources` 的指针)。
-   **参数**: 无。
-   **返回**: `void*` 类型的端口资源句柄。

### 5.2. `int32_t GPIO_Initialize(void *res, CSK_GPIO_SignalEvent_t cb_event, void* workspace)`
-   **功能**: 初始化指定的 GPIO 端口。包括使能时钟、注册中断处理函数 (如果 `cb_event` 非 NULL)、使能中断。
-   **参数**:
    -   `res`: GPIO 端口资源句柄。
    -   `cb_event`: (可选) 整个端口的中断回调函数。如果为 NULL，则不注册端口级回调。
    -   `workspace`: (可选) 传递给 `cb_event` 的用户数据。
-   **返回**: `CSK_DRIVER_OK` (成功) 或错误码。

### 5.3. `int32_t GPIO_Uninitialize(void *res)`
-   **功能**: 反初始化指定的 GPIO 端口。包括关闭时钟、注销中断处理函数。
-   **参数**: `res`: GPIO 端口资源句柄。
-   **返回**: `CSK_DRIVER_OK` (成功) 或错误码。

### 5.4. `CSK_DRIVER_VERSION GPIO_GetVersion(void)`
-   **功能**: 获取 GPIO 驱动的版本信息。
-   **参数**: 无。
-   **返回**: `CSK_DRIVER_VERSION` 结构体，包含 API 版本和驱动版本。

### 5.5. `int32_t GPIO_Control(void* res, uint32_t control, uint32_t arg)`
-   **功能**: 配置 GPIO 引脚的各种参数，如中断模式、上下拉、去抖动等。
-   **参数**:
    -   `res`: GPIO 端口资源句柄。
    -   `control`: 控制命令的组合 (使用上述关键宏定义)。
    -   `arg`: 控制命令的参数。对于引脚相关的操作，通常是引脚的位掩码 (例如 `CSK_GPIO_PIN0`)。对于 `CSK_GPIO_DEBOUNCE_SCALE`，`arg` 是预分频值。
-   **返回**: `CSK_DRIVER_OK` (成功) 或错误码。

### 5.6. `int32_t GPIO_PinWrite(void* res, uint32_t pin_mask, uint32_t val)`
-   **功能**: 向指定的 GPIO 输出引脚写入电平。
-   **参数**:
    -   `res`: GPIO 端口资源句柄。
    -   `pin_mask`: 要操作的引脚位掩码 (例如 `CSK_GPIO_PIN5`)。
    -   `val`: 要写入的电平值 (0 或 1)。
-   **返回**: `CSK_DRIVER_OK` (成功) 或错误码。

### 5.7. `int32_t GPIO_PinRead(void* res, uint32_t pin_mask)`
-   **功能**: 读取指定的 GPIO 输入引脚的电平。
-   **参数**:
    -   `res`: GPIO 端口资源句柄。
    -   `pin_mask`: 要读取的引脚位掩码 (例如 `CSK_GPIO_PIN3`)。注意：此函数设计为一次读取一个引脚，如果 `pin_mask` 包含多个位，行为可能未定义或只返回其中一个引脚的状态。
-   **返回**: 0 或 1 (读取到的电平值)，或错误码 (如果 `pin_mask` 无效或驱动内部错误)。

### 5.8. `int32_t GPIO_SetDir(void* res, uint32_t pin_mask, uint32_t dir)`
-   **功能**: 设置指定 GPIO 引脚的输入输出方向。
-   **参数**:
    -   `res`: GPIO 端口资源句柄。
    -   `pin_mask`: 要操作的引脚位掩码。
    -   `dir`: 方向 (`CSK_GPIO_DIR_INPUT` 或 `CSK_GPIO_DIR_OUTPUT`)。
-   **返回**: `CSK_DRIVER_OK` (成功) 或错误码。

### 5.9. `int32_t GPIO_Status(void* res, _GPIO_** status, uint32_t* size)`
-   **功能**: 获取指定 GPIO 端口所有引脚的状态信息。
-   **参数**:
    -   `res`: GPIO 端口资源句柄。
    -   `status`: (输出参数) 指向 `_GPIO_` 结构体数组的指针的指针。
    -   `size`: (输出参数) `_GPIO_` 结构体数组的大小 (即引脚数量)。
-   **返回**: `CSK_DRIVER_OK` (成功) 或错误码。

### 5.10. `int32_t GPIO_SetCallback(void* res, uint32_t pin_mask, CSK_GPIO_SignalEvent_t cb_event, void* usr)`
-   **功能**: 为单个 GPIO 引脚设置中断回调函数。
-   **参数**:
    -   `res`: GPIO 端口资源句柄。
    -   `pin_mask`: 要设置回调的引脚位掩码 (必须是单个引脚)。
    -   `cb_event`: 该引脚的中断回调函数。
    -   `usr`: 传递给 `cb_event` 的用户数据。
-   **返回**: `CSK_DRIVER_OK` (成功) 或错误码 (例如 `pin_mask` 不是单个引脚)。

## 6. 中断处理 (Interrupt Handling)

GPIO 中断可以通过两种方式处理：
1.  **端口级回调**: 在 `GPIO_Initialize` 时注册一个回调函数，当中断发生时，该回调函数被调用，参数 `event` 是一个位掩码，指示哪些引脚触发了中断。
2.  **引脚级回调**: 使用 `GPIO_SetCallback` 为特定引脚注册回调函数。当该引脚中断发生时，其注册的回调被调用。

在 `gpio.c` 中的 `GPIO_IRQ_Handler` (实际由 `GPIOA_IRQ_Handler` 或 `GPIOB_IRQ_Handler` 调用) 负责处理中断：
-   读取中断状态寄存器。
-   如果注册了端口级回调 (`gpio->info->cb_event`)，则调用它。
-   遍历所有引脚，如果某个引脚触发了中断并且注册了引脚级回调 (`gpio->info->gpio_info[i].cb`)，则调用它。
-   注意清除中断标志位的操作。

回调函数类型为 `CSK_GPIO_SignalEvent_t`:
```c
typedef void (*CSK_GPIO_SignalEvent_t) (uint32_t event, void* workspace);
```
-   `event`: 触发中断的引脚位掩码。
-   `workspace`: 用户传入的数据指针。

## 7. 使用流程 (Usage Workflow)

典型的 GPIO 使用流程如下：

1.  **引脚复用配置 (Pin Multiplexing)**:
    -   使用 `IOMuxManager_PinConfigure()` 函数将物理引脚配置为 GPIO 功能。例如：
        `IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_DEFAULT);`
    -   这一步至关重要，确保引脚被正确路由到 GPIO 控制器。

2.  **获取 GPIO 端口句柄**:
    -   `void* gpio_handler = GPIOA();` (或 `GPIOB()`)

3.  **初始化 GPIO 端口**:
    -   `GPIO_Initialize(gpio_handler, port_callback, user_data);`
    -   `port_callback` 和 `user_data` 是可选的，用于端口级中断。

4.  **配置引脚参数**:
    -   **设置方向**: `GPIO_SetDir(gpio_handler, CSK_GPIO_PINx, CSK_GPIO_DIR_OUTPUT/CSK_GPIO_DIR_INPUT);`
    -   **(可选) 设置上下拉**: `GPIO_Control(gpio_handler, CSK_GPIO_MODE_PULL_UP, CSK_GPIO_PINx);`
    -   **(可选) 配置中断 (输入引脚)**:
        -   设置中断触发类型: `GPIO_Control(gpio_handler, CSK_GPIO_SET_INTR_NEGATIVE_EDGE, CSK_GPIO_PINx);`
        -   使能中断: `GPIO_Control(gpio_handler, CSK_GPIO_INTR_ENABLE, CSK_GPIO_PINx);`
        -   (可选) 设置引脚级回调: `GPIO_SetCallback(gpio_handler, CSK_GPIO_PINx, pin_callback, pin_user_data);`
    -   **(可选) 配置去抖动**: `GPIO_Control(gpio_handler, CSK_GPIO_DEBOUNCE_ENABLE | CSK_GPIO_DEBOUNCE_CLK_PCLK, CSK_GPIO_PINx);`
        -   (可选) 设置去抖动分频: `GPIO_Control(gpio_handler, CSK_GPIO_DEBOUNCE_SCALE, prescaler_value);` (注意：`DEBOUNCE_SCALE` 的 `arg` 不是引脚掩码)

5.  **操作引脚**:
    -   **输出**: `GPIO_PinWrite(gpio_handler, CSK_GPIO_PINx, 1/0);`
    -   **输入**: `uint32_t value = GPIO_PinRead(gpio_handler, CSK_GPIO_PINx);`

6.  **反初始化 GPIO 端口** (当不再使用时):
    -   `GPIO_Uninitialize(gpio_handler);`

## 8. 示例代码分析 (Sample Code Analysis)

### 8.1. `samples/drivers/gpio/output/src/main.c`
-   演示了如何将一个 GPIO 引脚 (PA20) 配置为输出模式。
-   通过 `IOMuxManager_PinConfigure` 配置引脚复用。
-   调用 `GPIO_Initialize` 初始化 GPIOA。
-   调用 `GPIO_SetDir` 设置 PA20 为输出。
-   在循环中调用 `GPIO_PinWrite` 使 PA20 输出高低电平，实现闪烁效果。

### 8.2. `samples/drivers/gpio/input/src/main.c`
-   演示了如何读取一个 GPIO 引脚 (PA20) 的输入状态。
-   PA20 配置为输入，PA21 配置为输出 (用于产生测试信号)。
-   在循环中，先设置 PA21 的输出电平，然后通过 `GPIO_PinRead` 读取 PA20 的电平并打印。

### 8.3. `samples/drivers/gpio/input_interrupt/src/main.c`
-   演示了如何配置 GPIO 引脚 (PA20) 的输入中断 (下降沿触发)。
-   定义了一个端口级回调函数 `GPIOA_EventCallback_Negative`。
-   PA20 配置为输入，PA21 配置为输出。
-   通过 `GPIO_Initialize` 注册回调。
-   通过 `GPIO_Control` 配置 PA20 为下降沿触发并使能中断。
-   通过改变 PA21 的电平来触发 PA20 的中断，并在回调函数中处理事件。

## 9. 注意事项

-   **引脚复用 (IOMux)**: 在使用任何 GPIO 引脚之前，必须确保通过 `IOMuxManager` 正确配置了其复用功能为 GPIO。
-   **资源句柄**: `GPIOA()` 和 `GPIOB()` 返回的句柄是区分不同 GPIO 端口的关键。
-   **中断回调**: 可以选择端口级回调或引脚级回调，或者两者都用。驱动内部会先尝试调用端口级回调，然后是匹配的引脚级回调。
-   **错误检查**: API 函数通常返回 `CSK_DRIVER_OK` 表示成功，其他值为错误码，应进行检查。

此文档为 GPIO 驱动提供了一个结构化的视图，希望能帮助 AI 系统更好地理解和使用该驱动。
