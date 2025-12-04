# SPI 示例

本示例演示了 SPI 外设的功能。SPI 作为 master 模式，每隔 100ms 输出 1024 个字节的数据。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **SPI Master 模式**：SPI 作为主机模式工作
- **DMA 传输**：使用 DMA 发送数据
- **周期性发送**：每隔 100ms 发送一次数据
- **引脚复用配置**：配置 SPI1 的 MOSI、MISO、CLK、CS 引脚

### SPI 配置参数

| 参数 | 值 | 说明 |
|------|-----|------|
| SPI 控制器 | SPI1 | 使用 SPI1 外设 |
| 模式 | Master | 主机模式 |
| 时钟频率 | 1MHz | Master 输出时钟 |
| 数据位宽 | 8位 | 每次传输 8 位数据 |
| 时钟极性/相位 | CPOL=1, CPHA=1 | 时钟配置 |
| 数据顺序 | MSB First | 最高位优先 |
| 传输方式 | DMA | 使用 DMA 发送 |
| 数据长度 | 1024 字节 | 每次发送数据量 |

### SPI 引脚配置

| 引脚 | SPI 功能 | IOMUX 功能 | 说明 |
|------|----------|------------|------|
| PA15 | SPI_MOSI | 功能 6 | SPI Master 数据发送引脚 |
| PA14 | SPI_MISO | 功能 6 | SPI Master 数据接收引脚 |
| PA17 | SPI_CLK | 功能 6 | SPI 时钟引脚 |
| PA19 | SPI_CS | 功能 6 | SPI 片选引脚 |

### 硬件要求

- ARCS 系列开发板
- 逻辑分析仪或示波器（用于采样 SPI 引脚，分析 SPI 输出数据）
- USB 转串口工具（用于查看日志输出）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/spi
./build.sh
```

构建成功后，会在 `build` 目录下生成 `arcs.bin` 文件。

### 2. 烧录运行

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/arcs.bin -C arcs
```

参数说明：
- `-s /dev/ttyUSB0`：串口设备路径（根据实际情况修改）
- `-b 3000000`：烧录波特率
- `0x0`：烧录起始地址
- `build/arcs.bin`：烧录固件路径

> 💡 详细烧录步骤请参考 {ref}`快速开始 - 烧录运行 <flashing>`。

### 3. 查看输出

烧录完成后，复位开发板：
- 通过串口工具查看日志输出
- 使用逻辑分析仪或示波器采样 SPI 引脚，分析输出数据

## 🔧 配置说明

### 项目配置

本示例在 `prj.conf` 中配置：

```conf
CONFIG_MEM_CONFIG=y
```

### SPI 引脚定义

```c
#define SPI_MOSI_IO_PAD         CSK_IOMUX_PAD_A
#define SPI_MOSI_IO_PIN         15
#define SPI_MOSI_IO_SEL         CSK_IOMUX_FUNC_ALTER6

#define SPI_MISO_IO_PAD         CSK_IOMUX_PAD_A
#define SPI_MISO_IO_PIN         14
#define SPI_MISO_IO_SEL         CSK_IOMUX_FUNC_ALTER6

#define SPI_CLK_IO_PAD          CSK_IOMUX_PAD_A
#define SPI_CLK_IO_PIN          17
#define SPI_CLK_IO_SEL          CSK_IOMUX_FUNC_ALTER6

#define SPI_CS_IO_PAD           CSK_IOMUX_PAD_A
#define SPI_CS_IO_PIN           19
#define SPI_CS_IO_SEL           CSK_IOMUX_FUNC_ALTER6
```

### SPI 控制配置

```c
SPI_Control(spi_handler, 
    CSK_SPI_MODE_MASTER |      // master 模式
    CSK_SPI_TXIO_DMA |         // 使用 DMA 发送
    CSK_SPI_CPOL1_CPHA1 |      // CPOL=1, CPHA=1
    CSK_SPI_DATA_BITS(8) |     // 8位数据
    CSK_SPI_MSB_LSB,           // MSB first
    1000000);                  // spi1 的 master 输出时钟为 1MHz
```

## 📋 代码解析

### 关键代码段

#### 1. 全局变量定义

```c
#define DATA_SIZE 1024

static void* spi_handler = NULL;
static volatile uint32_t spi_event = 0;
static uint8_t data[DATA_SIZE];
```

#### 2. SPI 引脚初始化

