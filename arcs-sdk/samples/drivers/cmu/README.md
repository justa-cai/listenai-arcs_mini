# CMU示例
本示例展示读取并打印关键时钟频率（CPU/HCLK/APB/FLASH 及常用外设），并输出一次成功标识。

## 硬件连接
连接日志串口

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
********Arcs SDK@test_release-31-g6edf105a-dirty-@v0.0.22********
Running on hart-id: 1
Hello, world! CMU
CPU: 300000000 Hz
HCLK: 300000000 Hz
APB: 100000000 Hz
FLASH: 100000000 Hz
UART0: 3686400 Hz
UART1: 0 Hz
UART2: 0 Hz
SPI0: 0 Hz
I2C0: 0 Hz
[CMU] HCLK cfg: src=1(CoreClk) n=1 m=1
[CMU] APB cfg: n=1 m=3
[CMU] UART0 cfg: src=3(XtalClk) n=96 m=625
CMU check success
```


