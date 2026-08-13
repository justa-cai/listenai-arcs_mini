# GPIO 基础输入示例

## 功能说明

演示如何使用 LISA GPIO 驱动读取引脚电平状态，通过配置引脚为输入模式并启用上拉电阻，周期性读取引脚的高低电平状态。

## 硬件连接

- **PA23**: GPIO 输入引脚（配置为上拉输入模式）

可通过外接按钮或开关到 PA23 引脚来改变输入电平状态。将引脚接地时读取到低电平，悬空或接高电平时读取到高电平。

## 示例步骤

1. 获取 GPIOA 设备
2. 配置 PA23 引脚为输入模式，启用上拉电阻
3. 周期性读取引脚电平状态（每秒读取一次）
4. 打印引脚电平状态（HIGH/LOW）

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

```
=== LISA GPIO input example ===
gpioa device ready
Start input monitoring...
PA23 level: HIGH
PA23 level: HIGH
PA23 level: LOW
PA23 level: LOW
PA23 level: HIGH
...
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 GPIO 设备 |
| `lisa_device_ready()` | 检查设备是否就绪 |
| `lisa_gpio_configure()` | 配置 GPIO 引脚模式和属性 |
| `lisa_gpio_read_pin()` | 读取 GPIO 引脚电平状态 |

## 关键代码

```c
/* 配置为输入模式，启用上拉 */
int ret = lisa_gpio_configure(gpio_dev, INPUT_PIN,
                              LISA_GPIO_INPUT | LISA_GPIO_PULL_UP);

/* 读取引脚电平 */
int ret = lisa_gpio_read_pin(gpio_dev, INPUT_PIN);
LISA_LOGI(LOG_TAG, "PA%d level: %s", INPUT_PIN,
          (ret == LISA_GPIO_HIGH) ? "HIGH" : "LOW");
```

## 配置说明

### GPIO 输入模式配置

- **`LISA_GPIO_INPUT`**: 配置引脚为输入模式
- **`LISA_GPIO_PULL_UP`**: 启用内部上拉电阻（引脚悬空时默认为高电平）
- **`LISA_GPIO_PULL_DOWN`**: 启用内部下拉电阻（引脚悬空时默认为低电平）

### 读取返回值

- **`LISA_GPIO_HIGH`**: 高电平（通常为 1）
- **`LISA_GPIO_LOW`**: 低电平（通常为 0）
- **负数**: 读取失败，返回错误码

## 注意事项

1. **引脚模式**：输入模式下引脚为高阻态，建议启用上拉或下拉电阻避免浮空
2. **读取返回值**：`lisa_gpio_read_pin()` 返回 `LISA_GPIO_HIGH` 或 `LISA_GPIO_LOW`，负数表示错误
3. **电压兼容**：确认外部信号电压与芯片 IO 电压兼容，避免超过额定电压
