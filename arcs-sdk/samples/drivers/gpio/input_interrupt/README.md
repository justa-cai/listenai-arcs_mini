# GPIO的输入中断示例
本示例展示了如何使用GPIO的输入中断功能。

## 硬件连接
需要连接PA20到PA21引脚。

## 软件编译
在本目录下执行下面命令(PC端要求是ubuntu平台)

```shell
./build.sh
```

## 固件烧录

### ap固件烧录

### cp固件烧录

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/arcs.bin
```

## 日志输出

```
Hello, world! 
Trigger GPIOA Negative interrupt, event: 0x100000
[GPIOA INT] PASS
```
