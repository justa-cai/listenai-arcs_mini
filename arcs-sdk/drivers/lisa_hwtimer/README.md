# HWTIMER 驱动

基于 lisa_device 框架的硬件定时器设备驱动,为 ARCS / Venusa 平台提供统一的硬件定时器操作接口。

## 功能特性

- **设备支持**: 支持三种硬件定时器
  - GPT Timer: ARCS 8通道高性能定时器(支持32bit)；Venusa 2通道 16-bit GPT timer
  - AON Timer: 1通道低功耗定时器
  - Dual Timer: 2通道通用定时器
- **工作模式**: 支持单次触发和周期触发模式
- **灵活频率**: 支持多种预分频配置
- **中断回调**: 支持定时器超时回调机制
- **计数查询**: 支持实时读取当前计数值

## 配置选项

在平台对应的配置文件中启用驱动。ARCS 保留历史配置名以兼容已有工程，Venusa 使用独立配置名；二者都会在内部选择通用 timer 能力。

ARCS `prj.conf` 示例：

```kconfig
CONFIG_LISA_HWTIMER_DEVICE=y

# 启用 GPT Timer (8通道)
CONFIG_LISA_HWTIMER_ARCS_GPT_TIMER=y

# 启用 AON Timer (1通道低功耗)
CONFIG_LISA_HWTIMER_ARCS_AON_TIMER=y
# 时钟源选择(二选一):
# CONFIG_LISA_HWTIMER_ARCS_AON_TIMER_CLK_RC32K=y  # 内部RC32K时钟源(默认,精度较低,通过lisa_hwtimer_get_capabilities动态获取频率)
# CONFIG_LISA_HWTIMER_ARCS_AON_TIMER_CLK_XO32K=y  # 外接32K晶振(典型值32768Hz,精度更高)

# 启用 Dual Timer (2通道)
CONFIG_LISA_HWTIMER_ARCS_DUAL_TIMER=y

```

Venusa `prj_venusa.conf` 示例：

```kconfig
CONFIG_LISA_HWTIMER_DEVICE=y

# 启用 GPT Timer
CONFIG_LISA_HWTIMER_VENUSA_GPT_TIMER=y

# 启用 AON Timer
CONFIG_LISA_HWTIMER_VENUSA_AON_TIMER=y

# 启用 Dual Timer
CONFIG_LISA_HWTIMER_VENUSA_DUAL_TIMER=y

# Venusa AON 时钟源选择(三选一):
# CONFIG_LISA_HWTIMER_VENUSA_AON_TIMER_CLK_RC32K=y          # 内部RC32K时钟源(默认)
# CONFIG_LISA_HWTIMER_VENUSA_AON_TIMER_CLK_XO24M_DIV32K=y   # 外部XO24M分频到32K域
# CONFIG_LISA_HWTIMER_VENUSA_AON_TIMER_CLK_RC24M_DIV32K=y   # 内部RC24M分频到32K域
```

根据需要选择启用不同的定时器设备。

## API 接口

### 获取硬件能力

```c
int lisa_hwtimer_get_capabilities(lisa_device_t *dev, lisa_hwtimer_capabilities_t *caps);
```

### 频率控制

```c
int lisa_hwtimer_set_frequency(lisa_device_t *dev, uint8_t channel, uint32_t freq_hz);
```

### 定时器控制

```c
int lisa_hwtimer_start(lisa_device_t *dev, uint8_t channel, uint32_t count, lisa_hwtimer_mode_t mode);
int lisa_hwtimer_stop(lisa_device_t *dev, uint8_t channel);
int lisa_hwtimer_reset(lisa_device_t *dev, uint8_t channel);
int lisa_hwtimer_get_value(lisa_device_t *dev, uint8_t channel, uint32_t *count);
```

### 回调管理

```c
int lisa_hwtimer_set_callback(lisa_device_t *dev, uint8_t channel,
                               lisa_hwtimer_callback_t callback, void *user_data);
```

## 使用示例

### 基本定时器使用 (GPT Timer)

```c
#include "lisa_hwtimer.h"

// 定时器超时回调函数
void timer_timeout_callback(void *user_data)
{
    // 定时器超时处理逻辑(在中断上下文中执行)
    printf("Timer timeout!\n");
}

// 获取 GPT Timer 设备
lisa_device_t *timer = lisa_device_get("gpt_timer");
if (!timer) {
    return -1;
}

// 查询定时器能力
lisa_hwtimer_capabilities_t caps;
lisa_hwtimer_get_capabilities(timer, &caps);
printf("Timer has %d channels\n", caps.channel_count);

// 设置通道 0 的频率为 1MHz
lisa_hwtimer_set_frequency(timer, 0, 1000000);

// 注册超时回调
lisa_hwtimer_set_callback(timer, 0, timer_timeout_callback, NULL);

// 启动单次定时器: 1000 counts @ 1MHz = 1ms
lisa_hwtimer_start(timer, 0, 1000, LISA_HWTIMER_MODE_ONESHOT);

// 或使用便捷宏
LISA_HWTIMER_START_ONESHOT(timer, 0, 1000);
```

