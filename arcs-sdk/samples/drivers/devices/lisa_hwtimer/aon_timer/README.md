# LISA AON HWTIMER 基础示例

本示例演示 AON Timer 低功耗周期定时功能，每 1 秒触发一次回调。

## 硬件特性

- **通道数量**: 1个通道（通道0）
- **计数器位数**: 24位计数器
- **时钟源**:
  - **RC32K**: 内部RC振荡器，频率动态获取（典型值约32768Hz，实际存在较大偏差）
  - **XO32K**: 需要外接32K晶振，典型值32768Hz，精度高
- **分频**: 不支持分频，频率由时钟源决定
- **低功耗**: 支持低功耗模式运行

## 配置说明

在 [prj.conf](prj.conf) 中启用以下配置:

```kconfig
CONFIG_LISA_DEVICE=y
CONFIG_LISA_HWTIMER_DEVICE=y
CONFIG_LISA_HWTIMER_ARCS_AON_TIMER=y
```

### 时钟源配置

默认使用内部 RC32K 时钟源（`CONFIG_LISA_HWTIMER_ARCS_AON_TIMER_CLK_RC32K=y`）：
- 无需外部晶振硬件
- 功耗更低
- 频率通过 `lisa_hwtimer_get_capabilities()` 动态获取
- 精度较低，有偏差

可选配置使用外部 XO32K 时钟源（需要外接32K晶振硬件）:

```kconfig
CONFIG_LISA_HWTIMER_ARCS_AON_TIMER_CLK_XO32K=y
```


## 关键代码

```c
// 1. 获取设备
lisa_device_t *hwtimer_dev = lisa_device_get("aon_timer");
if (!lisa_device_ready(hwtimer_dev)) {
    // 设备未就绪，处理错误
    return -1;
}

// 2. 获取设备能力（查询实际频率）
lisa_hwtimer_capabilities_t caps;
int ret = lisa_hwtimer_get_capabilities(hwtimer_dev, &caps);
// RC32K: 通过CRM动态获取实际频率（可能与32768Hz有偏差）
// XO32K: 固定32768Hz

// 3. 设置频率
ret = lisa_hwtimer_set_frequency(hwtimer_dev, 0, caps.max_freq_hz);

// 4. 注册回调（在中断上下文执行）
ret = lisa_hwtimer_set_callback(hwtimer_dev, 0, timer_callback, NULL);

// 5. 启动周期定时器（1秒 = caps.max_freq_hz / caps.max_freq_hz）
uint32_t count = caps.max_freq_hz;
ret = lisa_hwtimer_start(hwtimer_dev, 0, count, LISA_HWTIMER_MODE_PERIODIC);
```

### 定时周期计算

```
周期(秒) = COUNT / 频率(Hz)
```

**本示例**: `1秒 = caps.max_freq_hz / caps.max_freq_hz`

**其他示例**:
- 500ms: `count = caps.max_freq_hz / 2`
- 100ms: `count = caps.max_freq_hz / 10`
- 10ms: `count = caps.max_freq_hz / 100`

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
=== LISA AON HWTIMER basic example ===
aon_timer device ready
Timer capabilities: channels=1, freq range=32768-32768 Hz
Set timer frequency: 32768 Hz
Timer started with count=32768 (period=1000.0ms)
Timer triggered: 1
Timer triggered: 2
Timer triggered: 3
...
```

定时器每 1 秒触发一次，打印递增计数。

## 注意事项

1. **时钟源精度**:
   - 使用 RC32K 时实际频率可能与标称值有偏差，建议通过 `get_capabilities` 查询实际频率
   - 需要精确计时场景建议使用 XO32K 时钟源

2. **回调执行上下文**: 定时器回调在中断上下文中执行，应保持简短快速，避免阻塞操作

3. **功耗考虑**: AON Timer 支持低功耗模式，适合长时间定时和低功耗应用场景

