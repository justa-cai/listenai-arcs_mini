# aon_timer的示例
本示例展示了aon_timer的​​周期性计数模式，该模式下会从AON_TIMER_RELOAD_COUNT周期装载计数值进行计数


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
Hello, world! aon_timer
aon timer trigger
aon_timer end
```
