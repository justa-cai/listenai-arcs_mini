# UART 驱动

基于 lisa_device 框架的 UART 设备驱动，为 ARCS 平台提供统一的串口通信接口。

## 功能特性

- **设备支持**: UART0、UART1、UART2 三个串口设备
- **传输模式**: 支持中断模式和 DMA 模式
- **DMA 通道管理**: 支持显式指定或自动分配 DMA 通道（共 4 个通道：0-3）
- **循环缓冲**: 支持 Ping-Pong 或多缓冲区接收，适用于不定长数据接收
- **通信配置**: 灵活配置波特率（1200-921600）、数据位、停止位、校验位、流控
- **数据传输**: 同步/异步读写、轮询收发、空闲中断检测

## 配置选项

在 `prj.conf` 中启用驱动：

```kconfig
CONFIG_LISA_UART_DEVICE=y
CONFIG_LISA_UART0=y          # 启用 UART0 设备
CONFIG_LISA_UART1=y          # 启用 UART1 设备
CONFIG_LISA_UART2=y          # 启用 UART2 设备
CONFIG_LISA_UART_ASYNC_API=y # 启用异步 API（可选）
```

根据需要选择启用 UART0/1/2 设备。

## API 接口

### 配置接口

```c
int lisa_uart_configure(lisa_device_t *dev, const lisa_uart_config_t *config);
int lisa_uart_get_config(lisa_device_t *dev, lisa_uart_config_t *config);
```

**重要**: 必须先调用 `lisa_uart_configure()` 配置设备后，才能使用下面的功能 API。设备初始化后不会自动配置。

### 数据传输接口

```c
int lisa_uart_write_sync(lisa_device_t *dev, const uint8_t *buf, uint32_t len, uint32_t timeout_ms);
int lisa_uart_read_sync(lisa_device_t *dev, uint8_t *buf, uint32_t len);
int lisa_uart_rx_enable(lisa_device_t *dev);
int lisa_uart_rx_disable(lisa_device_t *dev);
```

### 轮询接口

```c
int lisa_uart_poll_in(lisa_device_t *dev, uint8_t *byte);   // 非阻塞接收单字节
void lisa_uart_poll_out(lisa_device_t *dev, uint8_t byte);  // 阻塞发送单字节
```

### 异步接口（需启用 CONFIG_LISA_UART_ASYNC_API）

```c
int lisa_uart_write_async(lisa_device_t *dev, const uint8_t *buf, uint32_t len);
int lisa_uart_set_callback(lisa_device_t *dev, lisa_uart_callback_t callback, void *user_data);
int lisa_uart_write_abort(lisa_device_t *dev);
uint32_t lisa_uart_get_tx_count(lisa_device_t *dev);
```

## 硬件配置

### 引脚复用配置

UART 驱动在初始化时会自动调用板型目录中定义的 `lisa_uartX_pinmux()` 函数（X 为 0/1/2），用于配置 UART 设备的引脚复用。

**配置位置**:
- **定义**: `boards/<板型名>/pinmux.c` 中实现 `lisa_uart0_pinmux()`、`lisa_uart1_pinmux()`、`lisa_uart2_pinmux()` 函数
- **声明**: `boards/<板型名>/pinmux.h` 中声明 `void lisa_uartX_pinmux()`
- **调用时机**: 对应 UART 设备初始化时自动调用

**示例** (参考 `boards/arcs_evb/pinmux.c`):
```c
// UART0 引脚复用配置
void lisa_uart0_pinmux()
{
    // 配置 PA02 为 UART0_RX（功能码 2）
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 2, 2);
    // 配置 PA03 为 UART0_TX（功能码 2）
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 3, 2);
}

// UART1 引脚复用配置
void lisa_uart1_pinmux()
{
    // 配置 PA21 为 UART1_TX（功能码 3）
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, 3);
}
```

