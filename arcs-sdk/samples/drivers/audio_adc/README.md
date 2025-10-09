# audio adc示例
本示例演示了通过AP核录音，CP核获取录音数据的功能。
1. 示例代码在ap核运行，cp核通过共享内存获取音频数据。
2. 传输过来的音频数据为双麦16kHz采样率，16位采样精度。


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
Running on hart-id: 1
I/elog            [00:00:00.000 1 elog_async] EasyLogger V2.2.99 is initialize success.
Hello, world! Audio ADC
Stream frame: 0x28007b40
Stream frame: 0x28007f40
Stream frame: 0x28008340
Stream frame: 0x28008740
Stream frame: 0x28008b40
Stream frame: 0x28008f40
Stream frame: 0x28009340
Stream frame: 0x28009740
Stream frame: 0x28009b40
Stream frame: 0x28009f40
Stream frame: 0x2800a340
Stream frame: 0x2800a740
Stream frame: 0x2800ab40
Stream frame: 0x2800af40
Stream frame: 0x2800b340
```
