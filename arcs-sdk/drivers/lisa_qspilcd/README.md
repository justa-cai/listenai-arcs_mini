# QSPI LCD 驱动

基于 lisa_device 框架的 QSPI LCD 设备驱动，为 ARCS 平台提供统一的 QSPI 接口，用于驱动 LCD 显示屏等外设。

## 功能特性

- **设备支持**: QSPI LCD 控制器（qspilcd0）
- **传输模式**: 支持 PIO（轮询）和 DMA（直接内存访问）两种传输方式
- **数据线模式**: 支持单线（Single）、双线（Dual）和四线（Quad）传输
- **CS 控制**: 支持硬件自动 CS 控制，也支持通过 GPIO 进行手动 CS 控制
- **灵活配置**: 提供 `control` 接口配置总线速度、时钟模式、位序等参数
- **线程安全**: 内部使用互斥锁保护，支持多线程并发访问

## 配置选项

在 `prj.conf` 中启用驱动：

```kconfig
CONFIG_LISA_QSPILCD=y

# 配置 QSPI 使用的 GPDMA 通道号
CONFIG_LISA_QSPILCD_GPDMA_CH=0
```

## API 接口

### 数据传输

```c
int lisa_qspilcd_transfer(lisa_device_t *dev, const lisa_qspilcd_xfer_t *xfer);
int lisa_qspilcd_wait_done(lisa_device_t *dev, uint32_t timeout_ms);
```

### 模式配置

```c
int lisa_qspilcd_set_lane(lisa_device_t *dev, lisa_qspilcd_lane_num_t lane);
int lisa_qspilcd_set_data_bits(lisa_device_t *dev, uint8_t data_bits);
```

### CS 控制

```c
int lisa_qspilcd_cs_configure(lisa_device_t *dev, lisa_device_t *gpio_dev, uint32_t cs_pin);
void lisa_qspilcd_cs_control(lisa_device_t *dev, bool level);
```

### 高级控制

```c
int lisa_qspilcd_control(lisa_device_t *dev, uint32_t control, uint32_t arg);
```

#### 控制码组合说明

`control` 参数是一个 32 位控制码，由多个字段按位或组合而成：

```c
uint32_t control = LISA_QSPILCD_TXIO_PIO |      /* TX I/O 模式 */
                   LISA_QSPILCD_CPOL0_CPHA0 |   /* 时钟极性和相位 */
                   LISA_QSPILCD_MSB_LSB |       /* 位序 */
                   LISA_QSPILCD_MODE_MASTER |   /* 主从模式 */
                   LISA_QSPILCD_DATA_BITS(8);   /* 数据位宽 */

lisa_qspilcd_control(qspi, control, 50*1000*1000);  /* arg 为总线速度 */
```

#### 控制码字段定义

| 字段 | 位域 | 宏定义 | 说明 |
|------|------|--------|------|
| **主从模式** | [3:0] | `LISA_QSPILCD_MODE_MASTER` | 主机模式 |
|              |       | `LISA_QSPILCD_MODE_SLAVE` | 从机模式 |
| **TX I/O 模式** | [5:4] | `LISA_QSPILCD_TXIO_PIO` | CPU 轮询发送 |
|                 |       | `LISA_QSPILCD_TXIO_DMA` | DMA 发送 |
|                 |       | `LISA_QSPILCD_TXIO_AUTO` | 自动选择 |
| **RX I/O 模式** | [7:6] | `LISA_QSPILCD_RXIO_PIO` | CPU 轮询接收 |
|                 |       | `LISA_QSPILCD_RXIO_DMA` | DMA 接收 |
|                 |       | `LISA_QSPILCD_RXIO_AUTO` | 自动选择 |
| **时钟极性/相位** | [11:8] | `LISA_QSPILCD_CPOL0_CPHA0` | Mode 0 |
|                   |        | `LISA_QSPILCD_CPOL0_CPHA1` | Mode 1 |
|                   |        | `LISA_QSPILCD_CPOL1_CPHA0` | Mode 2 |
|                   |        | `LISA_QSPILCD_CPOL1_CPHA1` | Mode 3 |
| **数据位宽** | [17:12] | `LISA_QSPILCD_DATA_BITS(n)` | n = 1~32 |
| **位序** | [19:18] | `LISA_QSPILCD_MSB_LSB` | 高位优先 |
|          |         | `LISA_QSPILCD_LSB_MSB` | 低位优先 |
| **命令阶段** | [20] | `LISA_QSPILCD_CMD_PHASE_ENABLE` | 启用命令阶段 |
|              |      | `LISA_QSPILCD_CMD_PHASE_DISABLE` | 禁用命令阶段 |
| **地址阶段** | [21] | `LISA_QSPILCD_ADDR_PHASE_ENABLE` | 启用地址阶段 |
|              |      | `LISA_QSPILCD_ADDR_PHASE_DISABLE` | 禁用地址阶段 |
| **地址长度** | [23:22] | `LISA_QSPILCD_ADDR_PHASE_1BYTES` | 1 字节地址 |
|              |         | `LISA_QSPILCD_ADDR_PHASE_2BYTES` | 2 字节地址 |
|              |         | `LISA_QSPILCD_ADDR_PHASE_3BYTES` | 3 字节地址 |
|              |         | `LISA_QSPILCD_ADDR_PHASE_4BYTES` | 4 字节地址 |
| **独占操作** | [27:24] | `LISA_QSPILCD_SET_BUS_SPEED` | 设置总线速度 |
|              |         | `LISA_QSPILCD_GET_BUS_SPEED` | 获取总线速度 |
|              |         | `LISA_QSPILCD_ABORT_TRANSFER` | 中止传输 |
|              |         | `LISA_QSPILCD_RESET_FIFO` | 复位 FIFO |
| **DMA 控制** | [29:28] | `LISA_QSPILCD_DMA_ENABLE` | 启用 DMA |
|              |         | `LISA_QSPILCD_DMA_DISABLE` | 禁用 DMA |

