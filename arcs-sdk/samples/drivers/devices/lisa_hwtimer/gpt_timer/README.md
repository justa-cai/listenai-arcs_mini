# LISA GPT HWTIMER 基础示例

本示例演示 GPT Timer 周期定时功能,每 500ms 触发一次回调。

## 硬件特性

- 8个独立通道,32位计数器
- 基准时钟: 100MHz
- 频率范围: 781.25kHz ~ 100MHz
- 支持单次和周期触发模式

## 配置说明

在 [prj.conf](prj.conf) 中启用以下配置:

```kconfig
CONFIG_LISA_DEVICE=y
CONFIG_LISA_HWTIMER_DEVICE=y
CONFIG_LISA_HWTIMER_ARCS_GPT_TIMER=y
```

## 关键代码

```c
// 1. 获取设备
lisa_device_t *hwtimer_dev = lisa_device_get("gpt_timer");

// 2. 设置频率为 100MHz
lisa_hwtimer_set_frequency(hwtimer_dev, 0, 100000000);

// 3. 注册回调 (在中断上下文执行)
lisa_hwtimer_set_callback(hwtimer_dev, 0, timer_callback, NULL);

// 4. 启动周期定时器 (500ms = 50000000 / 100000000)
lisa_hwtimer_start(hwtimer_dev, 0, 50000000, LISA_HWTIMER_MODE_PERIODIC);
```

**定时周期计算**: `周期(秒) = COUNT / 频率(Hz)`

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
=== LISA GPT HWTIMER basic example ===
gpt_timer device ready
Timer capabilities: channels=8, freq range=781250-100000000 Hz
Timer triggered: 1
Timer triggered: 2
...
```

定时器每 500ms 触发一次,打印递增计数。

