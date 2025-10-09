# GPIO输入引脚示例
本示例展示了引脚作为GPIO输入引脚的功能。

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
gpio input enter...
PA21 -> PA20 value = 1
PA21 -> PA20 value = 0
PA21 -> PA20 value = 1
PA21 -> PA20 value = 0
PA21 -> PA20 value = 1
PA21 -> PA20 value = 0
PA21 -> PA20 value = 1
PA21 -> PA20 value = 0
PA21 -> PA20 value = 1
...
```
