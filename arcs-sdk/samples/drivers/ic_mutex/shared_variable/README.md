## 示例说明
- 演示了AP和CP两个核使用ic_mutex核间锁来互斥的使用在SRAM中的同一个位置的共享变量，两个核分别对这个变量进行自增操作，并打印结果。正确的结果是两个核的打印结果不一致。

## AP编译和烧录

### AP编译

```shell
cd ap
./build.sh
```

### AP烧录
AP固件烧录到flash的0位置

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/ap.bin
```

## CP编译和烧录

### CP编译

```shell
cd cp
./build.sh
```

### CP烧录
CP固件烧录到flash的0xd00000位置

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0xd00000 build/helloworld.bin
```

## 日志输出

AP串口日志在PB02引脚，CP串口日志在PA03引脚

### AP的串口日志如下：

```
********Arcs SDK@V0.0.10-6-g6e971fb7-dirty-@v0.0.10********
Running on hart-id: 0
AP Hard ID: 0
boot cp from address: 0x30d00000
ic_message_init done!
ic_mutex_task enter...
gCounter == 1
gCounter == 3
gCounter == 5
gCounter == 7
gCounter == 9
gCounter == 11
gCounter == 13
gCounter == 15
gCounter == 17
gCounter == 19
gCounter == 21
gCounter == 23
gCounter == 25
gCounter == 27
gCounter == 29
gCounter == 31
...
```

### CP的串口日志如下：

```
********Arcs SDK@V0.0.10-6-g6e971fb7-dirty-@v0.0.10********
Running on hart-id: 1
CP=======! Hard ID: 1
ic_message_init done!
ic_mutex_task enter...
gCounter == 2
gCounter == 4
gCounter == 6
gCounter == 8
gCounter == 10
gCounter == 12
gCounter == 14
gCounter == 16
gCounter == 18
gCounter == 20
gCounter == 22
gCounter == 24
gCounter == 26
gCounter == 28
gCounter == 30
...
```

