# PWM 驱动

基于 `lisa_device` 框架的 PWM 设备驱动，为 ARCS 与 Venusa 平台提供统一的 PWM 信号输出接口。

## 功能特性

- **设备支持**: `pwm0` 控制器
  - ARCS: 基于 GPT0 PWM HAL，提供 8 个逻辑通道，通道之间频率/占空比独立
  - Venusa: 基于 GPT0 GPT-PWM HAL，提供 8 个逻辑通道；逻辑通道 0-3 共用 GPT channel 0 周期，逻辑通道 4-7 共用 GPT channel 1 周期
- **频率控制**:
  - ARCS: 使用 PCLK，自动选择 1/2/4/8/16/32/64/128 分频
  - Venusa: 支持组级自动分频，但同组 4 port 共享频率；按 GPT channel 0/1 分别选择 `clk_src / ((prediv + 1) * div)` 计数时钟；按 datasheet 公式 `period_ticks = reload + 2` 换算 GPT reload
  - 支持宽频率范围（ARCS 取决于系统 PCLK；Venusa 取决于 PCLK/T0、prediv/divider 和 `reload + 2`）
  - 支持动态调整；Venusa 同组已有 1%-99% PWM 输出时，新频率必须能复用同组 clock/reload
- **占空比控制**: `0%-100%` 占空比，API 精度 1%
- **输出模式配置**:
  - ARCS: 支持边沿对齐（Edge Aligned）与中心对齐（Center Aligned）
  - Venusa: HAL 当前没有对齐模式选择位，`LISA_PWM_MODE_CENTER_ALIGNED` 会被接受但实际按硬件默认边沿对齐输出，并打印 warning
- **极性配置**: 支持正常极性（高电平有效）与反转极性（低电平有效）
  - ARCS: 中心对齐模式使用硬件极性翻转，边沿对齐模式使用 `100% - duty` 模拟反转极性
  - Venusa: 通过 HAL `output_polarity` 配置正常/反转极性
- **线程安全**: 内部使用互斥锁保护，可在多线程环境中安全使用

## 配置选项

在 `prj.conf` 中启用驱动:

```kconfig
# 启用 PWM 驱动
CONFIG_LISA_PWM=y
```

ARCS PWM 驱动默认使用 PCLK 时钟源和 Edge Aligned 输出模式；Venusa 会在 PCLK/T0 中自动选择可用时钟源。需要 Center Aligned 输出或反转极性时，可通过 `lisa_pwm_configure()` 配置通道属性。
`lisa_pwm_config_t` 应零初始化或完整指定所有字段；零初始化时，`mode` 默认为 `LISA_PWM_MODE_EDGE_ALIGNED`，`polarity` 默认为 `LISA_PWM_POLARITY_NORMAL`。

## API 接口

### 配置接口

```c
int lisa_pwm_configure(lisa_device_t *dev, uint32_t channel, const lisa_pwm_config_t *config);
int lisa_pwm_get_config(lisa_device_t *dev, uint32_t channel, lisa_pwm_config_t *config);
```

配置 PWM 通道的属性（输出模式、极性）。

### 控制接口

```c
int lisa_pwm_set(lisa_device_t *dev, uint32_t channel, uint32_t frequency_hz, uint8_t duty_cycle_percent);
int lisa_pwm_enable(lisa_device_t *dev, uint32_t channel);
int lisa_pwm_disable(lisa_device_t *dev, uint32_t channel);
```

推荐调用顺序：

1. `lisa_device_get("pwm0")` / `lisa_device_ready()`
2. 如需非默认模式或极性，先调用 `lisa_pwm_configure()`
3. 调用 `lisa_pwm_set()` 设置频率和占空比
4. 调用 `lisa_pwm_enable()` 启动输出
5. 运行中可继续调用 `lisa_pwm_set()` 动态调整
6. 调用 `lisa_pwm_disable()` 停止输出

## 使用示例

### 基础 PWM 输出

