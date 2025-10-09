# 示例说明

1. 此示例演示wifi扫描、连接、断开连接的使用
2. 示例中固定连接的是"Xiaomi_listenai_2.4G"，如果想连接其它wifi，请修改配置
3. 只能连接2.4G的wifi，不能连接5G的wifi

### 烧录 AP 固件
```shell
./cskburn -s /dev/ttyACM0 -b 3000000 0x0 ap.bin
```

### 烧录 CP 应用固件
```shell
./cskburn -s /dev/ttyACM0 -b 3000000 0xA00000 build/arcs.bin
```
