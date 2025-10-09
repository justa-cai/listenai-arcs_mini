## 示例说明
- 演示了AP和CP两个核使用ic_mutex核间锁来互斥的使用硬件加密模块，具体即两个核分别对同样的输入数据进行sha256计算，得到一样的结果。
- 如果AP和CP的prj.conf里面添加`CONFIG_ARCS_HAL_IC_MUTEX=n`的选项，就会发现两个核的加密结果不一致，说明核间锁起到了作用。

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
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
...
```

### CP的串口日志如下：

```
********Arcs SDK@V0.0.10-6-g6e971fb7-dirty-@v0.0.10********
Running on hart-id: 1
CP=======! Hard ID: 1
ic_message_init done!
ic_mutex_task enter...
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
...
```

