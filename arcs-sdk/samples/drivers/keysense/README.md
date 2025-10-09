# keysense示例

keysense 即模拟按键接口，共 2 个通道。每个通道最多支持 8 个按键（取决于按键电阻序列的精度），通过 GPADC 检测电压后确认按键值。keysense具有下面特性：
* 可选 kHz 计数时钟源：32KHz RC 和从 24MHz XTAL 分频的时钟
* 可配置的wakeup阈值和key-measure阈值 
* 支持按键感应：wakeup和中断 
* 支持按键测量：硬件 SAR ADC 触发控制 
* 支持按键press和按键release的中断请求

本示例演示了keysense外设的简单功能。通过注册key press和key release中断，如果key press中断触发，通过GPADC测量按键值，这个可以用于多个按键的识别和区分。

目前本开发板keysense0引脚没有接到具体按键，用户可以通过触摸PB2的引脚来触发按键事件，本示例按键采样值可能不准，仅作为演示。

## 硬件连接
- 芯片keysense引脚如下

| 引脚  | keysense    | 备注 |
|------|-------  | ---- |
| PB2 |用于keysense0 | keysense0外设的输入通道 |
| PB3 |用于keysense1 | keysense1外设的输入通道 |

- 本示例只用到了keysense0

## 软件编译
在本目录下执行下面命令(PC端要求是ubuntu平台)

```shell
./build.sh
```

## 固件烧录

### ap固件烧录

### cp固件烧录

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/arcs.bin
```

## 日志输出

芯片的串口日志如下：

```
Running on hart-id: 1
Hello, world! KEYSENSE
keysense press event generate
channel type id 1, adc value 0x336/963mV
keysense release event generate

```
