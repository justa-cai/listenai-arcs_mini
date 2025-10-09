# aon_wdg示例
本示例展示了aon_wdg的​中断喂狗功能，从AON_WDT_Reload中加载计数值，计数值清0后进入中断执行喂狗


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
I/elog            [00:00:00.000 1 elog_async] EasyLogger V2.2.99 is initialize success.
I/Aon_Wdt Sample  [00:00:00.001 1 main] Hello, world! Aon_Wdt
I/Aon_Wdt Sample  [00:00:00.734 1 isr] Aon_Wdt trigger,feed dog
I/Aon_Wdt Sample  [00:00:00.734 1 main] Aon_Wdt end
```