```c
void spi_pin_init()
{
   /* SPI1 引脚配置 */
   IOMuxManager_PinConfigure(SPI_CLK_IO_PAD,  SPI_CLK_IO_PIN,  SPI_CLK_IO_SEL);     // CLK
   IOMuxManager_PinConfigure(SPI_CS_IO_PAD,   SPI_CS_IO_PIN,   SPI_CS_IO_SEL);      // CS
   IOMuxManager_PinConfigure(SPI_MOSI_IO_PAD, SPI_MOSI_IO_PIN, SPI_MOSI_IO_SEL);    // MOSI
   IOMuxManager_PinConfigure(SPI_MISO_IO_PAD, SPI_MISO_IO_PIN, SPI_MISO_IO_SEL);    // MISO

   spi_handler = SPI1();
}
```

#### 3. SPI 事件回调函数

```c
static void spi_driver_event_cb(uint32_t event, uint32_t usr_param)
{
    // printf("event: %d\n", event);
    spi_event |= event;
}
```

#### 4. SPI 初始化和配置

```c
/* 初始化 SPI */
SPI_Initialize(spi_handler, spi_driver_event_cb, (uint32_t)spi_handler);
SPI_PowerControl(spi_handler, CSK_POWER_FULL);

/* 配置 SPI */
SPI_Control(spi_handler, 
    CSK_SPI_MODE_MASTER |      // master 模式
    CSK_SPI_TXIO_DMA |         // 使用 DMA 发送
    CSK_SPI_CPOL1_CPHA1 |      // CPOL=1, CPHA=1
    CSK_SPI_DATA_BITS(8) |     // 8位数据
    CSK_SPI_MSB_LSB,           // MSB first
    1000000);                  // spi1 的 master 输出时钟为 1MHz
```

#### 5. 准备发送数据

```c
/* 准备发送数据 */
for (int i = 0; i < DATA_SIZE; i++) {
    data[i] = i % 256;
}
```

数据内容：生成 0~255 循环的数据序列（总共 1024 字节）

#### 6. 周期性发送数据

```c
while(1) {
    spi_event = 0;
    /* 发送数据 */
    SPI_Send(spi_handler, data, DATA_SIZE);

    /* 等待发送完成 */
    while(!(spi_event & CSK_SPI_EVENT_TRANSFER_COMPLETE));
    
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

发送流程：
1. 清除事件标志
2. 调用 `SPI_Send()` 发送 1024 字节数据
3. 等待 `CSK_SPI_EVENT_TRANSFER_COMPLETE` 事件
4. 延时 100ms 后进行下一次发送

#### 7. 资源清理

```c
/* 关闭 SPI */
SPI_PowerControl(spi_handler, CSK_POWER_OFF);
SPI_Uninitialize(spi_handler);
```

#### 8. 主函数

```c
int main(int argc, char **argv)
{
    printf("Hello, world! SPI\n");

    spi_pin_init();

    spi_test();

    return 0;
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
Running on hart-id: 1
Hello, world! SPI
spi_set_bus_speed: sclk_div = 11 (sclk: 1000000), cs2sclk = 1

```

日志说明：
- `sclk_div = 11`：SPI 时钟分频系数
- `sclk: 1000000`：SPI 时钟频率 1MHz
- `cs2sclk = 1`：CS 到时钟的延迟

SPI 引脚输出：
- PA15 (MOSI)：每隔 100ms 输出 1024 字节数据
- PA17 (CLK)：输出 1MHz 时钟信号
- PA19 (CS)：片选信号

## ⚠️ 注意事项

1. **引脚复用**：
   - SPI1 的四个引脚都配置为功能 6（CSK_IOMUX_FUNC_ALTER6）

2. **数据内容**：
   - 发送数据为 0~255 循环序列，共 1024 字节
   - 数据生成：`data[i] = i % 256`

3. **DMA 传输**：
   - 使用 DMA 方式发送数据
   - 需要等待 `CSK_SPI_EVENT_TRANSFER_COMPLETE` 事件完成

4. **时钟配置**：
   - CPOL=1：空闲时时钟为高电平
   - CPHA=1：第二个时钟边沿采样数据
   - MSB First：最高位优先传输

5. **发送间隔**：
   - 每次发送后延时 100ms
   - 使用 `vTaskDelay(pdMS_TO_TICKS(100))`

6. **事件处理**：
   - 通过回调函数 `spi_driver_event_cb` 处理事件
   - 使用全局变量 `spi_event` 记录事件标志

7. **Master 模式**：
   - SPI 作为主机，控制时钟和片选信号
   - 时钟频率设置为 1MHz