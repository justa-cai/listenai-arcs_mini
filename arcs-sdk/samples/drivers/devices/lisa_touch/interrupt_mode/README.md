# LISA Touch 中断模式示例

## 功能说明

本示例演示如何使用 LISA Touch 驱动的中断模式读取触摸屏坐标。

在中断模式下，GPIO 中断引脚（INT）配置为上升沿触发，当触摸事件发生时芯片会拉高 INT 引脚。驱动内部创建专用任务处理 I2C 读取，中断回调只发送任务通知，用户回调在任务上下文中调用，可以安全使用 `printf` 等操作。

## 新特性

- **快速响应**：触摸事件立即通过中断响应，无需轮询，延迟通常 < 5ms
- **低功耗**：CPU 不需要持续轮询，可以进入低功耗模式
- **低 CPU 占用**：只在有触摸事件时才处理，事件驱动模式
- **安全可靠**：I2C 读取在任务上下文中执行，不会阻塞中断

## 硬件连接

- **PA22**: I2C SDA 引脚
- **PA23**: I2C SCL 引脚
- **PA25**: Touch 复位引脚（RST）
- **PA24**: Touch 中断引脚（INT）

## 使用场景

适用于需要快速响应触摸事件、低功耗的应用场景。底层采用任务通知 + 内部任务的设计，中断回调快速返回，I2C 读取在任务上下文中安全执行。

## 示例步骤

1. 获取 Touch、I2C 和 GPIO 设备
2. 配置 I2C 和 GPIO 引脚复用（通过 pinmux 重定向）
3. 配置 Touch 总线接口
4. 设置触摸事件回调函数
5. 启用中断模式（`LISA_TOUCH_INT_MODE_INTERRUPT`）
6. 启用 Touch 设备
7. 触摸事件通过中断自动处理，回调在任务上下文中调用

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**

```
=== LISA Touch Interrupt Mode Example ===
I2C pins configured (SDA: PA22, SCL: PA23)
GPIO pins configured (RST: PA25, INT: PA24)
touch_cst328 device ready
i2c0 device ready
gpioa device ready
Touch bus attached successfully
Touch callback set successfully
Touch interrupt mode enabled
Touch device enabled
Touch capabilities: max_x=240, max_y=320, max_points=1

=== Interrupt Mode Active ===
Touch events will be reported via callback when GPIO interrupt triggers
Press the touch screen to see coordinates in callback
(Callback is called from driver's internal task, not from ISR)

[Main Loop] System running, waiting for touch events...
[Callback] Touch: x= 120, y= 160, type=PRESS
[Callback] Touch: x= 121, y= 161, type=PRESS
[Callback] Touch: x= 125, y= 165, type=PRESS
[Callback] Touch: type=RELEASE
[Main Loop] System running, waiting for touch events...
...
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 Touch、I2C 和 GPIO 设备 |
| `lisa_touch_attach_bus()` | 配置 Touch 总线接口（I2C、GPIO） |
| `lisa_touch_set_callback()` | 设置触摸事件回调函数 |
| `lisa_touch_set_int_mode()` | 设置中断模式 |
| `lisa_touch_enable()` | 启用 Touch 设备 |

## 中断模式说明

### 工作原理

`lisa_touch_set_int_mode(INTERRUPT)` 在中断模式下的工作原理：

- **驱动内部任务**：创建专用任务等待触摸中断通知
- **中断回调（ISR 上下文）**：只发送任务通知（`vTaskNotifyGiveFromISR`），执行时间 < 1μs，快速返回
- **任务上下文读取**：被中断唤醒后，在任务上下文中读取 I2C 数据，可以安全使用阻塞 API
- **用户回调**：读取完成后在任务上下文中调用用户回调，可以安全使用 `printf`、`malloc` 等操作

### 事件类型行为

驱动检测到触摸状态并返回相应的事件类型：

| 触摸操作 | 返回事件类型 | 触发时机 |
|---------|------------|---------|
| 按下 | `LISA_TOUCH_EVENT_PRESS` | INT 引脚产生中断，检测到触摸，包含坐标信息 |
| 释放 | `LISA_TOUCH_EVENT_RELEASE` | INT 引脚产生中断，检测到释放 |

**说明**：
- 中断模式下，每次 INT 引脚产生中断时，如果检测到触摸，都会返回 `PRESS` 事件
- 如果手指在滑动，坐标会在每次 `PRESS` 事件中更新
- 检测到释放时返回 `RELEASE` 事件

**示例事件序列**（手指按下后滑动，然后抬起）：
```
[Callback] Touch: x= 120, y= 160, type=PRESS      // 按下中断
[Callback] Touch: x= 121, y= 161, type=PRESS      // 滑动中断（坐标更新）
[Callback] Touch: x= 125, y= 165, type=PRESS      // 滑动中断（坐标更新）
[Callback] Touch: type=RELEASE                    // 释放中断
```

**注意**：中断模式下，事件的频率取决于触摸芯片的中断生成频率，通常比轮询模式更快更实时。

## 关键代码

### 设置回调函数

```c
static void touch_event_callback(const lisa_touch_event_t *event, void *user_data)
{
    switch (event->type) {
        case LISA_TOUCH_EVENT_PRESS:
            /* 按下事件 */
            if (event->point_count > 0) {
                printf("[Callback] Touch: x=%4d, y=%4d, type=PRESS\n",
                       event->points[0].x, event->points[0].y);
            }
            break;
        case LISA_TOUCH_EVENT_RELEASE:
            /* 释放事件 */
            printf("[Callback] Touch: type=RELEASE\n");
            break;
    }
}

