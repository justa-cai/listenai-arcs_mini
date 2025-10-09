# UART收发的示例
本示例展示了UART的收发功能，使用的是中断模式。本示例会把接收到的数据原样发送回去。

## 硬件连接

- 芯片UART引脚如下

| 引脚  | UART    | 备注 |
|------|-------  | ---- |
| PA20 |UART2_RX | UART2接收 |
| PA21 |UART2_TX | UART2发送 |

- PC端需要通过串口板来连接芯片的PA20和PA21引脚

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

用户在PC端用串口软件发送数据，并且在串口软件的接收端看是否收到相同的数据。芯片的串口日志如下：

```
Hello, world! UART RX TX
```
