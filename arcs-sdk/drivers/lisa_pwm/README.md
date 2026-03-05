# PWM 驱动

基于 lisa_device 框架的 PWM 设备驱动，为 ARCS 平台提供统一的 PWM 信号输出接口。

## 功能特性

- **设备支持**: PWM0 控制器
  - 基于 GPT0（通用定时器）实现
  - 8 个独立 PWM 通道（Channel 0-7）
  - 每个通道可配置不同的频率、占空比和极性
- **频率控制**: 智能时钟分频选择
  - 自动选择最优时钟分频（÷1/2/4/8/16/32/64/128）
  - 支持宽频率范围（取决于系统 PCLK，典型范围 12Hz-100MHz）
  - 支持在相同分频范围内连续动态调整
- **占空比控制**: 0%-100% 占空比，精度 1%（支持完整范围）
- **极性配置**: 支持正常极性（高电平有效）与反转极性（低电平有效）
- **线程安全**: 内部使用互斥锁保护，可在多线程环境中安全使用

## 配置选项

在 `prj.conf` 中启用驱动:

```kconfig
# 启用 PWM 驱动
CONFIG_LISA_PWM=y
```

PWM 驱动默认使用 PCLK 时钟源和 Edge Aligned 输出模式，无需额外配置。

## API 接口

### 配置接口

```c
int lisa_pwm_configure(lisa_device_t *dev, uint32_t channel, const lisa_pwm_config_t *config);
int lisa_pwm_get_config(lisa_device_t *dev, uint32_t channel, lisa_pwm_config_t *config);
```

配置 PWM 通道的属性（如极性）。

### 控制接口

```c
int lisa_pwm_set(lisa_device_t *dev, uint32_t channel, uint32_t frequency_hz, uint8_t duty_cycle_percent);
int lisa_pwm_enable(lisa_device_t *dev, uint32_t channel);
int lisa_pwm_disable(lisa_device_t *dev, uint32_t channel);
```

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

### 配置极性

```c
#include "lisa_pwm.h"

// 1. 获取设备
lisa_device_t *pwm = lisa_device_get("pwm0");
if (!lisa_device_ready(pwm)) {
    return -1;
}

// 2. 配置通道 0 为反转极性（低电平有效）
lisa_pwm_config_t config = {
    .polarity = LISA_PWM_POLARITY_INVERTED,
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
void lisa_pwm_pinmux()
{
    // 配置 PA20 为 PWM 功能（功能码 12）
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, 12);
}
```

**注意**:
- 该函数由板型相关代码实现，不同板型的引脚配置可能不同
- 只需配置实际使用的 PWM 通道引脚
- 功能码需根据芯片手册确定

### 支持的通道

PWM0 控制器支持 8 个独立通道（Channel 0-7），每个通道可配置不同的频率、占空比和极性。

### PWM 时钟配置特性表

| 特性 | PWM0 |
|------|------|
| **设备基础** | 基于 GPT0 |
| **通道数** | 8 个独立通道 |
| **计数器位数** | 16 位 |
| **时钟源** | PCLK（外设时钟） |
| **输出模式** | Edge Aligned（边沿对齐） |
| **支持分频** | 1/2/4/8/16/32/64/128 |
| **频率范围** | 12Hz-100MHz（@PCLK=100MHz） |
| **占空比精度** | 1%（0%-100%） |
| **适用场景** | 电机控制、LED调光、信号生成 |

**说明**：
- 当前驱动使用 PCLK 时钟源和 Edge Aligned 模式
- HAL 层还支持 XTAL/EXT 时钟源和 Central Aligned 模式（当前未使用）

## 频率配置说明

### 时钟分频详细表

PWM 驱动使用 PCLK（外设时钟）作为基准时钟，支持 8 种分频配置。

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
最小频率 = 有效时钟 / 65535
最大频率 = 有效时钟 / 1
```

### 频率选择策略

驱动采用智能时钟分频选择机制：

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
// 第一次设置 5kHz，驱动选择 shift=4
lisa_pwm_set(pwm, 0, 5000, 50);    // ✓ 返回 0

// 跨分频配置（123Hz 需要 shift=2），驱动自动处理
lisa_pwm_set(pwm, 0, 123, 50);     // ✓ 返回 0，自动重新配置
// 日志：Frequency 123 Hz requires divider shift 2 (current 4), auto-reconfiguring...

// 再次跨分频（50kHz 需要 shift=0），驱动自动处理
lisa_pwm_set(pwm, 0, 50000, 50);   // ✓ 返回 0，自动重新配置
```

