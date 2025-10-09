# GPIO输出示例
本示例展示了GPIO引脚输出功能, 本示例会周期性地将PA20引脚输出高电平和低电平。

## 硬件连接
需要用逻辑分析仪或示波器观察PA20引脚

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

```
Hello, world! 
PA20 direction: 0
PA20 direction: 1
```
