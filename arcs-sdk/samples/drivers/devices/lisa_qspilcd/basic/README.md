# LISA QSPILCD PIO 传输示例

## 功能说明

演示使用 QSPI LCD 驱动进行 PIO（轮询）模式的数据传输，适用于小数据量命令传输场景。

PIO 模式下 CPU 直接参与数据搬移，传输完成后函数返回，无需额外等待。

## 硬件连接

- **PA18**: QSPI CLK（时钟）
- **PA19**: QSPI CS（片选，可选使用 GPIO 控制）
- **PA20**: QSPI IO0（数据线 0）
- **PA21**: QSPI IO1（数据线 1）
- **PA22**: QSPI IO2（数据线 2）
- **PA23**: QSPI IO3（数据线 3）

连接到支持 QSPI 接口的 LCD 显示屏模组。

## 使用场景

适用于发送 LCD 命令、配置寄存器等小数据量传输场景，实现简单，无需配置 DMA。

## 示例步骤

1. 获取 QSPI LCD 设备
2. 配置 CS 引脚（可选，使用 GPIO 手动控制）
3. 准备传输数据缓冲区
4. 配置传输参数（数据线模式、位宽、PIO 模式）
5. 调用 `lisa_qspilcd_transfer()` 同步传输

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
=== LISA QSPILCD PIO example ===
QSPILCD device ready
CS GPIO configured
PIO transfer (Single lane, 8-bit): OK
PIO transfer (Quad lane, 8-bit): OK
All transfers completed successfully!
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 QSPI LCD 设备 |
| `lisa_qspilcd_control()` | 配置 QSPI 控制参数（模式、时钟极性、数据位宽、总线速度等） |
| `lisa_qspilcd_cs_configure()` | 配置 CS 引脚（GPIO 手动控制） |
| `lisa_qspilcd_cs_control()` | 控制 CS 引脚电平 |
| `lisa_qspilcd_transfer()` | 启动数据传输（PIO 模式） |

## 控制参数配置

使用 `lisa_qspilcd_control()` 配置 QSPI LCD 的工作参数：

```c
uint32_t control = LISA_QSPILCD_TXIO_PIO |      /* TX 使用 PIO 模式 */
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

## PIO 传输说明

`lisa_qspilcd_transfer()` 在 PIO 模式下的工作原理：

- **底层实现**：CPU 直接将数据写入 QSPI FIFO，轮询等待传输完成
- **返回时机**：传输完成后函数返回，无需额外等待
- **适用场景**：小数据量命令传输（如 LCD 初始化命令、寄存器配置）
- **返回值**：成功返回 0，失败返回负数错误码

## 关键代码

```c
/* 获取 QSPI LCD 设备 */
lisa_device_t *qspi = lisa_device_get("qspilcd0");

/* 配置 CS 引脚（使用 GPIO 手动控制） */
lisa_device_t *gpio = lisa_device_get("gpioa");
lisa_qspilcd_cs_configure(qspi, gpio, 19);

/* 准备传输数据 */
uint8_t cmd[] = {0x2A, 0x00, 0x00, 0x00, 0xEF};

/* 配置 PIO 传输参数 */
lisa_qspilcd_xfer_t xfer = {
    .buf = cmd,
    .size_bytes = sizeof(cmd),
    .lane = LISA_QSPILCD_LANE_SINGLE,  /* 单线模式 */
    .data_bits = 8,
    .use_dma = false,                   /* PIO 模式 */
};

/* 拉低 CS，启动传输 */
lisa_qspilcd_cs_control(qspi, false);
lisa_qspilcd_transfer(qspi, &xfer);

/* 拉高 CS，结束传输 */
lisa_qspilcd_cs_control(qspi, true);
```

## 配置说明

### 数据线模式

| 模式 | 枚举值 | 说明 |
|------|--------|------|
| 单线 | `LISA_QSPILCD_LANE_SINGLE` | 使用 IO0 传输，兼容性最好 |
| 双线 | `LISA_QSPILCD_LANE_DUAL` | 使用 IO0/IO1 传输，带宽翻倍 |
| 四线 | `LISA_QSPILCD_LANE_QUAD` | 使用 IO0-IO3 传输，带宽最高 |

## 注意事项

1. **CS 控制时序**：手动 CS 模式下，需在传输前拉低、传输后拉高 CS
2. **数据线模式切换**：切换数据线模式应在总线空闲时进行
3. **适用数据量**：PIO 模式适合小数据量传输，大数据量建议使用 DMA 模式
