# SPI示例
本示例演示了SPI外设的功能。具体是SPI作为master，每隔100ms输出1024个字节的数据。

## 硬件连接
- 芯片SPI引脚如下

| 引脚  | SPI    | 备注 |
|------|-------  | ---- |
| PA15 |SPI_MOSI | SPI master数据发送引脚 |
| PA14 |SPI_MISO | SPI master数据接收引脚 |
| PA17 |SPI_CLK | SPI 时钟引脚 |
| PA19 |SPI_CS | SPI 片选引脚 |

- 需要用逻辑分析仪或示波器采样这些引脚来分析SPI的输出数据

## 软件编译
在本目录下执行下面命令(PC端要求是ubuntu平台)

```shell
./build.sh
```

## 固件烧录

### cp固件烧录

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/arcs.bin
```

## 日志输出

芯片的串口日志如下：

```
Running on hart-id: 1
Hello, world! SPI
spi_set_bus_speed: sclk_div = 11 (sclk: 1000000), cs2sclk = 1

```