```c
#include "lisa_pwm.h"

// 1. 获取 PWM 设备
lisa_device_t *pwm = lisa_device_get("pwm0");
if (!lisa_device_ready(pwm)) {
    return -1;
}

// 2. 设置频率与占空比（5kHz，50% 占空比）
int ret = lisa_pwm_set(pwm, 0, 5000, 50);
if (ret != LISA_DEVICE_OK) {
    printf("Failed to set PWM: %d\n", ret);
    return -1;
}

// 3. 启用 PWM 输出
ret = lisa_pwm_enable(pwm, 0);
if (ret == LISA_DEVICE_OK) {
    printf("PWM output started\n");
}

// 4. 停止输出
lisa_pwm_disable(pwm, 0);
```

### 0% 和 100% 占空比使用

```c
#include "lisa_pwm.h"

// 1. 获取 PWM 设备
lisa_device_t *pwm = lisa_device_get("pwm0");
if (!lisa_device_ready(pwm)) {
    return -1;
}

// 2. 设置 0% 占空比（输出低电平）
lisa_pwm_set(pwm, 0, 1000, 0);
lisa_pwm_enable(pwm, 0);
// 此时输出持续低电平

// 3. 切换到 100% 占空比（输出高电平）
lisa_pwm_set(pwm, 0, 1000, 100);
// 此时输出持续高电平

// 4. 切换到正常 PWM 波形（50% 占空比）
lisa_pwm_set(pwm, 0, 1000, 50);
// 此时输出标准 PWM 波形

// 5. 停止输出
lisa_pwm_disable(pwm, 0);
```

**注意**：
- 0% 和 100% 占空比通过停止计数器并直接控制输出电平实现
- 0% 占空比：输出低电平（正常极性）或高电平（反转极性）
- 100% 占空比：输出高电平（正常极性）或低电平（反转极性）

### 配置输出模式与极性

```c
#include "lisa_pwm.h"

// 1. 获取设备
lisa_device_t *pwm = lisa_device_get("pwm0");
if (!lisa_device_ready(pwm)) {
    return -1;
}

// 2. 配置通道 0 为中心对齐 + 反转极性（低电平有效）
lisa_pwm_config_t config = {
    .polarity = LISA_PWM_POLARITY_INVERTED,
    .mode = LISA_PWM_MODE_CENTER_ALIGNED,
};
lisa_pwm_configure(pwm, 0, &config);

// 3. 设置频率和占空比
lisa_pwm_set(pwm, 0, 5000, 50);

// 4. 启用输出
lisa_pwm_enable(pwm, 0);
```

### 动态调整频率和占空比

```c
#include "lisa_pwm.h"

// 1. 获取设备
lisa_device_t *pwm = lisa_device_get("pwm0");
if (!lisa_device_ready(pwm)) {
    return -1;
}

// 2. 设置初始参数并启用
lisa_pwm_set(pwm, 0, 1000, 10);  // 1kHz, 10%
lisa_pwm_enable(pwm, 0);

// 3. 动态调整占空比
lisa_pwm_set(pwm, 0, 1000, 50);  // 保持 1kHz, 调整为 50%
lisa_pwm_set(pwm, 0, 1000, 90);  // 保持 1kHz, 调整为 90%
lisa_pwm_set(pwm, 0, 1000, 0);   // 保持 1kHz, 调整为 0%（输出低电平）
lisa_pwm_set(pwm, 0, 1000, 100); // 保持 1kHz, 调整为 100%（输出高电平）

// 4. 动态调整频率（在相同时钟分频范围内）
lisa_pwm_set(pwm, 0, 2000, 50);  // 调整为 2kHz, 50%
lisa_pwm_set(pwm, 0, 5000, 70);  // 调整为 5kHz, 70%
lisa_pwm_set(pwm, 0, 10000, 30); // 调整为 10kHz, 30%

// 5. 停止输出
lisa_pwm_disable(pwm, 0);
```

### 多通道同时使用

```c
#include "lisa_pwm.h"

// 1. 获取设备
lisa_device_t *pwm = lisa_device_get("pwm0");
if (!lisa_device_ready(pwm)) {
    return -1;
}

// 2. 配置通道 0: 5kHz, 50% 占空比
lisa_pwm_set(pwm, 0, 5000, 50);
lisa_pwm_enable(pwm, 0);

// 3. 配置通道 1: 10kHz, 30% 占空比
lisa_pwm_set(pwm, 1, 10000, 30);
lisa_pwm_enable(pwm, 1);

// 4. 配置通道 2: 1kHz, 70% 占空比
lisa_pwm_set(pwm, 2, 1000, 70);
lisa_pwm_enable(pwm, 2);
```

