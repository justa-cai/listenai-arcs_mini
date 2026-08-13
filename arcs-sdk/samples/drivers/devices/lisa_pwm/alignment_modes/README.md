# PWM 对齐模式示例

## 功能说明

本示例演示如何通过 LISA PWM 驱动配置边沿对齐（Edge Aligned）和中心对齐（Center Aligned）输出模式，并分别观察正常极性与反转极性的 PWM 波形。

示例会在同一个 PWM 通道上循环输出模式/极性配置。`arcs_evb` 支持以下 4 种配置；`venusa_rd_evb` datasheet 描述 GPT 硬件支持中心对齐，但当前 Venusa GPT PWM HAL 没有公开中心对齐选择接口，因此 sample 在 Venusa 上只运行前 2 种边沿对齐配置。

| 序号 | 输出模式 | 极性 | 频率 | 占空比 |
|------|----------|------|------|--------|
| 1 | Edge Aligned | Normal | 5kHz | 30% |
| 2 | Edge Aligned | Inverted | 5kHz | 30% |
| 3 | Center Aligned | Normal | 5kHz | 30% |
| 4 | Center Aligned | Inverted | 5kHz | 30% |

每种配置保持 3 秒，便于使用示波器或逻辑分析仪观察波形变化。

## 硬件连接

- **arcs_evb**: PA20 为 PWM0 通道 0 输出引脚
- **venusa_rd_evb**: PA14 为 PWM0 逻辑通道 0 输出引脚（GPT0 channel 0 / PWM port 0，ALT11）

本示例已为 `arcs_evb` 和 `venusa_rd_evb` 重定向 `lisa_pwm_pinmux()`，可将示波器或逻辑分析仪探头连接到对应引脚，地线连接到 GND。其他板型未在示例中内置 pinmux 重定向，运行时会打印提示；使用前需确认板级默认 pinmux 是否已经将 PWM0 通道 0 路由到可观测引脚，或按板型补充 `lisa_pwm_pinmux()`。

## 示例步骤

1. 获取 `pwm0` 设备
2. 配置 PWM 通道 0 的输出模式和极性
3. 设置 5kHz、30% 占空比
4. 启动 PWM 输出
5. 每 3 秒切换到下一组模式/极性配置

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

**终端输出：**

```text
=== LISA PWM alignment modes example ===
pwm0 device ready
PWM alignment modes example running
edge aligned normal: 5000Hz, 30% duty
edge aligned inverted: 5000Hz, 30% duty
center aligned normal: 5000Hz, 30% duty
center aligned inverted: 5000Hz, 30% duty
```

`venusa_rd_evb` 会额外打印中心对齐不支持提示，并且只循环前两条 edge aligned 输出。

**PWM 输出：**

- Edge Aligned + Normal：边沿对齐波形，高电平占 30%
- Edge Aligned + Inverted：边沿对齐反相波形
- Center Aligned + Normal：中心对齐波形，高电平占 30%（仅 ARCS sample 运行）
- Center Aligned + Inverted：中心对齐反相波形（仅 ARCS sample 运行）

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 PWM 设备 |
| `lisa_device_ready()` | 检查设备是否就绪 |
| `lisa_pwm_configure()` | 配置 PWM 输出模式和极性 |
| `lisa_pwm_set()` | 设置 PWM 通道频率和占空比 |
| `lisa_pwm_enable()` | 启动 PWM 通道输出 |

## 关键代码

```c
lisa_pwm_config_t config = {
    .polarity = LISA_PWM_POLARITY_INVERTED,
    .mode = LISA_PWM_MODE_EDGE_ALIGNED,
};

lisa_pwm_configure(pwm_dev, 0, &config);
lisa_pwm_set(pwm_dev, 0, 5000, 30);
lisa_pwm_enable(pwm_dev, 0);
```

## 模式说明

### 边沿对齐模式

`LISA_PWM_MODE_EDGE_ALIGNED` 是默认输出模式。ARCS 驱动在边沿对齐反转极性时使用 `100% - duty` 写入 HAL 来模拟反相；Venusa 驱动通过 GPT PWM HAL 的 `output_polarity` 位配置反转极性。

### 中心对齐模式

`LISA_PWM_MODE_CENTER_ALIGNED` 适用于需要中心对齐波形的应用。ARCS 支持该模式；Venusa datasheet 描述 GPT 硬件支持中心对齐，但当前 Venusa GPT PWM HAL 没有公开中心对齐选择接口，驱动会打印 warning 并按硬件默认边沿对齐输出，因此本 sample 在 Venusa 上不运行中心对齐用例。

## 验证方法

1. 构建并烧录本示例
2. 在 `arcs_evb` 上观察 PA20，在 `venusa_rd_evb` 上观察 PA14；其他板型先确认 PWM0 通道 0 对应输出引脚
3. 确认波形每 3 秒切换一次
4. `arcs_evb` 对比 Edge Aligned 和 Center Aligned 的波形位置，以及 Normal/Inverted 的有效电平差异；`venusa_rd_evb` 对比 Edge Aligned 的 Normal/Inverted 差异

## 注意事项

1. 本示例为 `arcs_evb` 重定向到 PA20，为 `venusa_rd_evb` 重定向到 PA14
2. ARCS Edge Aligned 反转极性通过 `100% - duty` 实现；Venusa 使用 HAL 极性位
3. Center Aligned 用例仅在 ARCS 上运行；Venusa 当前 HAL 未公开中心对齐选择接口
4. 不同板型的 PWM 引脚可能不同，移植时需按板型修改 pinmux 配置
