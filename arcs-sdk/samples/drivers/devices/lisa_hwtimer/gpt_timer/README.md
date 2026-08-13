# HWTIMER GPT Timer 周期定时示例

## 功能说明

本示例演示 GPT Timer 周期定时功能，每 500ms 触发一次回调。ARCS 示例使用 100MHz 计数频率；Venusa 示例使用 10kHz 计数频率，使 500ms 周期对应 5000 count，以适配 16-bit GPT timer 的 count 上限。10kHz 只是示例值，驱动会按 `freq_hz` 选择可精确表示的 PCLK/T0 分频配置。

## 硬件连接

无需外部连接。GPT Timer 为芯片内部外设；ARCS 具有 8 个独立 timer 通道和 32 位计数器，Venusa 当前 hwtimer 适配 GPT channel 0-1 两个 16-bit timer。Venusa PWM 模式下同一 GPT 硬件的每个 channel 有 4 个 PWM port，总计可形成 8 路 PWM 输出，但这不是 hwtimer 的独立 timer 通道。

## 示例步骤

1. 获取 `gpt_timer` 设备并检查设备就绪状态
2. 查询设备能力，获取定时器通道数和频率范围
3. 设置定时器频率为平台默认示例频率（ARCS 100MHz，Venusa 10kHz）
4. 注册定时器中断回调函数
5. 启动周期定时器，设置周期为 500ms
6. 定时器每 500ms 触发一次回调，打印递增计数

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```


ARCS 默认构建使用 `prj.conf`，保留历史配置名 `CONFIG_LISA_HWTIMER_ARCS_GPT_TIMER=y`；Venusa 构建时由 `CMakeLists.txt` 自动切换到 `prj_venusa.conf`，无需额外传配置参数。

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出
Arcs 平台预期能力和启动参数示例：
```
=== LISA GPT HWTIMER basic example ===
gpt_timer device ready
Timer capabilities: channels=8, freq range=781250-100000000 Hz
Set timer frequency: 100000000 Hz
Timer started with count=50000000 (period=500.0ms)
Timer triggered: 1
Timer triggered: 2
...
```

Venusa 平台预期能力和启动参数示例：

```
=== LISA GPT HWTIMER basic example ===
gpt_timer device ready
Timer capabilities: channels=2, freq range=2-100000000 Hz
Set timer frequency: 10000 Hz
Timer started with count=5000 (period=500.0ms)
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
lisa_device_t *hwtimer_dev = lisa_device_get("gpt_timer");

/* 设置频率：ARCS 为 100MHz，Venusa 为 10kHz */
lisa_hwtimer_set_frequency(hwtimer_dev, 0, TIMER_FREQ_HZ);

/* 注册回调 */
lisa_hwtimer_set_callback(hwtimer_dev, 0, timer_callback, NULL);

/* 启动周期定时器（500ms = TIMER_COUNT / TIMER_FREQ_HZ） */
lisa_hwtimer_start(hwtimer_dev, 0, TIMER_COUNT, LISA_HWTIMER_MODE_PERIODIC);
```

## 注意事项

1. **定时周期计算**：周期(秒) = COUNT / 频率(Hz)，例如 ARCS 500ms = 50000000 / 100000000，Venusa 500ms = 5000 / 10000
2. **回调上下文**：定时器回调在中断上下文中执行，应保持简短快速
3. **通道数量**：ARCS 支持 8 个 GPT Timer 通道；Venusa hwtimer 支持 GPT channel 0-1 两个 16-bit timer，count 范围为 1-65535。Venusa PWM 的 8 路输出来自 2 个 GPT channel × 4 个 PWM port，不等同于 8 个独立 timer 通道
4. **GPT/PWM 复用**：Venusa GPT timer 与 PWM 复用 GPT channel。某个 GPT channel 已作为 hwtimer 运行时，不应同时作为 PWM group 使用；PWM 逻辑 channel 0-3 属于 GPT channel 0，逻辑 channel 4-7 属于 GPT channel 1
