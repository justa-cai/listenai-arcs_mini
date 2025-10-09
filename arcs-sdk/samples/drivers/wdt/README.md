# WDT示例
本示例演示了看门狗外设的功能。
示例中设置看门狗中断时间为1s,复位阶段为0.5s。然后刷新了10次看门狗，然后等待看门狗触发中断并复位芯片。

## 硬件连接
连接日志串口

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

芯片的串口日志如下：

```
********Arcs SDK@V0.0.9-5-g9273928a-dirty-@v0.0.9********
Running on hart-id: 1
Hello, world! WDT
refresh
refresh
refresh
refresh
refresh
refresh
refresh
refresh
refresh
refresh
waiting WDT interrupt trigger

********Arcs SDK@V0.0.9-5-g9273928a-dirty-@v0.0.9********
Running on hart-id: 1
Hello, world! WDT
refresh
refresh
refresh
refresh
refresh
refresh
refresh
refresh
refresh
refresh
waiting WDT interrupt trigger

********Arcs SDK@V0.0.9-5-g9273928a-dirty-@v0.0.9********
Running on hart-id: 1
Hello, world! WDT
refresh
refresh
refresh
refresh
...
```
