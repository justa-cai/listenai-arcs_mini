# DMA收发的示例
本示例展示了使用dma进行内存到内存（M2M）数据传输的全过程，初始化源和目的缓冲区后将数据从源区传输到目的区域并打印


## 软件编译
在本目录下执行下面命令(PC端要求是ubuntu平台)

```shell
./build.sh
```

## 固件烧录


### cp固件烧录

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x1000 build/arcs.bin
```

## 日志输出

芯片的串口日志如下：

```
Hello, world! dma
SourceBuf: DD CC BB AA 
DestinBuf (Before): 00 99 88 77 
[DMA_DrvEvent]: event = 1, channel = 0, xfer_bytes = 4092
After DMA transfer:
DestinBuf (After ): DD CC BB AA 
Memory compare success
```
