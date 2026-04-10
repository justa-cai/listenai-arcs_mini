# LISA HWTIMER Dual Timer 周期定时示例

## 功能说明

本示例演示 Dual Timer 周期定时功能，配置 16kHz 基准时钟，每 500ms 触发一次回调。

## 硬件连接

无需外部连接。Dual Timer 为芯片内部外设，具有 2 个独立通道和 32 位计数器，支持 1/16/256 分频。

## 示例步骤

1. 获取 `dual_timer` 设备并检查设备就绪状态
2. 查询设备能力，获取定时器通道数和频率范围
3. 设置定时器频率为 16kHz
4. 注册定时器中断回调函数
5. 启动周期定时器，设置周期为 500ms
6. 定时器每 500ms 触发一次回调，打印递增计数

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
=== LISA DUAL HWTIMER basic example ===
dual_timer device ready
Timer capabilities: channels=2, freq range=62 -16000 Hz
Set timer frequency: 16000 Hz
Timer started with count=8000 (period=500.0ms)
Timer triggered: 1
Timer triggered: 2
...
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取定时器设备 |
| `lisa_hwtimer_get_capabilities()` | 查询定时器能力（通道数、频率范围） |
| `lisa_hwtimer_set_frequency()` | 设置定时器频率 |
| `lisa_hwtimer_set_callback()` | 注册定时器中断回调函数 |
| `lisa_hwtimer_start()` | 启动定时器（支持单次和周期模式） |

## 关键代码

```c
/* 获取设备 */
lisa_device_t *hwtimer_dev = lisa_device_get("dual_timer");

/* 设置频率为 16kHz */
lisa_hwtimer_set_frequency(hwtimer_dev, 0, 16000);

/* 注册回调 */
lisa_hwtimer_set_callback(hwtimer_dev, 0, timer_callback, NULL);

/* 启动周期定时器（500ms = 8000 / 16000） */
lisa_hwtimer_start(hwtimer_dev, 0, 8000, LISA_HWTIMER_MODE_PERIODIC);
```

## 注意事项

1. **定时周期计算**：周期(秒) = COUNT / 频率(Hz)，例如 500ms = 8000 / 16000
2. **回调上下文**：定时器回调在中断上下文中执行，应保持简短快速
3. **分频设置**：支持 1/16/256 分频，频率范围为 62Hz ~ 16kHz
