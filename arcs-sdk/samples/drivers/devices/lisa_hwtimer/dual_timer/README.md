# LISA DUAL HWTIMER 基础示例

本示例演示 Dual Timer 周期定时功能,每 500ms 触发一次回调。

## 硬件特性

- 2个独立通道,32位计数器
- 基准时钟: 16kHz
- 支持分频: 1/16/256
- 频率范围: 62Hz ~ 16kHz

## 配置说明

在 [prj.conf](prj.conf) 中启用以下配置:

```kconfig
CONFIG_LISA_DEVICE=y
CONFIG_LISA_HWTIMER_DEVICE=y
CONFIG_LISA_HWTIMER_ARCS_DUAL_TIMER=y
```

## 关键代码

```c
// 1. 获取设备
lisa_device_t *hwtimer_dev = lisa_device_get("dual_timer");

// 2. 设置频率为 16kHz
lisa_hwtimer_set_frequency(hwtimer_dev, 0, 16000);

// 3. 注册回调 (在中断上下文执行)
lisa_hwtimer_set_callback(hwtimer_dev, 0, timer_callback, NULL);

// 4. 启动周期定时器 (500ms = 8000 / 16000)
lisa_hwtimer_start(hwtimer_dev, 0, 8000, LISA_HWTIMER_MODE_PERIODIC);
```

**定时周期计算**: `周期(秒) = COUNT / 频率(Hz)`

例如: `500ms = 8000 / 16000 Hz`

## 编译

```{eval-rst}
.. include:: /sample_build.rst
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

定时器每 500ms 触发一次,打印递增计数。
