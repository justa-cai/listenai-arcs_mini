# SPI 驱动

基于 lisa_device 框架的 SPI 设备驱动，为 ARCS 平台提供统一的SPI通信接口。

## 功能特性

- **设备支持**: SPI0、SPI1、SPI2 三个SPI设备
- **工作模式**: 支持主机模式和从机模式
- **传输模式**: 支持中断模式和 DMA 模式
- **SPI 模式**: 支持模式0-3（CPOL/CPHA组合）
- **位序配置**: 支持 MSB 先行或 LSB 先行
- **片选控制**: 支持硬件片选和软件片选
- **数据传输**: 支持单独发送、接收或全双工收发
- **异步操作**: 支持回调通知机制
- **智能管理**: DMA 通道自动管理，模式切换时自动释放

## 配置选项

在 `prj.conf` 中启用驱动：

```kconfig
CONFIG_LISA_SPI_DEVICE=y
CONFIG_LISA_SPI0=y          # 启用 SPI0 设备
CONFIG_LISA_SPI1=y          # 启用 SPI1 设备
CONFIG_LISA_SPI2=y          # 启用 SPI2 设备
CONFIG_LISA_SPI_ASYNC_API=y # 启用异步 API（可选）
```

根据需要选择启用 SPI0/1/2 设备。

## API 接口

### 配置接口

```c
/* 配置SPI总线，自动处理DMA通道的配置和释放 */
int lisa_spi_configure(lisa_device_t *dev, const lisa_spi_config_t *config);

/* 获取当前SPI配置 */
int lisa_spi_get_config(lisa_device_t *dev, lisa_spi_config_t *config);
```

### 数据传输接口

```c
/* 全双工传输（同时发送和接收） */
int lisa_spi_transfer(lisa_device_t *dev, const lisa_spi_transfer_t *xfer);

/* 只发送数据 */
int lisa_spi_write(lisa_device_t *dev, const uint8_t *buf, uint32_t len);

/* 只接收数据 */
int lisa_spi_read(lisa_device_t *dev, uint8_t *buf, uint32_t len);
```

### 回调接口

```c
/* 注册传输完成回调函数 */
int lisa_spi_register_callback(lisa_device_t *dev, lisa_spi_transfer_callback_t cb, void *user_data);
```

## 硬件配置

### 引脚复用配置

SPI 驱动在初始化时会自动调用版型目录中定义的 `lisa_spiX_pinmux()` 函数（X 为 0/1/2），用于配置 SPI 设备的引脚复用。

**配置位置**:
- **定义**: `boards/<板型名>/pinmux.c` 中实现 `lisa_spi0_pinmux()`、`lisa_spi1_pinmux()`、`lisa_spi2_pinmux()` 函数
- **声明**: `boards/<板型名>/pinmux.h` 中声明 `void lisa_spiX_pinmux()`
- **调用时机**: 对应 SPI 设备初始化时自动调用

**示例** (参考 `boards/arcs_evb/pinmux.c`):
```c
// SPI0 引脚复用配置
__attribute__((weak)) void lisa_spi0_pinmux()
{

}

// SPI1 引脚复用配置
__attribute__((weak)) void lisa_spi1_pinmux()
{

}

// SPI2 引脚复用配置
__attribute__((weak)) void lisa_spi2_pinmux()
{

}
```

## 使用示例

### 基本配置与数据传输（中断模式）

```c
#include "lisa_spi.h"

// 获取 SPI0 设备
lisa_device_t *spi0 = lisa_device_get("spi0");
if (!lisa_device_ready(spi0)) {
    return -1;
}

// 配置SPI参数（中断模式）
lisa_spi_config_t config = LISA_SPI_CONFIG_DEFAULT();  // 默认：模式0，8位，MSB先行，1MHz，中断模式
lisa_spi_configure(spi0, &config);

// 注册回调函数（异步操作）
void spi_callback(void *user_data) {
    // 传输完成处理
}
int ret = lisa_spi_register_callback(spi0, spi_callback, NULL);
if (ret != LISA_DEVICE_OK) {
    // 注册失败处理
}
```

### 全双工数据传输

```c
// 同时发送和接收数据（全双工）
const uint8_t tx_buf[] = {0x01, 0x02, 0x03, 0x04};
uint8_t rx_buf[4] = {0};

lisa_spi_transfer_t transfer = {
    .tx_buf = tx_buf,
    .rx_buf = rx_buf,
    .len = 4,
};

int ret = lisa_spi_transfer(spi0, &transfer);
if (ret == LISA_DEVICE_OK) {
    // 传输成功，检查接收数据
    printf("Received: %02X %02X %02X %02X\n", 
           rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3]);
}
```

### 单独发送数据

```c
// 只发送数据（半双工）
const uint8_t tx_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
int ret = lisa_spi_write(spi0, tx_data, sizeof(tx_data));
if (ret == LISA_DEVICE_OK) {
    // 发送成功
}
```

### 单独接收数据

```c
// 只接收数据（会发送虚拟数据产生时钟）
uint8_t rx_data[8] = {0};
int ret = lisa_spi_read(spi0, rx_data, sizeof(rx_data));
if (ret == LISA_DEVICE_OK) {
    // 接收成功
}
```

### DMA模式配置与使用

