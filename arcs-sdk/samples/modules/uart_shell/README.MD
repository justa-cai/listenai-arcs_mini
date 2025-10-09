# UART_SHELL的示例
本示例展示了UART_SHELL的功能

## 硬件连接

- 芯片UART引脚如下

| 引脚  | UART    | 备注 |
|------|-------  | ---- |
| PA2 |UART0_RX | UART0接收 |
| PA3 |UART0_TX | UART0发送 |


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
