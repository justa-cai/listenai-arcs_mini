# 示例说明

1. 此示例演示websocket的使用
2. 可以在linux环境下运行websocket-server.py来作为测试的服务器，运行前需要修改脚本中的ip地址
3. 确保设备和运行websocket-server.py的设备在同一个局域网


### 烧录 AP 固件
```shell
cskburn -s /dev/ttyACM0 -b 3000000 0x0 ap.bin -C arcs
```

### 烧录 CP 应用固件
```shell
cskburn -s /dev/ttyACM0 -b 3000000 0xA00000 build/arcs.bin -C arcs
```
