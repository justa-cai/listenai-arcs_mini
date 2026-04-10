# LISA HWTIMER AON Timer 周期定时示例

## 功能说明

本示例演示 AON Timer 低功耗周期定时功能，每 1 秒触发一次回调，支持 RC32K 和 XO32K 两种时钟源。

## 硬件连接

无需外部连接。AON Timer 为芯片内部外设，默认使用内部 RC32K 时钟源；如需使用 XO32K 时钟源，需要外接 32K 晶振。

## 示例步骤

1. 获取 `aon_timer` 设备并检查设备就绪状态
2. 查询设备能力，获取定时器通道数和频率范围
3. 设置定时器频率为时钟源最大频率
4. 注册定时器中断回调函数
5. 启动周期定时器，设置周期为 1 秒
6. 定时器每 1 秒触发一次回调，打印递增计数

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
lisa_device_t *hwtimer_dev = lisa_device_get("aon_timer");

/* 查询实际频率 */
lisa_hwtimer_capabilities_t caps;
lisa_hwtimer_get_capabilities(hwtimer_dev, &caps);

/* 设置频率 */
lisa_hwtimer_set_frequency(hwtimer_dev, 0, caps.max_freq_hz);

/* 注册回调 */
lisa_hwtimer_set_callback(hwtimer_dev, 0, timer_callback, NULL);

/* 启动周期定时器（1秒） */
lisa_hwtimer_start(hwtimer_dev, 0, caps.max_freq_hz, LISA_HWTIMER_MODE_PERIODIC);
```

## 配置说明

```kconfig
CONFIG_LISA_DEVICE=y
CONFIG_LISA_HWTIMER_DEVICE=y
CONFIG_LISA_HWTIMER_ARCS_AON_TIMER=y
# 可选：使用外部 XO32K 时钟源（需要外接32K晶振）
# CONFIG_LISA_HWTIMER_ARCS_AON_TIMER_CLK_XO32K=y
```

## 注意事项

1. **时钟源精度**：使用 RC32K 时实际频率可能与标称值有偏差，建议通过 `lisa_hwtimer_get_capabilities()` 查询实际频率
2. **回调上下文**：定时器回调在中断上下文中执行，应保持简短快速，避免阻塞操作
3. **低功耗**：AON Timer 支持低功耗模式运行，适合长时间定时和低功耗应用场景