lisa_touch_set_callback(touch_dev, touch_event_callback, NULL);
```

回调函数会在**任务上下文中**被调用（由驱动内部任务触发），可以：
- 安全使用 `printf`、`malloc` 等操作
- 执行耗时操作
- 调用其他阻塞 API

### 启用中断模式

```c
lisa_touch_set_int_mode(touch_dev, LISA_TOUCH_INT_MODE_INTERRUPT);
```

此函数会：
- 创建驱动内部读取任务（如果不存在）
- 配置 GPIO 中断为上升沿触发
- 注册中断回调函数
- 启用 GPIO 中断

## 工作流程

```
用户触摸屏幕
  ↓
[硬件中断] INT 引脚上升沿
  ↓
[ISR] cst328_gpio_irq_callback()  (< 1μs)
  ├─ vTaskNotifyGiveFromISR()     (发送任务通知)
  └─ portYIELD_FROM_ISR()          (触发任务切换)
  ↓
[任务切换] 调度器唤醒驱动内部任务
  ↓
[任务上下文] cst328_read_task
  ├─ ulTaskNotifyTake()            (收到通知，继续执行)
  ├─ cst328_touch_read_event_internal()
  │   ├─ DEVICE_LOCK()             (获取互斥锁)
  │   ├─ cst328_read_reg()         (I2C 读取，安全阻塞)
  │   ├─ 解析触摸数据
  │   └─ DEVICE_UNLOCK()           (释放互斥锁)
  └─ priv->callback()              (调用用户回调)
      └─ touch_event_callback()     (用户代码)
          └─ printf()               (安全，在任务上下文)
  ↓
[任务上下文] cst328_read_task
  └─ ulTaskNotifyTake()             (再次阻塞，等待下次中断)
```

## 配置说明

在 `prj.conf` 中启用相关驱动：

```ini
CONFIG_LISA_DEVICE=y
CONFIG_LISA_TOUCH_DEVICE=y
CONFIG_LISA_TOUCH_ARCS_CST328=y    # 启用 CST328 芯片
CONFIG_LISA_I2C=y
CONFIG_LISA_I2C0=y
CONFIG_LISA_GPIO_DEVICE=y
CONFIG_LISA_GPIOA=y
```

## 注意事项

1. **禁止混用模式**：⚠️ **中断模式下不能调用 `lisa_touch_read_event()`**
   - 设置为中断模式后，必须使用回调函数接收触摸事件
   - 如果在中断模式下调用 `lisa_touch_read_event()`，将返回 `LISA_DEVICE_ERR_NOT_SUPPORT` 错误
   - 原因：避免轮询读取与中断任务产生资源竞争，导致状态混乱和重复事件
   - 如需切换回轮询模式，请先调用 `lisa_touch_set_int_mode(LISA_TOUCH_INT_MODE_POLLING)`

2. **回调函数安全**：回调函数在任务上下文中调用，可以安全使用各种 API。可以执行耗时操作，但建议保持简短以保持响应性

2. **任务优先级**：驱动内部任务优先级为 `tskIDLE_PRIORITY + 2`，可以通过 Kconfig 调整优先级

3. **引脚配置**：示例中通过 pinmux 重定向自定义引脚配置，会覆盖 `boards/arcs_evb/pinmux.c` 中的默认实现

4. **I2C 操作**：I2C 读取在任务上下文中执行，完全安全

5. **去重处理**：示例代码中实现了 RELEASE 事件去重，避免重复输出
