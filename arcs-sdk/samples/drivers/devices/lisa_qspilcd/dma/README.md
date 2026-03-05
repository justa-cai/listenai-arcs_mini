# LISA QSPILCD DMA 传输示例

## 功能说明

演示使用 QSPI LCD 驱动进行 DMA（直接内存访问）模式的数据传输，适用于大数据量像素传输场景。

底层使用 GPDMA 硬件进行数据搬移，CPU 无需参与，通过信号量实现异步等待。

### 新特性

- **DMA 硬件传输**：无需 CPU 参与数据搬移，降低 CPU 占用
- **异步非阻塞**：调用后立即返回，DMA 后台传输
- **高带宽传输**：支持四线并行传输，适合大数据量场景
- **信号量同步**：传输完成后通过信号量通知应用层

## 硬件连接

- **PA18**: QSPI CLK（时钟）
- **PA19**: QSPI CS（片选，可选使用 GPIO 控制）
- **PA20**: QSPI IO0（数据线 0）
- **PA21**: QSPI IO1（数据线 1）
- **PA22**: QSPI IO2（数据线 2）
- **PA23**: QSPI IO3（数据线 3）

连接到支持 QSPI 接口的 LCD 显示屏模组。

## 使用场景

适用于需要高速传输 LCD 显示数据的场景，底层采用 DMA 硬件传输，CPU 占用更低，支持四线并行传输提升带宽。

## 示例步骤

1. 获取 QSPI LCD 设备
2. 配置 CS 引脚（可选，使用 GPIO 手动控制）
3. 准备传输数据缓冲区（32 字节对齐）
4. 配置传输参数（数据线模式、位宽、DMA 使能）
5. 调用 `lisa_qspilcd_transfer()` 启动 DMA 传输
6. 调用 `lisa_qspilcd_wait_done()` 等待传输完成

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
=== LISA QSPILCD DMA example ===
QSPILCD device ready
CS GPIO configured
DMA transfer started (Quad lane, 8-bit)
DMA transfer complete, time: 2 ms
All transfers completed successfully!
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 QSPI LCD 设备 |
| `lisa_qspilcd_control()` | 配置 QSPI 控制参数（模式、时钟极性、数据位宽、总线速度等） |
| `lisa_qspilcd_cs_configure()` | 配置 CS 引脚（GPIO 手动控制） |
| `lisa_qspilcd_cs_control()` | 控制 CS 引脚电平 |
| `lisa_qspilcd_transfer()` | 启动数据传输（DMA 模式） |
| `lisa_qspilcd_wait_done()` | 等待 DMA 传输完成 |

## 控制参数配置

使用 `lisa_qspilcd_control()` 配置 QSPI LCD 的工作参数：

```c
uint32_t control = LISA_QSPILCD_TXIO_DMA |      /* TX 使用 DMA 模式 */
                   LISA_QSPILCD_CPOL0_CPHA0 |   /* 时钟极性和相位 */
                   LISA_QSPILCD_MSB_LSB |       /* MSB 优先 */
                   LISA_QSPILCD_MODE_MASTER |   /* 主机模式 */
                   LISA_QSPILCD_DATA_BITS(8);   /* 8 位数据宽度 */

lisa_qspilcd_control(qspi, control, 50*1000*1000);  /* 50MHz 总线速度 */
```

### 控制码字段说明

| 字段 | 宏定义 | 说明 |
|------|--------|------|
| **TX I/O 模式** | `LISA_QSPILCD_TXIO_PIO` | 使用 CPU 轮询发送 |
|                 | `LISA_QSPILCD_TXIO_DMA` | 使用 DMA 发送 |
|                 | `LISA_QSPILCD_TXIO_AUTO` | 自动选择（PIO 或 DMA） |
| **时钟极性/相位** | `LISA_QSPILCD_CPOL0_CPHA0` | CPOL=0, CPHA=0（Mode 0） |
|                   | `LISA_QSPILCD_CPOL0_CPHA1` | CPOL=0, CPHA=1（Mode 1） |
|                   | `LISA_QSPILCD_CPOL1_CPHA0` | CPOL=1, CPHA=0（Mode 2） |
|                   | `LISA_QSPILCD_CPOL1_CPHA1` | CPOL=1, CPHA=1（Mode 3） |
| **位序** | `LISA_QSPILCD_MSB_LSB` | 高位优先（MSB first） |
|          | `LISA_QSPILCD_LSB_MSB` | 低位优先（LSB first） |
| **主从模式** | `LISA_QSPILCD_MODE_MASTER` | 主机模式 |
|              | `LISA_QSPILCD_MODE_SLAVE` | 从机模式 |
| **数据位宽** | `LISA_QSPILCD_DATA_BITS(n)` | 设置数据位宽（n = 1~32） |

