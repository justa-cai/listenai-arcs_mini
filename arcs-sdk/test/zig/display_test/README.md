# Zig Display Test 示例

## 功能说明

该示例演示如何在 C 侧完成板级显示总线初始化后，在 Zig 中通过 `arcs.Display` 读取显示能力、开屏、调节亮度并对整屏进行填色。它适合作为 Zig 访问显示设备和 panel 相关能力的验证样例。

## 硬件连接

该示例面向 `arcs_evb` + `ST7789P3` SPI 屏，依赖以下板级资源：

- `display` 设备
- `spi1`
- `gpiob`（CS / DC 等控制引脚）
- `gpioa`（复位引脚）
- `pwm0`（背光）

C 侧 `main.c` 会先完成 `lisa_display_attach_bus()` 配置，再调用 Zig 导出的测试函数。

## 示例步骤

1. C 侧获取 `display` 设备并配置 SPI 4-wire 总线
2. 配置 panel 为 `st7789p3`
3. 设置背光为 PWM 模式
4. 调用 Zig 导出的 `zig_display_test()`
5. Zig 侧读取能力信息、开屏并设置亮度
6. 依次填充红、绿、蓝三色
7. 执行亮度渐变测试并输出 PASS 日志

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

建议将示例路径替换为：

```bash
./build.sh -C -S test/zig/display_test -DBOARD=arcs_evb
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

串口中可看到类似日志：

```text
=== Zig Display Test ===
Display device found
Display bus attached (SPI 4-wire, ST7789P3)
Calling zig_display_test()...
Display test starting...
[PASS] display: device opened
[PASS] display: capabilities 240x320
[PASS] display: blanking off (screen on)
[PASS] display: brightness set to 80%
Filling screen with RED...
Filling screen with GREEN...
Filling screen with BLUE...
[PASS] display: brightness ramp 0->100
=== Zig Display Test PASSED ===
```

屏幕侧应看到全屏依次显示红、绿、蓝、白等颜色变化，并伴随亮度渐变。

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_display_attach_bus()` | C 侧绑定显示总线和 panel |
| `arcs.Display.open()` | Zig 侧打开显示设备 |
| `lcd.getCapabilities()` | 获取屏幕分辨率等能力 |
| `lcd.on()` | 开启显示输出 |
| `lcd.setBrightness()` | 设置背光亮度 |
| `lcd.fillRect()` | 对矩形区域填充颜色 |

## 关键代码

```c
lisa_display_config_t display_config = {
    .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
    .panel_name = "st7789p3",
    ...
};

int ret = lisa_display_attach_bus(display_device, &display_config);
```

```zig
const caps = try lcd.getCapabilities();
try lcd.on();
try lcd.setBrightness(80);
try lcd.fillRect(0, 0, caps.width, caps.height, arcs.Display.Color.red);
```

## 注意事项

- **板级初始化**: 该示例依赖较强的板级初始化，若切换屏幕、panel 或背光方案，需要同步修改 C 侧配置
- **配置要求**: `prj.conf` 中已启用显示设备、ST7789P3 panel、SPI 4-wire 和 PWM 等相关配置
- **一致性检查**: 如果显示设备名、panel 名称或引脚映射与板级实现不一致，`attach_bus` 或 Zig 侧操作会直接失败