## 硬件配置

### 引脚复用配置

PWM 驱动在初始化时会自动调用板型目录中定义的 `lisa_pwm_pinmux()` 函数，用于配置 PWM 通道的引脚复用。

**配置位置**:
- **定义**: `boards/<板型名>/pinmux.c` 中实现 `lisa_pwm_pinmux()` 函数
- **声明**: `boards/<板型名>/pinmux.h` 中声明 `void lisa_pwm_pinmux()`
- **调用时机**: PWM0 设备初始化时自动调用

**示例** (参考 `boards/arcs_evb/pinmux.c`):
```c
// arcs
void lisa_pwm_pinmux()
{
    // 配置 PA20 为 PWM 功能（功能码 12）
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, 12);
}

// venusa
void lisa_pwm_pinmux()
{
    // 配置 PA14 为 PWM 功能（功能码 12）
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 14, 11);
}

```

**注意**:
- 该函数由板型相关代码实现，不同板型的引脚配置可能不同
- 只需配置实际使用的 PWM 通道引脚
- 功能码需根据芯片手册确定

### 支持的通道

- ARCS: 基于 GPT0 PWM HAL，提供 8 个逻辑通道，通道之间频率/占空比独立
- Venusa: 基于 GPT0 GPT-PWM HAL，提供 8 个逻辑通道；逻辑通道 0-3 共用 GPT channel 0 周期，逻辑通道 4-7 共用 GPT channel 1 周期


### ARCS PWM 时钟配置特性表

| 特性 | PWM0 |
|------|------|
| **设备基础** | 基于 GPT0 |
| **通道数** | 8 个独立通道 |
| **计数器位数** | 16 位 |
| **时钟源** | PCLK（外设时钟） |
| **输出模式** | Edge Aligned（边沿对齐）、Center Aligned（中心对齐） |
| **支持分频** | 1/2/4/8/16/32/64/128 |
| **频率范围** | Edge: 12Hz-100MHz；Center: 6Hz-50MHz（@PCLK=100MHz） |
| **占空比精度** | 1%（0%-100%） |
| **适用场景** | 电机控制、LED调光、信号生成 |

**说明**：
- ARCS 当前驱动固定使用 PCLK 时钟源
- 输出模式可通过 `lisa_pwm_config_t.mode` 选择，默认 `LISA_PWM_MODE_EDGE_ALIGNED`
- Venusa 的 group 级时钟选择和共享周期约束见“平台差异”

## 频率配置说明

### ARCS 时钟分频详细表

ARCS PWM 驱动使用 PCLK（外设时钟）作为基准时钟，支持 8 种分频配置。

**分频配置表（PCLK = 100 MHz）**:

| 时钟分频 | 分频系数 | 有效时钟 | 最小频率 | 最大频率 |
|---------|---------|---------|---------|---------|
| ÷1 | 2^0 | 100 MHz | 1526 Hz | 100 MHz |
| ÷2 | 2^1 | 50 MHz | 763 Hz | 50 MHz |
| ÷4 | 2^2 | 25 MHz | 382 Hz | 25 MHz |
| ÷8 | 2^3 | 12.5 MHz | 191 Hz | 12.5 MHz |
| ÷16 | 2^4 | 6.25 MHz | 95 Hz | 6.25 MHz |
| ÷32 | 2^5 | 3.125 MHz | 48 Hz | 3.125 MHz |
| ÷64 | 2^6 | 1.5625 MHz | 24 Hz | 1.5625 MHz |
| ÷128 | 2^7 | 781.25 kHz | 12 Hz | 781.25 kHz |

**计算公式**:
```
有效时钟 = PCLK ÷ 2^shift
边沿对齐最小频率 = 有效时钟 / 65535
中心对齐最小频率 = 有效时钟 / (2 × 65535)
边沿对齐最大频率 = 有效时钟 / 1
中心对齐最大频率 = 有效时钟 / 2
```

