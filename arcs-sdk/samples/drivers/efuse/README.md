# efuse的示例
本示例展示了efuse的读取功能，先读取efuse的uuid，在读取efuse地址0x0的值


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
Hello, world! efuse
Device UUID: 0x0000000000000000
Start dumping eFuse contents:
eFuse[0x00] = 0x00000000
eFuse dump complete.
```
