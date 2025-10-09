# GPADC示例
本gpadc示例演示了GPADC外设的功能。
GPADC总共有10个采样通道，包含：
- 2个内部采样通道（芯片内部电压测量、芯片温度测量）
- 6个外部GPADC采样引脚 (PB2、PB3、PB4、PB5、PB6、PB7)
- 2个keysense采样通道(PB2和PB3)

本示例演示了GPADC对PB4、PB6、PB7引脚进行了采样，以及对芯片内部电压采样的情况。


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

芯片的串口日志如下（具体采样值与实际情况有关）：

```
Running on hart-id: 1
Hello, world! GPADC
start adc ..............................................
channel type id 6, adc value 0x359/3012mV
channel type id 8, adc value 0x2eb/2626mV
channel type id 9, adc value 0xa0/562mV
channel type id 0, adc value 0x3a8/3290mV
start adc ..............................................
channel type id 6, adc value 0x13a/1103mV
channel type id 8, adc value 0x1ec/1729mV
channel type id 9, adc value 0xa0/562mV
channel type id 0, adc value 0x3a7/3287mV
start adc ..............................................
channel type id 6, adc value 0x13b/1107mV
channel type id 8, adc value 0x1eb/1726mV
channel type id 9, adc value 0xa1/566mV
channel type id 0, adc value 0x3a8/3290mV
start adc ..............................................
channel type id 6, adc value 0x13b/1107mV
channel type id 8, adc value 0x1eb/1726mV
channel type id 9, adc value 0xa1/566mV
channel type id 0, adc value 0x3a8/3290mV
start adc ..............................................
channel type id 6, adc value 0x13b/1107mV
channel type id 8, adc value 0x1eb/1726mV
channel type id 9, adc value 0xa1/566mV
channel type id 0, adc value 0x3a8/3290mV
...
```
