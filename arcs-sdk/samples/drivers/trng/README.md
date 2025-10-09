# trng的示例
本示例展示了TRNG的真随机数数生成功能，启动TRNG后进入中断回调函数生成随机数，共生成5次


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
Hello, world! TRNG
TRNG test interrupt modes, test begintrng data is below:
trng data is 0xbb209e5e
...
trng interrupt test end!!!!
```
