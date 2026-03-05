# LISA GPIO 基础输出示例

## 功能说明

演示如何使用 LISA GPIO 驱动控制 LED 闪烁，通过配置引脚为输出模式，周期性输出高低电平实现 LED 的亮灭切换。

## 硬件连接

- **PB09**: LED 控制引脚（外接 LED 与限流电阻，ARCS-EVB 板载接口）

将 LED 正极（长脚）通过限流电阻（推荐 220Ω-1kΩ）连接到 PB09 引脚，负极（短脚）连接到 GND。

## 示例步骤

1. 获取 GPIOB 设备
2. 配置 PB09 引脚为输出模式，初始电平为低
3. 循环切换引脚输出电平（高/低）
4. 每次切换间隔 1 秒，实现 LED 闪烁效果

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
=== LISA GPIO output example ===
gpiob device ready
Start toggling LED...
LED is ON
LED is OFF
LED is ON
LED is OFF
...
```

**LED 状态：**
- LED 每秒切换一次亮灭状态
- 亮时对应高电平输出，灭时对应低电平输出

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 GPIO 设备 |
| `lisa_device_ready()` | 检查设备是否就绪 |
| `lisa_gpio_configure()` | 配置 GPIO 引脚模式和属性 |
| `lisa_gpio_write_pin()` | 设置 GPIO 引脚输出电平 |

## 关键代码

```c
/* 配置为输出模式，初始电平为低 */
int ret = lisa_gpio_configure(gpio_dev, LED_PIN,
                              LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_LOW);

/* 循环切换 LED 状态 */
bool led_on = true;
while (1) {
    lisa_gpio_write_pin(gpio_dev, LED_PIN,
                        led_on ? LISA_GPIO_HIGH : LISA_GPIO_LOW);
    LISA_LOGI(LOG_TAG, "LED is %s", led_on ? "ON" : "OFF");
    vTaskDelay(pdMS_TO_TICKS(1000));
    led_on = !led_on;
}
```

## 配置说明

### GPIO 输出模式配置

- **`LISA_GPIO_OUTPUT`**: 配置引脚为输出模式
- **`LISA_GPIO_OUTPUT_INIT_LOW`**: 输出初始电平为低电平
- **`LISA_GPIO_OUTPUT_INIT_HIGH`**: 输出初始电平为高电平

### 输出电平控制

- **`LISA_GPIO_HIGH`**: 输出高电平（LED 点亮）
- **`LISA_GPIO_LOW`**: 输出低电平（LED 熄灭）

## 注意事项

1. **限流电阻**：外接 LED 时必须串联限流电阻，防止电流过大损坏引脚或 LED
2. **电流能力**：GPIO 引脚输出电流有限（通常为 4-8mA），确保 LED 工作电流在允许范围内
3. **电压兼容**：确认 LED 的正向电压与引脚输出电压兼容