### ARCS 频率选择策略

ARCS 驱动采用智能时钟分频选择机制：

1. **第一次调用** `lisa_pwm_set()`
   - 自动选择能够支持目标频率的最优时钟分频
   - 配置 HAL 层时钟分频

2. **后续调用** `lisa_pwm_set()`（跨分频自动处理）
   - 检测到需要不同的时钟分频器时自动重新配置
   - 自动禁用通道（如果已启用）
   - 清除 HAL 层状态并重新配置时钟分频器
   - 自动重新启用通道（如果之前是启用状态）

**示例**（PCLK=100MHz）:
```c
// 第一次设置 5kHz，驱动选择 shift=0
lisa_pwm_set(pwm, 0, 5000, 50);    // ✓ 返回 0

// 跨分频配置（123Hz 需要 shift=4），驱动自动处理
lisa_pwm_set(pwm, 0, 123, 50);     // ✓ 返回 0，自动重新配置
// 日志：Frequency 123 Hz requires divider shift 4 (current 0), auto-reconfiguring...

// 再次跨分频（50kHz 需要 shift=0），驱动自动处理
lisa_pwm_set(pwm, 0, 50000, 50);   // ✓ 返回 0，自动重新配置
```

### Venusa 频率选择策略

Venusa 驱动采用 GPT group 级自动选频和同组频率仲裁机制：

1. **group 级选频**
   - LISA channel 0-3 共享 `GPT_CHANNEL_0` 的 `clock/divider/reload`
   - LISA channel 4-7 共享 `GPT_CHANNEL_1` 的 `clock/divider/reload`
   - 因此频率配置以 GPT channel 为单位生效，不是每个 PWM port 完全独立

2. **group 可重配时自动选择最优计数时钟**
   - 在 `GPT_CLK_SRC_PCLK` / `GPT_CLK_SRC_T0` 中选择可用时钟源
   - 遍历 `GPT_CLK_DIV_1/2/4/8/16/32/64/128`
   - 根据目标频率和 16-bit reload 上限反推最小 `prediv`
   - 在满足目标频率的组合中选择最高 `tick_hz`，提高 duty 分辨率

3. **同组已有 1%-99% PWM 输出时复用当前周期**
   - 同组已有其它 port 正在输出 PWM 波形时，驱动不重新配置 group clock/reload
   - 新通道必须用当前 `tick_hz` 计算出相同 `reload`，否则返回 `LISA_DEVICE_ERR_NOT_SUPPORT`
   - 0%/100% 静态输出不占用 PWM counter，不阻止同组重新选频

**计算公式**:
```
tick_hz = source_hz / divider / (prediv + 1)
period_ticks = round(tick_hz / frequency_hz)
reload = period_ticks - 2
实际频率约为 tick_hz / (reload + 2)
```

**示例**:
```c
lisa_pwm_set(pwm, 0, 1000, 50);
lisa_pwm_enable(pwm, 0);

/* channel 1 和 channel 0 同属 GPT channel 0，同组已有 1%-99% PWM */
lisa_pwm_set(pwm, 1, 1000, 30);  // OK，复用同组频率
lisa_pwm_set(pwm, 1, 2000, 30);  // 返回 LISA_DEVICE_ERR_NOT_SUPPORT，频率冲突

/* channel 4 属于 GPT channel 1，可以独立选择另一组频率 */
lisa_pwm_set(pwm, 4, 2000, 50);  // OK
```

## 输出模式与极性配置

PWM 支持两种输出模式：

- `LISA_PWM_MODE_EDGE_ALIGNED` - 边沿对齐模式
  - 默认模式
  - ARCS 硬件不支持通过极性位直接翻转边沿对齐波形，配置反转极性时驱动会将写入 HAL 的占空比转换为 `100% - duty`
  - Venusa 通过 HAL `output_polarity` 配置正常/反转极性

- `LISA_PWM_MODE_CENTER_ALIGNED` - 中心对齐模式
  - ARCS 支持中心对齐波形，驱动直接配置 HAL 极性位
  - Venusa HAL 当前没有对齐模式选择位，会接受配置但实际按硬件默认边沿对齐输出

