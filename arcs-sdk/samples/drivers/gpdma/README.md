# gpdma的示例
本示例展示了使用gpdma进行内存到内存（M2M）数据传输


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
Hello, world! gpdma
GPDMA Normal mode trigger
[GPDMA][NORMAL][M2M][CH0][Word] source: 0x200132dc -> destination: 0x20013470 transfer success!!!
gpdma end
```