**注意**:
- 该函数由板型相关代码实现，不同板型的引脚配置可能不同
- 只需配置实际使用的引脚（如仅使用 TX，则只需配置 TX 引脚）
- 如果使用硬件流控（RTS/CTS），还需额外配置流控引脚

## 使用示例

### 基本配置与同步发送

```c
#include "lisa_uart.h"

// 获取 UART0 设备
lisa_device_t *uart0 = lisa_device_get_by_name("uart0");
if (!uart0) {
    return -1;
}

// 配置串口参数
lisa_uart_config_t config = LISA_UART_CONFIG_DEFAULT();  // 115200, 8N1
lisa_uart_configure(uart0, &config);

// 同步发送数据
const uint8_t data[] = "Hello UART\r\n";
int ret = lisa_uart_write_sync(uart0, data, sizeof(data) - 1, 1000);
if (ret > 0) {
    // 成功发送 ret 字节
}
```

### DMA 模式配置（自动分配通道）

```c
// DMA 模式，自动分配 DMA 通道
lisa_uart_config_t config = LISA_UART_CONFIG_DMA();
config.rx_buf_config.buffer_count = 2;    // Ping-Pong 模式
config.rx_buf_config.buffer_size = 512;   // 单个缓冲区大小
lisa_uart_configure(uart0, &config);

// 启用接收
lisa_uart_rx_enable(uart0);

// DMA 模式下的数据收发与中断模式相同
const uint8_t data[] = "DMA Mode Test\r\n";
lisa_uart_write_sync(uart0, data, sizeof(data) - 1, 1000);
```

### DMA 模式配置（显式指定通道）

```c
// 显式指定 DMA 通道（适用于多设备协同工作）
lisa_uart_config_t config = LISA_UART_CONFIG_DMA();
config.dma_tx_channel = 0;                // 指定 TX 使用 DMA 通道 0
config.dma_rx_channel = 1;                // 指定 RX 使用 DMA 通道 1
config.rx_buf_config.buffer_count = 2;
config.rx_buf_config.buffer_size = 512;
lisa_uart_configure(uart0, &config);

// 另一个设备使用不同的通道，避免冲突
lisa_device_t *uart1 = lisa_device_get_by_name("uart1");
lisa_uart_config_t config1 = LISA_UART_CONFIG_DMA();
config1.dma_tx_channel = 2;               // 指定 TX 使用 DMA 通道 2
config1.dma_rx_channel = 3;               // 指定 RX 使用 DMA 通道 3
config1.rx_buf_config.buffer_count = 2;
config1.rx_buf_config.buffer_size = 512;
lisa_uart_configure(uart1, &config1);
```

### 循环缓冲区接收（不定长数据）

```c
// 配置循环接收缓冲区（2个缓冲区，每个256字节）
lisa_uart_config_t config = LISA_UART_CONFIG_DEFAULT();
config.rx_buf_config.buffer_count = 2;    // Ping-Pong 模式
config.rx_buf_config.buffer_size = 256;   // 单个缓冲区大小
lisa_uart_configure(uart0, &config);

// 启用接收
lisa_uart_rx_enable(uart0);

// 读取数据（阻塞直到接收到数据或空闲中断）
uint8_t rx_buf[128];
int len = lisa_uart_read_sync(uart0, rx_buf, sizeof(rx_buf));
if (len > 0) {
    // 成功接收 len 字节（可能因空闲中断提前返回）
} else if (len == LISA_DEVICE_ERR_OVERFLOW) {
    // 缓冲区溢出，需要恢复接收
    lisa_uart_rx_disable(uart0);
    lisa_uart_rx_enable(uart0);
}

// 停止接收
lisa_uart_rx_disable(uart0);
```

### 轮询模式收发

```c
// 轮询发送单字节
lisa_uart_poll_out(uart0, 'A');

// 轮询接收单字节（非阻塞）
uint8_t byte;
if (lisa_uart_poll_in(uart0, &byte) == LISA_DEVICE_OK) {
    // 成功接收到字节
}
```

