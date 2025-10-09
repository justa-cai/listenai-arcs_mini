# RTC示例
本RTC示例演示了RTC闹钟的功能和分钟中断的能力。
本示例中通过设置RTC时间和RTC闹钟时间， RTC时间在到达RTC闹钟时间时会触发闹钟中断。
本示例中RTC也会在整分钟的时候产生中断。

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

芯片的串口日志如下：

```
Running on hart-id: 1
Hello, world! RTC
RTC Minute interrupt trigger
Years: 1; Month: 6; Week: 5; Day: 6; Hour: 14; Min: 49; Sec: 0
RTC Alarm interrupt trigger
Years: 1; Month: 6; Week: 5; Day: 6; Hour: 14; Min: 49; Sec: 10
RTC Minute interrupt trigger
Years: 1; Month: 6; Week: 5; Day: 6; Hour: 14; Min: 50; Sec: 0
RTC Minute interrupt trigger
Years: 1; Month: 6; Week: 5; Day: 6; Hour: 14; Min: 51; Sec: 0
RTC Minute interrupt trigger
Years: 1; Month: 6; Week: 5; Day: 6; Hour: 14; Min: 52; Sec: 0
...
```