PWM 支持两种输出极性：

- `LISA_PWM_POLARITY_NORMAL` - 正常极性
  - 高电平有效
  - 占空比表示高电平时间比例
  - 适用于大多数应用场景

- `LISA_PWM_POLARITY_INVERTED` - 反转极性
  - 低电平有效
  - 占空比表示低电平时间比例
  - 适用于需要低电平驱动的设备

示例：

```c
lisa_pwm_config_t config = {
    .polarity = LISA_PWM_POLARITY_INVERTED,
    .mode = LISA_PWM_MODE_EDGE_ALIGNED,
};
lisa_pwm_configure(pwm, 0, &config);

/* Edge Aligned + Inverted: 用户请求 30% 低电平有效占空比，
 * 驱动实际向 HAL 写入 70% 占空比以模拟反相。
 */
lisa_pwm_set(pwm, 0, 5000, 30);
```

在 Venusa 上，`LISA_PWM_MODE_CENTER_ALIGNED` 会降级为硬件默认边沿对齐输出。

## 平台差异

### ARCS

| 特性 | 说明 |
|------|------|
| HAL | `GPT0_PWM()` / `HAL_GPT_PWM*` |
| 逻辑通道 | 0-7，通道互相独立 |
| 时钟源 | PCLK |
| 分频 | 自动选择 1/2/4/8/16/32/64/128 |
| 对齐模式 | Edge Aligned / Center Aligned |
| 极性 | Center Aligned 使用 HAL 极性位；Edge Aligned 反转极性通过 `100% - duty` 模拟 |
| 0% / 100% | 驱动使用静态电平路径处理边界占空比 |

### Venusa

| 特性 | 说明 |
|------|------|
| HAL | `GPT0()` / `HAL_GPT_*PWM*` |
| 逻辑通道 | 0-7；0-3 映射到 GPT channel 0 的 PWM port 0-3，4-7 映射到 GPT channel 1 的 PWM port 0-3 |
| 时钟源 | 自动在 `GPT_CLK_SRC_PCLK` / `GPT_CLK_SRC_T0` 中选择，并保存每个 GPT channel 当前 `clk_src` / `prediv` / `clk_div` / `tick_hz` |
| 分频 | GPT group 级自动选择 `prediv` 和 `GPT_CLK_DIV_1/2/4/8/16/32/64/128`，优先选择可满足 16-bit reload 的较高计数时钟以提高 duty 分辨率 |
| 频率 | `reload = round(tick_hz / frequency_hz) - 2`；硬件最大 PWM 输出频率以 datasheet 50MHz 为上限，实际还受当前 PCLK/T0、分频和 `reload + 2` 公式限制 |
| 共享周期 | 同一 GPT channel 下的 4 个 port 共用 clock/divider/reload；同组同时输出 1%-99% PWM 时必须使用相同频率，0%/100% 静态输出不占用 PWM counter |
| 对齐模式 | HAL 当前没有对齐选择，中心对齐请求会降级为硬件默认边沿对齐 |
| 极性 | 通过 HAL `output_polarity` 配置正常/反转极性 |
| 0% / 100% | datasheet 说明边界占空比会绕开 shadow register；驱动改用 disable PWM port + `init_level` 输出静态电平，避免动态切换毛刺 |

Venusa 通道映射：

| LISA channel | GPT channel | GPT PWM port | sample 常用引脚 |
|--------------|-------------|--------------|-----------------|
| 0 | `GPT_CHANNEL_0` | `GPT_PWM_PORT_0` | PA14 / ALT11 |
| 1 | `GPT_CHANNEL_0` | `GPT_PWM_PORT_1` | PA15 / ALT11 |
| 2 | `GPT_CHANNEL_0` | `GPT_PWM_PORT_2` | PA16 / ALT11 |
| 3 | `GPT_CHANNEL_0` | `GPT_PWM_PORT_3` | PA17 / ALT11 |
| 4 | `GPT_CHANNEL_1` | `GPT_PWM_PORT_0` | PA18 / ALT11 |
| 5 | `GPT_CHANNEL_1` | `GPT_PWM_PORT_1` | PA19 / ALT11 |
| 6 | `GPT_CHANNEL_1` | `GPT_PWM_PORT_2` | PA20 / ALT11 |
| 7 | `GPT_CHANNEL_1` | `GPT_PWM_PORT_3` | PA21 / ALT11 |