### 周期定时器使用

```c
// 设置频率为 10MHz
lisa_hwtimer_set_frequency(timer, 1, 10000000);

// 注册回调
lisa_hwtimer_set_callback(timer, 1, timer_timeout_callback, NULL);

// 启动周期定时器: 10000 counts @ 10MHz = 1ms, 周期触发
lisa_hwtimer_start(timer, 1, 10000, LISA_HWTIMER_MODE_PERIODIC);

// 或使用便捷宏
LISA_HWTIMER_START_PERIODIC(timer, 1, 10000);

// 稍后停止定时器
lisa_hwtimer_stop(timer, 1);
```

### 低功耗定时器使用 (AON Timer)

```c
// 获取 AON Timer 设备
lisa_device_t *aon_timer = lisa_device_get("aon_timer");
if (!aon_timer) {
    return -1;
}

// 获取 AON Timer 能力,查询实际频率
lisa_hwtimer_capabilities_t caps;
lisa_hwtimer_get_capabilities(aon_timer, &caps);
uint32_t aon_freq = caps.max_freq_hz;  // RC32K: 动态获取, XO32K: 固定32768Hz

// AON Timer 只有1个通道(通道0), 频率由时钟源决定，并且内部也不支持分频
lisa_hwtimer_set_frequency(aon_timer, 0, aon_freq);

// 注册回调
lisa_hwtimer_set_callback(aon_timer, 0, timer_timeout_callback, NULL);

// 启动周期定时器: aon_freq counts @ aon_freq Hz = 1秒
lisa_hwtimer_start(aon_timer, 0, aon_freq, LISA_HWTIMER_MODE_PERIODIC);
```

### 读取定时器计数值

```c
// 启动定时器后,可以随时读取当前计数值
uint32_t current_count;
lisa_hwtimer_get_value(timer, 0, &current_count);

// 计算已经过的时间
// 已过时间(秒) = (初始count - 当前count) / 频率(Hz)
```

### 重置定时器

```c
// 重置定时器计数值到初始状态(部分定时器支持)
int ret = lisa_hwtimer_reset(timer, 0);
if (ret == LISA_DEVICE_ERR_NOT_SUPPORT) {
    // 该定时器不支持重置操作
}
```

## 定时器类型对比

| 平台 | Timer | 通道数 | 计数器位数 | 时钟源/分频 | 频率范围 | 适用场景 |
|------|-------|--------|------------|-------------|----------|----------|
| ARCS | GPT Timer | 8 | 32位 | 100MHz, 1/2/4/8/16/32/64/128 | 781.25kHz-100MHz | 高精度定时 |
| Venusa | GPT Timer | 2 | 16位 | PCLK/T0, divider 1-128, prediv 1-65536 | 运行时计算 | 中高频短周期定时 |
| ARCS | AON Timer | 1 | 24位 | RC32K/XO32K | ~32.768kHz | 低功耗应用 |
| Venusa | AON Timer | 1 | 24位 | RC32K/XO24M_DIV32K/RC24M_DIV32K | ~32.768kHz | 低功耗应用 |
| ARCS/Venusa | Dual Timer | 2 | 32位 | 16kHz, 1/16/256 | 62Hz-16kHz | 通用定时 |

## 定时器模式

- `LISA_HWTIMER_MODE_ONESHOT` - 单次触发模式
  - 定时器触发一次后自动停止
  - 适用于延时、超时等场景

- `LISA_HWTIMER_MODE_PERIODIC` - 周期触发模式
  - 定时器持续周期触发直到手动停止
  - 适用于周期性任务、心跳等场景

## 频率配置说明

### GPT Timer 频率配置

ARCS GPT Timer 基准时钟为 100MHz,支持以下频率:

```c
// 支持的频率 (Hz)
100000000   // 100MHz  (分频/1)
50000000    // 50MHz   (分频/2)
25000000    // 25MHz   (分频/4)
12500000    // 12.5MHz (分频/8)
6250000     // 6.25MHz (分频/16)
3125000     // 3.125MHz(分频/32)
1562500     // 1.5625MHz(分频/64)
781250      // 781.25kHz(分频/128)
```

Venusa GPT Timer 使用 `PCLK` / `T0` 作为可选时钟源，计数频率公式为:

```
tick_hz = source_hz / divider / (prediv + 1)
```

