# PWM收发的示例
本示例展示了PWM的输出示例，其中PWM0输出1kHZ占空比为50%的波形，PWM1输出2kHZ占空比为70%的波形。

## 硬件连接

- 芯片PWM引脚如下

| 引脚  | PWM    | 备注 |
|------|-------  | ---- |
| PA20 |PWM0 | PWM通道0输出 |
| PA21 |PWM1 | PWM通道1输出 |

- 需要用逻辑分析仪或示波器观察PA20和PA21引脚的PWM输出频率和占空比

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
Hello, world! PWM
```
