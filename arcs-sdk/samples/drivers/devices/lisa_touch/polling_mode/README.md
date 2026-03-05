# LISA Touch 轮询模式示例

## 功能说明

本示例演示如何使用 LISA Touch 驱动的轮询模式读取触摸屏坐标。

在轮询模式下，主循环定期调用 `lisa_touch_read_event()` 读取触摸数据，不依赖 GPIO 中断，通过持续轮询检测触摸事件。实现简单，适合对实时性要求不高的应用。

## 硬件连接

- **PA22**: I2C SDA 引脚
- **PA23**: I2C SCL 引脚
- **PA25**: Touch 复位引脚（RST）
- **PA24**: Touch 中断引脚（INT，轮询模式下不使用）

## 示例步骤

1. 获取 Touch、I2C 和 GPIO 设备
2. 配置 I2C 和 GPIO 引脚复用（通过 pinmux 重定向）
3. 配置 Touch 总线接口
4. 设置轮询模式（`LISA_TOUCH_INT_MODE_POLLING`）
5. 启用 Touch 设备
6. 循环读取触摸事件并显示坐标（50ms 采样间隔）

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**

```
=== LISA Touch Polling Mode Example ===
I2C pins configured (SDA: PA22, SCL: PA23)
GPIO pins configured (RST: PA25, INT: PA24)
touch_cst328 device ready
i2c0 device ready
gpioa device ready
Touch bus attached successfully
Touch polling mode enabled
Touch device enabled
Touch capabilities: max_x=240, max_y=320, max_points=1

=== Polling Mode Active ===
Start reading touch events (polling every 50ms)...
Press the touch screen to see coordinates

[Polling] Touch: x= 120, y= 160, type=PRESS
[Polling] Touch: x= 121, y= 161, type=PRESS
[Polling] Touch: x= 125, y= 165, type=PRESS
[Polling] Touch: type=RELEASE
...
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 Touch、I2C 和 GPIO 设备 |
| `lisa_touch_attach_bus()` | 配置 Touch 总线接口（I2C、GPIO） |
| `lisa_touch_set_int_mode()` | 设置轮询模式 |
| `lisa_touch_enable()` | 启用 Touch 设备 |
| `lisa_touch_read_event()` | 读取触摸事件（坐标、状态） |

## 关键代码

### 设置轮询模式

```c
lisa_touch_set_int_mode(touch_dev, LISA_TOUCH_INT_MODE_POLLING);
```

此函数会禁用 GPIO 中断（如果之前已启用），确保设备处于轮询模式。

### 读取触摸事件

```c
lisa_touch_event_t event;
int ret = lisa_touch_read_event(touch_dev, &event);

if (ret == 0) {
    switch (event.type) {
        case LISA_TOUCH_EVENT_PRESS:
            /* 按下事件 */
            if (event.point_count > 0) {
                printf("[Polling] Touch: x=%4d, y=%4d, type=PRESS\n",
                       event.points[0].x, event.points[0].y);
            }
            break;
        case LISA_TOUCH_EVENT_RELEASE:
            /* 释放事件 */
            printf("[Polling] Touch: type=RELEASE\n");
            break;
    }
}
```

### 调整轮询间隔

```c
vTaskDelay(pdMS_TO_TICKS(50));  /* 50ms采样间隔 */
```

可以根据应用需求调整：
- **更快的响应**：减小延迟（如 20ms），但会增加 CPU 占用
- **更低的功耗**：增大延迟（如 100ms），但响应会变慢

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

## 轮询模式说明

### 工作原理

- **实现方式**：主循环定期调用 `lisa_touch_read_event()` 读取触摸数据
- **无需中断**：不依赖 GPIO 中断，适合没有中断引脚或中断资源紧张的场景
- **可控采样率**：可以灵活调整轮询间隔（本示例为 50ms）
- **线程安全**：驱动内部使用互斥锁保护，确保多线程环境下的数据一致性

### 事件类型行为

驱动检测到触摸状态并返回相应的事件类型：

| 触摸操作 | 返回事件类型 | 说明 |
|---------|------------|------|
| 按下 | `LISA_TOUCH_EVENT_PRESS` | 检测到触摸时返回，包含坐标信息 |
| 释放 | `LISA_TOUCH_EVENT_RELEASE` | 检测到释放时返回 |
| 无触摸 | `LISA_TOUCH_EVENT_NONE` | 无触摸事件 |

**说明**：
- 轮询模式下，每次读取时如果触摸状态为按下，都会返回 `PRESS` 事件
- 如果手指在滑动，坐标会在每次 `PRESS` 事件中更新
- 检测到释放时返回 `RELEASE` 事件

**示例输出序列**（50ms 轮询间隔，手指持续按下 200ms 后抬起）：
```
[Polling] Touch: x= 120, y= 160, type=PRESS      // 0ms: 检测到按下
[Polling] Touch: x= 120, y= 160, type=PRESS      // 50ms: 持续按下（坐标未变）
[Polling] Touch: x= 121, y= 161, type=PRESS      // 100ms: 持续按下（坐标更新）
[Polling] Touch: x= 125, y= 165, type=PRESS      // 150ms: 持续按下（滑动，坐标更新）
[Polling] Touch: type=RELEASE                    // 200ms: 检测到释放
```

## 注意事项

1. **模式互斥**：轮询模式和中断模式是互斥的
   - 在轮询模式下，使用 `lisa_touch_read_event()` 主动读取触摸事件
   - 如需切换到中断模式，请先调用 `lisa_touch_set_int_mode(LISA_TOUCH_INT_MODE_INTERRUPT)` 并设置回调函数
   - ⚠️ 设置为中断模式后，不能再调用 `lisa_touch_read_event()`，否则返回错误

2. **轮询间隔**：间隔太短会导致 CPU 占用高、功耗大；间隔太长会导致响应延迟大，可能丢失快速触摸。建议范围：20ms ~ 100ms

3. **线程安全**：驱动内部使用互斥锁保护，可以在多线程环境中安全使用

4. **引脚配置**：示例中通过 pinmux 重定向自定义引脚配置，会覆盖 `boards/arcs_evb/pinmux.c` 中的默认实现

5. **I2C 配置**：I2C 总线在 `attach_bus` 时自动配置（标准速度 100kHz）

6. **性能考虑**：如果对实时性和功耗有要求，建议使用中断模式