#### 参数说明

- **dev**: QSPI LCD 设备句柄
- **control**: 控制码，由上述宏按位或组合
- **arg**: 附加参数，含义取决于控制码：
  - 设置总线速度时为频率值（Hz）
  - 复位 FIFO 时为 FIFO 类型（1=TX, 2=RX, 3=Both）
  - 其他情况通常为 0

#### 返回值

- `LISA_DEVICE_OK`: 操作成功
- `LISA_DEVICE_ERR_INVALID`: 参数无效
- `LISA_DEVICE_ERR_IO`: 底层操作失败

## 使用示例

### 1. 基础 PIO 传输

```c
#include "lisa_qspilcd.h"

lisa_device_t *qspi = lisa_device_get("qspilcd0");
if (!lisa_device_ready(qspi)) {
    return -1;
}

// 准备传输数据
const uint8_t command[] = {0x2A}; // 示例命令
const uint8_t data[] = {0x00, 0x01, 0x02, 0x03}; // 示例数据

lisa_qspilcd_xfer_t xfer_cmd = {
    .buf = command,
    .size_bytes = sizeof(command),
    .lane = LISA_QSPILCD_LANE_SINGLE,
    .data_bits = 8,
    .use_dma = false, // 使用 PIO
};

lisa_qspilcd_xfer_t xfer_data = {
    .buf = data,
    .size_bytes = sizeof(data),
    .lane = LISA_QSPILCD_LANE_QUAD, // 使用四线传输数据
    .data_bits = 8,
    .use_dma = false, // 使用 PIO
};

// 发送命令 (单线)
lisa_qspilcd_transfer(qspi, &xfer_cmd);
lisa_qspilcd_wait_done(qspi, 1000);

// 发送数据 (四线)
lisa_qspilcd_transfer(qspi, &xfer_data);
lisa_qspilcd_wait_done(qspi, 1000);
```

### 2. DMA 传输

```c
#include "lisa_qspilcd.h"

// ... 获取设备 ...

// 大块数据适合 DMA 传输
static uint8_t frame_buffer[1024];

lisa_qspilcd_xfer_t dma_xfer = {
    .buf = frame_buffer,
    .size_bytes = sizeof(frame_buffer),
    .lane = LISA_QSPILCD_LANE_QUAD,
    .data_bits = 8,
    .use_dma = true, // 启用 DMA
};

// 启动 DMA 传输
int ret = lisa_qspilcd_transfer(qspi, &dma_xfer);
if (ret == LISA_DEVICE_OK) {
    // 等待 DMA 完成，信号量机制，低功耗
    ret = lisa_qspilcd_wait_done(qspi, 5000);
    if (ret == LISA_DEVICE_OK) {
        printf("DMA transfer complete\n");
    }
}
```

### 3. 手动 CS 控制

```c
#include "lisa_qspilcd.h"
#include "lisa_gpio.h"

// ... 获取 qspi 设备 ...
lisa_device_t *gpio = lisa_device_get("gpio0");
const uint32_t cs_pin = 20; // 假设 CS 在 PA20

// 1. 关联 GPIO 和 CS 引脚
lisa_qspilcd_cs_configure(qspi, gpio, cs_pin);

// 2. 手动拉低 CS
lisa_qspilcd_cs_control(qspi, false);

// 3. 执行一系列传输
lisa_qspilcd_transfer(qspi, &xfer1);
lisa_qspilcd_wait_done(qspi, 1000);

lisa_qspilcd_transfer(qspi, &xfer2);
lisa_qspilcd_wait_done(qspi, 1000);

// 4. 手动拉高 CS
lisa_qspilcd_cs_control(qspi, true);
```

