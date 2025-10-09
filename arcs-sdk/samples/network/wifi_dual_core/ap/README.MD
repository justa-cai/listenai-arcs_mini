# 说明

这是用于验证 双核wifi 的sample，AP侧固件

## 前置条件
确保已经安装了git lfs, 运行以下命令拉取大文件 
```shell
git lfs pull
```

## 烧录固件
```shell
./tools/sqlite/image/cskburn_v2.0.2 -s /dev/ttyACM0 -b 3000000 0x0 build/arcs.bin
```

