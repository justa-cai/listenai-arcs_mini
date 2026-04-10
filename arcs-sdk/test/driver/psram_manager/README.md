# PSRAM Manager 测试
用于测试PSRAM Manager初始化是否成功

## 硬件连接
连接日志串口

## 软件编译
在 SDK 根目录执行下面命令(PC端要求是ubuntu平台)

```shell
./build.sh -S test/driver/psram_manager
```

## 固件烧录

### cp固件烧录

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/arcs.bin -C arcs
```