## 极性配置

PWM 支持两种输出极性：

- `LISA_PWM_POLARITY_NORMAL` - 正常极性
  - 高电平有效
  - 占空比表示高电平时间比例
  - 适用于大多数应用场景

- `LISA_PWM_POLARITY_INVERTED` - 反转极性
  - 低电平有效
  - 占空比表示低电平时间比例
  - 适用于需要低电平驱动的设备

## 参数范围

| 参数 | 范围 | 说明 |
|------|------|------|
| 频率 | 12 Hz – 100 MHz | 实际范围取决于 PCLK（100MHz 时约 12Hz-100MHz） |
| 占空比 | 0% – 100% | 精度 1%，支持完整范围（0%和100%通过直接控制输出电平实现） |
| 通道数 | 0 – 7 | 8 个独立通道 |

## 注意事项

1. **引脚配置**: 使用 PWM 通道前必须通过板型 pinmux 配置将对应引脚切换为 PWM 功能
2. **推荐使用顺序**: `lisa_pwm_set()` → `lisa_pwm_enable()` → 动态调整 `lisa_pwm_set()` → `lisa_pwm_disable()`
3. **占空比范围**: 支持完整的 0%-100% 占空比范围，精度 1%。0% 和 100% 通过停止计数器并直接控制输出电平实现
4. **时钟分频自动选择**: 驱动会根据目标频率自动选择最佳时钟分频，无需手动配置
5. **连续调用支持**: 可在同一时钟分频范围内连续多次调用 `lisa_pwm_set()` 修改频率和占空比
6. **跨分频自动处理**: 驱动会自动检测并处理跨时钟分频的频率切换，无需手动禁用通道
7. **频率兼容性检查**: 如果新频率需要不同的时钟分频，驱动会自动重新配置分频器
8. **极性配置建议**: 极性修改建议在通道禁用状态下进行
9. **通道独立性**: 8 个通道完全独立，每个通道的时钟分频、频率、占空比互不影响
10. **线程安全**: 驱动内部使用互斥锁保护，支持多线程并发访问
11. **时钟依赖**: PWM 输出频率依赖于系统 PCLK 配置（典型值 24/48/100 MHz）

## 返回值说明

| 返回值 | 说明 |
|--------|------|
| `LISA_DEVICE_OK (0)` | 成功 |
| `LISA_DEVICE_ERR_INVALID (-1)` | 参数无效（如占空比超出范围、频率需要不同时钟分频） |
| `LISA_DEVICE_ERR_RANGE` | 通道号超出范围（有效范围 0-7） |
| `LISA_DEVICE_ERR_NOT_READY` | 设备未初始化或通道未配置 |
| `LISA_DEVICE_ERR_IO (-10)` | 底层 HAL 操作失败 |
| `LISA_DEVICE_ERR_NOT_SUPPORT` | 操作不受支持 |

**常见错误场景**:

1. **占空比超出范围**
   ```c
   lisa_pwm_set(pwm, 0, 5000, 101);   // 返回 -1，占空比超出 0-100 范围
   ```
   **注意**: 0% 和 100% 占空比现在已支持，会通过停止计数器并直接控制输出电平实现。

2. **频率超出硬件支持范围**
   ```c
   lisa_pwm_set(pwm, 0, 200000000, 50);  // 返回 -1，频率过高
   lisa_pwm_set(pwm, 0, 1, 50);         // 返回 -1，频率过低
   ```

**建议**：
- 在应用设计时，选择在相同分频范围内的频率可以减少重新配置的开销
- 例如：1kHz-50kHz 都可以使用 shift=1（50MHz），可以随意切换
- 虽然驱动自动处理跨分频配置，但频繁跨越多个数量级的频率变化（如 50kHz ↔ 100Hz）会有短暂的输出中断

## 文件说明

- `lisa_pwm.h` - 驱动头文件，包含所有 API 和类型定义
- `lisa_pwm_arcs.c` - ARCS 平台适配实现
- `CMakeLists.txt` - 构建配置
- `Kconfig` - 配置选项