## 硬件配置

### 引脚复用配置

QSPI 驱动在初始化时会自动调用板型目录中定义的 `lisa_qspilcd_pinmux()` 函数，用于配置 QSPI 的 CLK, CS, IO0-IO3 引脚复用。

- **定义**: `boards/<板型名>/pinmux.c` 中实现 `lisa_qspilcd_pinmux()` 函数。
- **声明**: `boards/<板型名>/pinmux.h` 中声明 `void lisa_qspilcd_pinmux()`。

**示例** (参考 `boards/arcs_evb/pinmux.c`):

```c
void lisa_qspilcd_pinmux()
{
    // 配置 PA18, PA19, PA20, PA21, PA22, PA23 为 QSPI 功能
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 18, 12); // CLK
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 19, 12); // CS
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, 12); // IO0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, 12); // IO1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22, 12); // IO2
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 23, 12); // IO3
}
```

**注意**: 功能码需根据芯片手册确定。

## 参数说明

### 数据线模式

| 模式 | 枚举值 | 说明 |
|---|---|---|
| 单线 | `LISA_QSPILCD_LANE_SINGLE` | 使用 IO0 进行数据传输 |
| 双线 | `LISA_QSPILCD_LANE_DUAL` | 使用 IO0, IO1 进行数据传输 |
| 四线 | `LISA_QSPILCD_LANE_QUAD` | 使用 IO0, IO1, IO2, IO3 进行数据传输 |

### `control` 接口常用控制码

| 控制码 | 参数 `arg` | 说明 |
|---|---|---|
| `LISA_QSPILCD_SET_BUS_SPEED` | `uint32_t` 频率 (Hz) | 设置 QSPI 总线时钟频率 |
| `LISA_QSPILCD_GET_BUS_SPEED` | `uint32_t*` 指针 | 获取当前总线频率 |
| `LISA_QSPILCD_DMA_ENABLE` | `0` | 启用 DMA 模式 |
| `LISA_QSPILCD_DMA_DISABLE` | `0` | 禁用 DMA 模式 (切换到 PIO) |
| `LISA_QSPILCD_CPOL0_CPHA0` ... `CPOL1_CPHA1` | `0` | 设置 SPI 时钟极性与相位 |
| `LISA_QSPILCD_MSB_LSB` / `LSB_MSB` | `0` | 设置位序 |
| `LISA_QSPILCD_RESET_FIFO` | `3` (TX & RX) | 复位硬件 FIFO |

## 返回值说明

| 返回值 | 说明 |
|-------|------|
| `LISA_DEVICE_OK (0)` | 操作成功 |
| `LISA_DEVICE_ERR_INVALID` | 参数无效（如 `xfer` 为空） |
| `LISA_DEVICE_ERR_NOT_READY` | 设备未初始化 |
| `LISA_DEVICE_ERR_IO` | 底层 HAL 操作失败 |
| `LISA_DEVICE_ERR_TIMEOUT` | 等待传输完成超时 |
| `LISA_DEVICE_ERR_NOT_SUPPORT` | 不支持的操作 |

## 注意事项

1. **引脚配置**: 使用 QSPI 前必须通过板型 `pinmux` 配置将对应引脚切换为 QSPI 功能
2. **DMA 通道**: 需通过 Kconfig 正确配置 GPDMA 通道号，确保没有与其他外设冲突
3. **传输同步**: `lisa_qspilcd_transfer()` 是异步接口，必须调用 `lisa_qspilcd_wait_done()` 等待传输完成
4. **CS 管理**: 默认使用 QSPI 控制器内置 CS 引脚；如需手动控制，需先调用 `lisa_qspilcd_cs_configure()` 配置 GPIO
5. **参数切换**: 切换数据线模式或数据位宽应在总线空闲时进行
6. **线程安全**: 驱动 API 内部使用互斥锁保护，可在多线程环境中安全调用
7. **DMA 缓冲区**: 使用 DMA 传输时，缓冲区地址需满足 DMA 对齐要求

## 文件说明

- `lisa_qspilcd.h` - 驱动头文件，包含所有 API 和类型定义
- `lisa_qspilcd_arcs.c` - ARCS 平台适配实现
- `Kconfig` - 配置选项
- `CMakeLists.txt` - 构建配置