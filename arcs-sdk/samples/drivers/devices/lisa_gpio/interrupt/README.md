# LISA GPIO 中断示例

## 功能说明

演示如何使用 GPIO 中断功能实现事件驱动的引脚状态检测，通过配置下降沿中断，在引脚电平由高变低时自动触发中断回调，无需 CPU 轮询，降低功耗。

### 新特性

- **事件驱动**：引脚状态变化时自动触发中断，无需主动轮询
- **低功耗**：CPU 可进入休眠状态，等待中断唤醒
- **实时响应**：硬件中断响应速度快，适合对时序敏感的场景
- **多种触发模式**：支持边沿触发和电平触发，满足不同应用需求

## 硬件连接

- **PA23**: GPIO 输入引脚（配置为上拉输入，下降沿中断）

可通过外接按钮连接 PA23 引脚到 GND，按下按钮时引脚电平由高变低，触发下降沿中断。

## 使用场景

适用于需要实时响应外部事件的场景（如按键检测、传感器信号检测等），通过中断方式可以避免轮询带来的 CPU 占用，同时提高响应速度。

## 示例步骤

1. 获取 GPIOA 设备
2. 配置 PA23 引脚为输入模式，启用上拉电阻
3. 配置下降沿中断，设置中断回调函数
4. 使能 PA23 引脚中断
5. 等待中断触发（主循环进入低功耗等待）
6. 中断触发时执行回调函数，打印日志并禁用中断

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
=== LISA GPIO interrupt example ===
Waiting for interrupt...
Waiting for interrupt...
Interrupt triggered on gpioa pin 23
Waiting for interrupt...
Waiting for interrupt...
...
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 GPIO 设备 |
| `lisa_device_ready()` | 检查设备是否就绪 |
| `lisa_gpio_configure()` | 配置 GPIO 引脚模式和属性 |
| `lisa_gpio_configure_irq()` | 配置 GPIO 中断模式和回调函数 |
| `lisa_gpio_enable_irq()` | 使能 GPIO 引脚中断 |
| `lisa_gpio_disable_irq()` | 禁用 GPIO 引脚中断 |

## 中断模式说明

GPIO 中断支持多种触发模式，可根据应用场景选择：

### 边沿触发模式

- **`LISA_GPIO_IRQ_EDGE_RISING`**: 上升沿触发（低电平 → 高电平）
- **`LISA_GPIO_IRQ_EDGE_FALLING`**: 下降沿触发（高电平 → 低电平）
- **`LISA_GPIO_IRQ_EDGE_BOTH`**: 双边沿触发（任意电平变化）

### 电平触发模式

- **`LISA_GPIO_IRQ_LEVEL_HIGH`**: 高电平触发（引脚为高电平时持续触发）
- **`LISA_GPIO_IRQ_LEVEL_LOW`**: 低电平触发（引脚为低电平时持续触发）

**推荐使用边沿触发模式**，避免电平触发模式下的重复中断。

## 关键代码

```c
/* 中断回调函数 */
static void button_irq_callback(uint32_t pin, void *user_data)
{
    lisa_device_t *gpio_dev = (lisa_device_t *)user_data;
    LISA_LOGI(LOG_TAG, "Interrupt triggered on %s pin %d", gpio_dev->name, pin);

    /* 处理完中断后禁用中断，防止重复触发 */
    lisa_gpio_disable_irq(gpio_dev, pin);
}

/* 配置为输入模式，启用上拉 */
int ret = lisa_gpio_configure(gpio_dev, GPIO_PIN,
                              LISA_GPIO_INPUT | LISA_GPIO_PULL_UP);

/* 配置下降沿中断 */
ret = lisa_gpio_configure_irq(gpio_dev, GPIO_PIN,
                              LISA_GPIO_IRQ_EDGE_FALLING,
                              button_irq_callback,
                              gpio_dev);  /* 传递设备指针作为 user_data */

/* 使能中断 */
ret = lisa_gpio_enable_irq(gpio_dev, GPIO_PIN);
```

## 配置说明

### 中断回调函数

中断回调函数原型：
```c
void callback(uint32_t pin, void *user_data);
```

- **`pin`**: 触发中断的引脚编号
- **`user_data`**: 用户自定义数据指针，在配置中断时传入

### 用户数据传递

可通过 `user_data` 参数传递上下文信息（如设备指针、状态变量等）：

```c
lisa_gpio_configure_irq(gpio_dev, GPIO_PIN,
                       LISA_GPIO_IRQ_EDGE_FALLING,
                       button_irq_callback,
                       gpio_dev);  /* 传递设备指针 */
```

## 注意事项

1. **回调上下文**：中断回调函数在中断上下文中执行，应尽快返回，避免耗时操作
2. **中断嵌套**：避免在回调函数中执行可能引发中断嵌套的操作
3. **禁用中断**：示例中在回调函数中禁用中断，防止重复触发。实际应用中可根据需求决定是否禁用
4. **线程安全**：如需在回调中访问共享资源，注意使用合适的同步机制
5. **去抖动**：按键等机械触点可能产生抖动，建议在应用层实现软件去抖动