```c
// 配置DMA模式（需要预留DMA通道）
lisa_spi_config_t config = LISA_SPI_CONFIG_DEFAULT();
config.tx_transfer_mode = LISA_SPI_DMA_TRANSFER;  // DMA传输模式
config.rx_transfer_mode = LISA_SPI_DMA_TRANSFER;
config.tx_dma_channel = 0;  // 发送DMA通道号
config.rx_dma_channel = 1;  // 接收DMA通道号

// 一次调用完成所有配置，驱动会自动配置DMA通道
lisa_spi_configure(spi0, &config);

// DMA传输需要32字节对齐的缓冲区
uint8_t *tx_buf = lisa_mem_align_alloc(32, 8);
uint8_t *rx_buf = lisa_mem_align_alloc(32, 8);

memcpy(tx_buf, (uint8_t[]){1,2,3,4,5,6,7,8}, 8);

lisa_spi_transfer_t transfer = {
    .tx_buf = tx_buf,
    .rx_buf = rx_buf,
    .len = 8,
};

int ret = lisa_spi_transfer(spi0, &transfer);
// 等待回调通知传输完成

// 释放内存
lisa_mem_free(tx_buf);
lisa_mem_free(rx_buf);
```

### 主从机通信示例

```c
// 主机配置
lisa_spi_config_t master_config = LISA_SPI_CONFIG_DEFAULT();
master_config.master_mode = true;  // 主机模式
master_config.frequency = 1000000;  // 1MHz
lisa_spi_configure(spi_master, &master_config);

// 从机配置
lisa_spi_config_t slave_config = LISA_SPI_CONFIG_DEFAULT();
slave_config.master_mode = false;  // 从机模式
slave_config.frequency = 1000000;  // 频率需与主机匹配
lisa_spi_configure(spi_slave, &slave_config);

// 主机发起传输
const uint8_t master_tx[] = {0x12, 0x34, 0x56, 0x78};
uint8_t master_rx[4] = {0};
lisa_spi_transfer_t master_xfer = {
    .tx_buf = master_tx,
    .rx_buf = master_rx,
    .len = 4,
};

// 从机准备接收
const uint8_t slave_tx[] = {0x87, 0x65, 0x43, 0x21};
uint8_t slave_rx[4] = {0};
lisa_spi_transfer_t slave_xfer = {
    .tx_buf = slave_tx,
    .rx_buf = slave_rx,
    .len = 4,
};

// 从机先开始传输（等待主机时钟）
lisa_spi_transfer(spi_slave, &slave_xfer);
// 主机开始传输
lisa_spi_transfer(spi_master, &master_xfer);
```

### 模式切换示例

```c
// 从中断模式切换到DMA模式
lisa_spi_config_t config;
lisa_spi_get_config(spi0, &config);

config.tx_transfer_mode = LISA_SPI_DMA_TRANSFER;
config.rx_transfer_mode = LISA_SPI_DMA_TRANSFER;
config.tx_dma_channel = 0;
config.rx_dma_channel = 1;

lisa_spi_configure(spi0, &config);  // 驱动自动配置DMA通道

// 从DMA模式切换回中断模式
config.tx_transfer_mode = LISA_SPI_INTERRUPT_TRANSFER;
config.rx_transfer_mode = LISA_SPI_INTERRUPT_TRANSFER;

lisa_spi_configure(spi0, &config);  // 驱动自动释放DMA通道
```

## 配置宏

驱动提供便捷配置宏用于快速初始化：

- `LISA_SPI_CONFIG_DEFAULT()` - 模式0，8位，MSB先行，1MHz，主机模式，中断传输，硬件控制片选
- `LISA_SPI_CONFIG_HIGH_SPEED()` - 模式0，8位，MSB先行，10MHz，主机模式，中断传输，硬件控制片选

## SPI 模式说明

| 模式 | CPOL | CPHA | 描述 |
|------|------|------|------|
| 0 | 0 | 0 | 空闲时CLK为低电平，第一个时钟边沿采样 |
| 1 | 0 | 1 | 空闲时CLK为低电平，第二个时钟边沿采样 |
| 2 | 1 | 0 | 空闲时CLK为高电平，第一个时钟边沿采样 |
| 3 | 1 | 1 | 空闲时CLK为高电平，第二个时钟边沿采样 |

## 传输模式

- `LISA_SPI_INTERRUPT_TRANSFER` - 使用中断模式传输，适用于小数据量传输
- `LISA_SPI_DMA_TRANSFER` - 使用 DMA 模式传输，适用于大数据量传输（需预留DMA通道）

## 片选控制

- `LISA_SPI_FLAG_HARDWARE_CS` - 硬件控制片选，由SPI控制器自动管理CS信号
- `LISA_SPI_FLAG_SOFTWARE_CS` - 软件控制片选，用户通过GPIO手动控制CS信号

## 注意事项

1. **DMA 对齐**: 使用 DMA 模式时，发送和接收缓冲区必须32字节对齐，建议使用 `lisa_mem_align_alloc` 分配内存
2. **DMA 通道**: 使用 DMA 模式前需预留 DMA 通道，通道号在配置中指定
3. **自动管理**: DMA 通道由驱动自动管理，模式切换时会自动释放，无需手动操作
4. **主从同步**: 从机模式下需要先调用传输函数等待主机时钟，主机负责产生时钟信号
5. **频率匹配**: 主从机通信时，从机的频率配置应与主机匹配或更高
7. **回调上下文**: 传输完成回调在中断上下文执行，应保持简短快速
8. **错误处理**: 传输函数返回错误码，应检查返回值并进行适当的错误处理

