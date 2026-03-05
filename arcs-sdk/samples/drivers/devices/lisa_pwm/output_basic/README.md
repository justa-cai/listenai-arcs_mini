# LISA PWM 基础输出示例

## 功能说明

演示如何使用 LISA PWM 驱动输出固定频率和占空比的 PWM 信号，通过配置 PWM 通道参数并启动输出，实现可控的脉冲宽度调制信号。

PWM（脉冲宽度调制）信号广泛应用于 LED 调光、电机控制、音频输出等场景，通过调节占空比可以控制平均输出功率。

## 硬件连接

- **PA20**: PWM0 通道 0 输出引脚

可连接 LED（带限流电阻）或示波器到 PA20 引脚进行验证。连接 LED 时，LED 正极通过限流电阻（推荐 220Ω-1kΩ）连接到 PA20，负极连接到 GND。

## 示例步骤

1. 获取 PWM0 设备
2. 设置 PWM 通道 0 参数（频率 5kHz，占空比 50%）
3. 启动 PWM 输出
4. 保持 PWM 持续输出

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
=== LISA PWM output example ===
pwm0 device ready
PWM enabled: 5kHz, 50% duty cycle
```

**PWM 输出：**
- PA20 引脚输出 5kHz 的 PWM 信号
- 占空比为 50%（高电平和低电平时间各占一半）

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 PWM 设备 |
| `lisa_device_ready()` | 检查设备是否就绪 |
| `lisa_pwm_set()` | 设置 PWM 通道频率和占空比 |
| `lisa_pwm_enable()` | 启动 PWM 通道输出 |
| `lisa_pwm_disable()` | 停止 PWM 通道输出 |

## 关键代码

```c
#define PWM_CHANNEL 0

/* 设置 PWM 频率为 5000Hz，占空比为 50% */
int ret = lisa_pwm_set(pwm_dev, PWM_CHANNEL, 5000, 50);
if (ret != 0) {
    LISA_LOGE(LOG_TAG, "Error: PWM set failed (code: %d)", ret);
    return -1;
}

/* 启用 PWM 输出 */
ret = lisa_pwm_enable(pwm_dev, PWM_CHANNEL);
if (ret != 0) {
    LISA_LOGE(LOG_TAG, "Error: PWM enable failed (code: %d)", ret);
    return -1;
}

LISA_LOGI(LOG_TAG, "PWM enabled: 5kHz, 50%% duty cycle");
```

## 参数说明

### PWM 配置参数

`lisa_pwm_set()` 函数参数说明：

```c
int lisa_pwm_set(lisa_device_t *dev, uint32_t channel,
                 uint32_t frequency, uint32_t duty_cycle);
```

- **`dev`**: PWM 设备指针
- **`channel`**: PWM 通道编号（本示例使用通道 0）
- **`frequency`**: PWM 输出频率，单位 Hz（本示例为 5000Hz = 5kHz）
- **`duty_cycle`**: 占空比，范围 0-100，表示百分比（本示例为 50%）

### 占空比说明

- **1%-10%**: 低占空比，低亮度/低速
- **50%**: 高电平和低电平时间各占一半（中等亮度）
- **90%-99%**: 高占空比，高亮度/高速

**注意**：硬件限制不支持 0% 或 100% 占空比

### 频率选择

- **低频率（< 100Hz）**: 可能产生闪烁，适合步进电机等应用
- **中频率（100Hz - 10kHz）**: 适合 LED 调光、音频等应用
- **高频率（> 10kHz）**: 适合高速电机控制、开关电源等应用

本示例使用 **5kHz** 作为推荐频率，既能保证 LED 无闪烁，又能确保稳定输出。

## 验证方法

### 方法 1: LED 验证

1. 连接 LED（带限流电阻）到 PA20 引脚
2. LED 应以 **中等亮度** 持续点亮（50% 占空比）
3. 如果 LED 全亮或全灭，检查接线和代码配置

### 方法 2: 示波器验证

1. 连接示波器探头到 PA20 引脚
2. 观察波形应为矩形波，参数如下：
   - **频率**: 5000 Hz (5 kHz)
   - **占空比**: 50%（高电平时间 = 低电平时间 = 100μs）
   - **幅值**: 芯片 IO 电压（通常为 3.3V）

## 使用场景

适用于需要可调输出功率的场景：

- **LED 调光**：通过调节占空比控制 LED 亮度
- **电机调速**：通过调节占空比控制电机转速
- **音频输出**：产生不同频率的音调
- **模拟 DAC**：通过低通滤波将 PWM 转换为模拟电压

## 注意事项

1. **占空比范围**：占空比参数范围为 1-99，硬件不支持 0% 或 100%
2. **频率限制**：PWM 频率受硬件限制，过高或过低的频率可能无法精确输出
3. **限流保护**：连接 LED 时必须串联限流电阻，防止电流过大损坏引脚
4. **通道独立**：每个 PWM 通道可独立配置不同的频率和占空比
5. **引脚复用**：确保引脚已正确配置为 PWM 功能（示例中通过 pinmux 自动配置）