## 参数范围

| 参数 | ARCS | Venusa |
|------|------|--------|
| 通道 | 0-7 | 0-7 |
| 频率 | 取决于 PCLK、分频和模式 | 取决于 PCLK/T0、group 级自动分频和 `reload + 2` 周期公式 |
| 占空比 | 0%-100% | 0%-100% |
| 输出模式 | Edge / Center | Edge；Center 请求降级为 Edge |
| 极性 | Normal / Inverted | Normal / Inverted |

## 注意事项

1. 使用 PWM 前必须确认 `lisa_pwm_pinmux()` 已将目标引脚配置为 PWM 功能。
2. Venusa 支持组级自动分频，但同组 4 port 共享频率；如果同组已有非 0%/100% 通道正在输出，再设置/启用不能复用同一组 clock/divider/reload 的频率会返回 `LISA_DEVICE_ERR_NOT_SUPPORT`。
3. Venusa 的 `HAL_GPT_PWMControl()` 会临时 disable 整个 GPT channel；驱动会在配置后恢复同组已 enable 的 port，但运行中修改同组 port 仍可能出现极短暂毛刺。
4. ARCS 支持中心对齐；Venusa 当前 HAL 无中心对齐控制，sample 在 Venusa 上只演示边沿对齐和极性切换。
5. 频率无法整除计数时，Venusa 使用四舍五入后的整数 reload，实际频率约为 `tick_hz / (reload + 2)`。
6. PM suspend 后驱动会清空通道逻辑状态；唤醒后应用需重新 `configure()` / `set()` / `enable()`。

## 返回值说明

| 返回值 | 说明 |
|--------|------|
| `LISA_DEVICE_OK (0)` | 成功 |
| `LISA_DEVICE_ERR_INVALID (-1)` | 参数无效（如占空比超出范围，或 ARCS 频率不支持） |
| `LISA_DEVICE_ERR_RANGE (-11)` | 通道号或频率周期计数超出范围 |
| `LISA_DEVICE_ERR_NOT_READY (-9)` | 设备未初始化或通道未配置 |
| `LISA_DEVICE_ERR_IO (-10)` | 底层 HAL 操作失败 |
| `LISA_DEVICE_ERR_NOT_SUPPORT (-6)` | 操作不受支持，如 Venusa 同组已启用通道频率冲突 |

**常见错误场景**:

1. **占空比超出范围**
   ```c
   lisa_pwm_set(pwm, 0, 5000, 101);   // 返回 -1，占空比超出 0-100 范围
   ```
   **注意**: 0% 和 100% 占空比现在已支持，会通过停止计数器并直接控制输出电平实现。

2. **频率超出硬件支持范围**
   ```c
   lisa_pwm_set(pwm, 0, 200000000, 50); // ARCS 返回 -1；Venusa 返回 -11，频率过高
   lisa_pwm_set(pwm, 0, 1, 50);         // ARCS 返回 -1；Venusa 返回 -11，频率过低
   ```

**建议**：
- ARCS 应用设计时，选择在相同分频范围内的频率可以减少重新配置的开销
- Venusa 应用设计时，同组 4 个 port 建议规划为相同频率；不同频率请分配到不同 GPT group
- 虽然驱动自动处理跨分频配置，但频繁跨越多个数量级的频率变化（如 50kHz ↔ 100Hz）会有短暂的输出中断

## 文件说明

- `lisa_pwm.h` - 驱动头文件，包含所有 API 和类型定义
- `lisa_pwm_arcs.c` - ARCS 平台适配实现
- `lisa_pwm_venusa.c` - Venusa 平台适配实现
- `CMakeLists.txt` - 构建配置, 根据 `CONFIG_SOC_ARCS` / `CONFIG_SOC_VENUSA` 选择平台实现
- `Kconfig` - 配置选项