### 函数原型

```c
lisa_err_t lisa_qspilcd_control(lisa_device_t *dev, uint32_t control, uint32_t arg);
```

- **dev**: QSPI LCD 设备句柄
- **control**: 控制码，由上述宏按位或组合
- **arg**: 附加参数，通常为总线速度（Hz）
- **返回值**: 成功返回 `LISA_DEVICE_OK`，失败返回错误码

## DMA 传输说明

`lisa_qspilcd_transfer()` 在 DMA 模式下的工作原理：

- **底层实现**：GPDMA 硬件自动搬移数据到 QSPI FIFO，CPU 无需参与
- **返回时机**：调用后立即返回，不等待传输完成
- **完成通知**：DMA 传输完成后触发中断，释放信号量
- **等待方式**：调用 `lisa_qspilcd_wait_done()` 阻塞等待完成
- **返回值**：成功返回 0，超时返回 `LISA_DEVICE_ERR_TIMEOUT`

## 关键代码

```c
/* 获取 QSPI LCD 设备 */
lisa_device_t *qspi = lisa_device_get("qspilcd0");

/* 配置 CS 引脚（使用 GPIO 手动控制） */
lisa_device_t *gpio = lisa_device_get("gpioa");
lisa_qspilcd_cs_configure(qspi, gpio, 19);

/* 准备传输数据（32 字节对齐） */
static uint8_t tx_buf[256] __attribute__((aligned(32)));

/* 配置 DMA 传输参数 */
lisa_qspilcd_xfer_t xfer = {
    .buf = tx_buf,
    .size_bytes = sizeof(tx_buf),
    .lane = LISA_QSPILCD_LANE_QUAD,  /* 四线模式 */
    .data_bits = 8,
    .use_dma = true,                  /* DMA 模式 */
};

/* 拉低 CS，启动传输 */
lisa_qspilcd_cs_control(qspi, false);
lisa_qspilcd_transfer(qspi, &xfer);

/* 等待 DMA 完成 */
lisa_qspilcd_wait_done(qspi, 1000);

/* 拉高 CS，结束传输 */
lisa_qspilcd_cs_control(qspi, true);
```

## 配置说明

### DMA 通道配置

在 `prj.conf` 中配置 DMA 通道：

```kconfig
CONFIG_LISA_QSPILCD_GPDMA_CH=0
```

### 内存对齐要求

- **要求**：DMA 模式下发送缓冲区必须 **32 字节对齐**
- **实现**：使用 `__attribute__((aligned(32)))` 属性
- **原因**：DMA 控制器对内存地址有对齐要求，可提升传输效率

### 数据线模式

| 模式 | 枚举值 | 说明 |
|------|--------|------|
| 单线 | `LISA_QSPILCD_LANE_SINGLE` | 使用 IO0 传输，兼容性最好 |
| 双线 | `LISA_QSPILCD_LANE_DUAL` | 使用 IO0/IO1 传输，带宽翻倍 |
| 四线 | `LISA_QSPILCD_LANE_QUAD` | 使用 IO0-IO3 传输，带宽最高 |

## 注意事项

1. **DMA 通道冲突**：确保配置的 DMA 通道没有与其他外设冲突
2. **缓冲区生命周期**：DMA 传输完成前不能释放或修改发送缓冲区内容
3. **内存对齐**：DMA 模式必须使用 32 字节对齐的发送缓冲区
4. **传输同步**：DMA 模式必须调用 `lisa_qspilcd_wait_done()` 等待完成
5. **CS 控制时序**：手动 CS 模式下，需在传输前拉低、传输后拉高 CS
