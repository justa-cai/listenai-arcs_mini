# 示例说明

本示例实现了ADB设备的枚举.
仅仅实现了以下几个基本命令:

1. adb shell
2. adb push
3. adb pull
4. adb ls

> 可使用`CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT`配置adb默认目录, 在使用过程中,如果不使用绝对路径, `adb push/pull`为`CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT`定义的路径下操作

### 速率
```
❯ adb push random_content_1GB.txt 1G.bin
random_content_1GM.txt: 1 file pushed, 0 skipped. 3.8 MB/s (1048576000 bytes in 264.848s)
❯ adb pull 1G.bin 1G.bin.test
1G.bin: 1 file pulled, 0 skipped. 3.9 MB/s (1048576000 bytes in 256.089s)
```