- `divider` 支持 1/2/4/8/16/32/64/128
- `prediv` 寄存器范围为 0-65535，对应实际分频 1-65536
- LISA hwtimer API 直接用 `count / frequency_hz` 计算定时时间，因此 Venusa 适配层只接受可精确表示的 `freq_hz`
- Venusa GPT Timer 当前使用 16-bit timer，`count` 范围为 1-65535
- Venusa GPT Timer 当前不支持 32-bit period/count：`Driver_GPT_TIMER.h` 只暴露 `HAL_GPT_TIMER_16BITS_INDEX_0` 和两个 8-bit timer index，`HAL_GPT_ReadTimerCount()` / `HAL_GPT_SetTimerPeriodByCount()` 的 count 参数为 `uint16_t`；`gpt_reg.h` 中 `REG_CHx_RELOAD` 也拆分为 16-bit reload 配置和 16-bit 当前计数值，而不是 32-bit reload/count

### AON Timer 频率配置

AON Timer 频率由 Kconfig 配置的时钟源决定:

```c
// 使用内部RC32K时钟源 (CONFIG_LISA_HWTIMER_ARCS_AON_TIMER_CLK_RC32K=y, 默认)
// 频率通过 CRM_GetSrcFreq(CRM_IpSrcAon32kClk) 动态获取
// 典型值约为 32768Hz, 但可能有偏差

// 使用外部32K晶振 (CONFIG_LISA_HWTIMER_ARCS_AON_TIMER_CLK_XO32K=y)
32768       // 固定 32.768kHz (精度高, 需要外部晶振硬件支持)

// Venusa 默认使用 CONFIG_LISA_HWTIMER_VENUSA_AON_TIMER_CLK_RC32K
// 也可选择 XO24M_DIV32K 或 RC24M_DIV32K，均工作在 32K 域
```

**时钟源选择建议:**
- **RC32K**: 无需外部晶振, 功耗更低, 但精度较差, 适用于对时间精度要求不高的场景
- **XO32K**: 需要外部32K晶振, 精度高, 适用于需要精确计时的场景

### Dual Timer 频率配置

Dual Timer 基准时钟为 16kHz,支持以下频率:

```c
// 支持的频率 (Hz)
16000       // 16kHz (分频/1)
1000        // 1kHz  (分频/16)
62          // 62Hz (分频/256)
```

## 定时时间计算

定时器的实际定时时间通过以下公式计算:

```
定时时间(秒) = count / 频率(Hz)
```

### 示例计算

**示例1**: GPT Timer 定时 10ms

```c
// 方法1: 使用 1MHz 频率
lisa_hwtimer_set_frequency(timer, 0, 1000000);      // 1MHz
lisa_hwtimer_start(timer, 0, 10000, ...);           // 10000/1000000 = 10ms

// 方法2: 使用 10MHz 频率
lisa_hwtimer_set_frequency(timer, 0, 10000000);     // 10MHz
lisa_hwtimer_start(timer, 0, 100000, ...);          // 100000/10000000 = 10ms
```

**示例2**: AON Timer 定时 1秒

```c
// 先获取实际频率
lisa_hwtimer_capabilities_t caps;
lisa_hwtimer_get_capabilities(aon_timer, &caps);
uint32_t freq = caps.max_freq_hz;  // RC32K: 动态获取, XO32K: 32768Hz

lisa_hwtimer_set_frequency(aon_timer, 0, freq);
lisa_hwtimer_start(aon_timer, 0, freq, ...);        // freq/freq = 1s
```

**示例3**: Dual Timer 定时 500ms

```c
lisa_hwtimer_set_frequency(dual_timer, 0, 1000);    // 1kHz
lisa_hwtimer_start(dual_timer, 0, 500, ...);        // 500/1000 = 0.5s
```

## 注意事项

1. **通道范围**:
   - GPT Timer(hwtimer API):
     - ARCS 0-7 (8个独立 timer 通道)
     - Venusa 0-1 (2个 timer 通道)
       Venusa PWM 模式下每个 GPT channel 另有 4 个 PWM port，总计 8 路 PWM 输出
   - AON Timer: 0 (1个通道)
   - Dual Timer: 0-1 (2个通道)

2. **频率限制**: 必须使用硬件支持的预设频率,不支持任意频率

3. **回调上下文**: 超时回调在中断上下文中执行,应保持简短快速

4. **AON Timer 时钟源选择**:
   - **ARCS RC32K / Venusa RC32K**: 内部RC振荡器, 频率动态获取, 精度较低但功耗最低
   - **ARCS XO32K**: 外部32K晶振, 固定32768Hz, 精度高但需要硬件支持
   - **Venusa XO24M_DIV32K / RC24M_DIV32K**: 24M 时钟分频到 32K 域
   - 建议在使用前通过 `get_capabilities` 查询实际频率

5. **精度问题**:
   - AON Timer 使用 RC32K 时钟源时误差较大, 不适合精确计时场景
   - GPT Timer 和使用 XO32K 的 AON Timer 精度较高
