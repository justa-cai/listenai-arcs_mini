# Zig Blinky 示例

## 功能说明

该示例演示如何在 Zig 中通过 `arcs.Gpio` 打开设备、配置输出引脚并周期性翻转板载 LED。它是一个典型的“设备 HAL + 定时休眠”入门示例，适合快速理解 Zig 侧如何访问 ARCS SDK 的 GPIO 设备框架。

## 硬件连接

该示例面向 `arcs_evb` 板载 LED。

- **设备**: `gpiob`
- **引脚**: `PAD_B[9]`
- **现象**: LED 以 500ms 周期翻转，共切换 20 次

## 示例步骤

1. 初始化 Zig 日志系统
2. 打开 `gpiob` 设备
3. 将 `PAD_B[9]` 配置为 GPIO 输出
4. 循环调用 `toggle()` 翻转 LED
5. 通过 `arcs.sleep(500)` 控制闪烁节奏
6. 输出结束日志并返回

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

建议将示例路径替换为：

```bash
./build.sh -C -S samples/libraries/zig/blinky -DBOARD=arcs_evb
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

串口中可看到类似日志：

```text
=== Zig Blinky Demo ===
Calling zig_blinky_main()...
Blinky starting — opening gpiob...
gpiob opened successfully
LED pin 9 configured as output
LED ON  (toggle #0)
LED OFF (toggle #1)
...
Blinky done — 20 toggles completed
=== Zig Blinky PASSED ===
```

同时板载 LED 会持续闪烁约 10 秒。

在 CI 真机验证中，样例通过条件以串口出现 `=== Zig Blinky PASSED ===` 为准，而不是依赖人工观察 LED 现象。

## 核心 API

| API | 说明 |
|-----|------|
| `arcs.Gpio.open()` | 打开 GPIO 设备 |
| `gpio.configOutput()` | 配置输出引脚 |
| `gpio.toggle()` | 翻转输出电平 |
| `arcs.sleep()` | 毫秒级休眠 |

## 关键代码

```zig
var gpio = arcs.Gpio.open("gpiob") catch return -2;
const LED_PIN: u32 = 9;

gpio.configOutput(LED_PIN, .{ .init_high = false }) catch return -3;

var count: u32 = 0;
while (count < 20) : (count += 1) {
    gpio.toggle(LED_PIN) catch {};
    arcs.sleep(500);
}
```

## 注意事项

- **板型依赖**: 当前引脚号和设备名依赖 `arcs_evb` 板级配置
- **移植调整**: 若切换到其他板型，需要同步调整 LED 对应设备和引脚
- **覆盖范围**: 示例主要验证 Zig GPIO HAL 的基础可用性，不覆盖输入、中断等高级模式
