# sysheap的示例
本示例展示了sysheap的sample，分别展示从psram malloc空间和从sram malloc 空间


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
Hello, world! sysheap
SRAM malloc success: 0x2001321c
PSRAM malloc success: 0x28003378
sysheap end.
```
