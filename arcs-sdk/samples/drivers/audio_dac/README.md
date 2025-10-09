# audio adc示例
本示例演示了通过AP控制DAC播放音频，CP发送音频数据。
1. 示例代码在cp核运行，ap核进行实际的音频播放。
2. cp核生成1kHz的音频数据，传输到ap核进行播放。


## 硬件连接
连接日志串口

## 软件编译
在本目录下执行下面命令(PC端要求是ubuntu平台)

```shell
./build.sh
```

## 固件烧录

### ap固件烧录

烧录示例源码根目录下的ap.bin

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 ap.bin
```

### cp固件烧录

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0xa00000 build/arcs.bin
```

## 日志输出

芯片的串口日志如下（具体采样值与实际情况有关）：

```
********Arcs SDK@V0.0.10-8-g52552ce3-dirty-@v0.0.10********
Running on hart-id: 1
Hello, world! Audio DAC
```