### 异步发送与事件回调

```c
// 事件回调函数
void uart_event_handler(lisa_uart_event_t event, void *user_data)
{
    if (event & LISA_UART_EVENT_TX_DONE) {
        // 发送完成
    }
    if (event & LISA_UART_EVENT_RX_READY) {
        // 接收就绪
    }
    if (event & LISA_UART_EVENT_RX_TIMEOUT) {
        // 空闲中断（不定长数据接收完成）
    }
}

// 注册回调
lisa_uart_set_callback(uart0, uart_event_handler, NULL);

// 异步发送（不等待完成）
const uint8_t data[] = "Async Data";
lisa_uart_write_async(uart0, data, sizeof(data) - 1);
```

## 配置宏

驱动提供便捷配置宏用于快速初始化：

- `LISA_UART_CONFIG_DEFAULT()` - 115200, 8N1, 无流控, 中断模式
- `LISA_UART_CONFIG_LOW_SPEED()` - 9600, 8N1, 无流控, 中断模式
- `LISA_UART_CONFIG_HIGH_SPEED()` - 921600, 8N1, 无流控, 中断模式
- `LISA_UART_CONFIG_FLOW_CONTROL()` - 115200, 8N1, RTS/CTS 硬件流控, 中断模式
- `LISA_UART_CONFIG_DMA()` - 115200, 8N1, 无流控, DMA 模式, 自动分配 DMA 通道

**注意**: 所有配置宏默认使用 `dma_tx_channel = 0xFF` 和 `dma_rx_channel = 0xFF`（自动分配）。如需显式指定通道，请在配置后修改这两个字段。

## 事件类型

- `LISA_UART_EVENT_TX_DONE` - 发送完成
- `LISA_UART_EVENT_RX_READY` - 接收数据就绪
- `LISA_UART_EVENT_RX_TIMEOUT` - 接收空闲中断（不定长数据接收完成）
- `LISA_UART_EVENT_ERROR` - 错误事件
- `LISA_UART_EVENT_OVERRUN` - 接收溢出
- `LISA_UART_EVENT_PARITY_ERROR` - 校验错误
- `LISA_UART_EVENT_FRAME_ERROR` - 帧错误

**特别注意**: 如果功能 API 返回 `LISA_DEVICE_ERR_NOT_READY`，请检查是否已调用 `lisa_uart_configure()` 配置设备。

## 注意事项

1. **必须先配置**: 设备初始化后没有默认配置，必须先调用 `lisa_uart_configure()` 配置设备，才能使用任何功能 API（如 `write_sync`、`read_sync`、`rx_enable` 等），否则将返回 `LISA_DEVICE_ERR_NOT_READY` 错误
2. **DMA 通道管理**:
   - 系统共有 4 个 DMA 通道（0-3），需合理分配以避免冲突
   - 设置 `dma_tx_channel = 0xFF` 和 `dma_rx_channel = 0xFF` 可自动分配通道
   - 多个 UART 设备同时使用 DMA 模式时，建议显式指定不同的通道
   - 配置 DMA 通道必须在调用 `lisa_uart_configure()` 时指定，不能事后修改
3. **循环缓冲**: 接收不定长数据时，建议配置循环缓冲区并启用空闲中断（`rx_buf_config`）
4. **溢出处理**: 当 `read_sync` 返回 `LISA_DEVICE_ERR_OVERFLOW` 时，需调用 `rx_disable` + `rx_enable` 恢复接收
5. **接收使能**: 使用循环缓冲区接收时，需先调用 `rx_enable` 启动接收，否则 `read_sync` 返回 `LISA_DEVICE_ERR_NOT_READY`
6. **线程安全**: 驱动支持全双工通信，可在不同线程中同时进行收发操作
7. **事件回调**: 在中断上下文执行，应保持简短快速
