# ADC 驱动

基于 lisa_device 框架的 ADC 设备驱动，为 ARCS 平台提供统一的模拟信号采样接口。

## 功能特性

- **设备支持**: ADC0 控制器，支持多个模拟输入通道
- **通道支持**: 支持通道 0-5 的模拟信号采样，以及 VBAT 和温度传感器特殊通道
- **按通道配置**: 每个通道可独立配置参考电压和分辨率
- **多种参考电压**: 支持 1.2V、3.6V、VDD_IO/2、VDD_IO/3、VDD_IO*3/2、VDD_IO、外部参考电压
- **线程安全**: 内部使用互斥锁保护并发访问
- **灵活配置**: 通过 `lisa_adc_channel_setup()` 运行时配置通道参数

## 配置选项

在 Kconfig 中启用驱动：

```kconfig
CONFIG_LISA_ADC=y
```

## API 接口

### 配置 ADC 通道

```c
int lisa_adc_channel_setup(lisa_device_t *dev, uint32_t channel,
                            const lisa_adc_channel_config_t *config);
```

配置指定通道的参考电压和分辨率。应在读取通道之前调用。

**参数**:
- `dev`: ADC 设备指针
- `channel`: ADC 通道号（0-5：普通通道，6：VBAT，7：TEMP）
- `config`: 通道配置参数，包含：
  - `reference`: 参考电压类型（枚举）
  - `resolution`: ADC 分辨率（枚举）

**参考电压类型**:
- `LISA_ADC_REF_VDD_1V2`: 固定 1.2V 参考电压
- `LISA_ADC_REF_VDD_3V6`: 固定 3.6V 参考电压
- `LISA_ADC_REF_VDD_IO_AUTO`: VDD_IO 自动分压（硬件自动选择：VDD_IO≤2.4V时分压系数1/2，>2.4V时分压系数1/3）
- `LISA_ADC_REF_VDD_IO_AUTO_MUL3`: VDD_IO 自动分压 + 3倍缓冲（测量范围扩大3倍）
- `LISA_ADC_REF_EXTERNAL`: 外部参考电压 Vref_ext

**分辨率类型**:
- `LISA_ADC_RESOLUTION_10BIT`: 10-bit（0-1023）

**返回值**:
- `LISA_DEVICE_OK (0)`: 成功
- `LISA_DEVICE_ERR_INVALID`: 参数无效
- `LISA_DEVICE_ERR_RANGE`: 通道号超出范围
- `LISA_DEVICE_ERR_NOT_SUPPORT`: 不支持的配置

### 读取 ADC 值

```c
int lisa_adc_read(lisa_device_t *dev, uint32_t channel, uint16_t *value);
```

读取指定通道的 ADC 原始采样值。如果通道未配置，将使用默认配置（1.2V 参考电压）。

**参数**:
- `dev`: ADC 设备指针
- `channel`: ADC 通道号（0-5：普通通道，6：VBAT，7：TEMP）
- `value`: 输出采样值指针

**返回值**:
- `LISA_DEVICE_OK (0)`: 成功
- `LISA_DEVICE_ERR_INVALID`: 参数无效
- `LISA_DEVICE_ERR_RANGE`: 通道号超出范围
- `LISA_DEVICE_ERR_IO`: 硬件 IO 错误

## 使用方法

### 基本步骤

1. **获取设备**: 通过 `lisa_device_get()` 获取 ADC 设备
2. **配置通道**: 调用 `lisa_adc_channel_setup()` 配置通道参数
3. **读取数据**: 调用 `lisa_adc_read()` 读取指定通道的 ADC 值
4. **电压转换**: 使用 `LISA_ADC_RAW_TO_MV` 宏将原始值转换为电压值

### 基础使用示例

```c
#include "lisa_adc.h"

// 1. 获取 ADC 设备
lisa_device_t *adc = lisa_device_get("adc0");
if (!lisa_device_ready(adc)) {
    return -1;
}

// 2. 配置通道2使用1.2V参考电压
lisa_adc_channel_config_t ch2_config = {
    .reference = LISA_ADC_REF_VDD_1V2,
    .resolution = LISA_ADC_RESOLUTION_10BIT,
};
lisa_adc_channel_setup(adc, 2, &ch2_config);

// 3. 读取 ADC 通道2的值
uint16_t raw_value;
int ret = lisa_adc_read(adc, 2, &raw_value);
if (ret == LISA_DEVICE_OK) {
    // 5. 计算电压值（毫伏）
    uint32_t voltage_mv = LISA_ADC_RAW_TO_MV(raw_value, 1200, 10);
    printf("Channel 2: %u mV (raw: %u)\n", voltage_mv, raw_value);
}
```

### 不同通道使用不同参考电压

```c
#include "lisa_adc.h"

lisa_device_t *adc = lisa_device_get("adc0");

// 通道0: 使用1.2V参考电压测量低电压信号
lisa_adc_channel_config_t ch0_config = {
    .reference = LISA_ADC_REF_VDD_1V2,
    .resolution = LISA_ADC_RESOLUTION_10BIT,
};
lisa_adc_channel_setup(adc, 0, &ch0_config);

// 通道2: 使用3.6V参考电压测量高电压信号
lisa_adc_channel_config_t ch2_config = {
    .reference = LISA_ADC_REF_VDD_3V6,
    .resolution = LISA_ADC_RESOLUTION_10BIT,
};
lisa_adc_channel_setup(adc, 2, &ch2_config);

// 读取并转换
uint16_t raw0, raw2;
lisa_adc_read(adc, 0, &raw0);
lisa_adc_read(adc, 2, &raw2);

uint32_t voltage0 = LISA_ADC_RAW_TO_MV(raw0, 1200, 10);
uint32_t voltage2 = LISA_ADC_RAW_TO_MV(raw2, 3600, 10);

printf("CH0: %u mV (1.2V ref)\n", voltage0);
printf("CH2: %u mV (3.6V ref)\n", voltage2);
```

### 使用 VDD_IO 自动分压参考电压

```c
#include "lisa_adc.h"

lisa_device_t *adc = lisa_device_get("adc0");

// 配置通道3使用 VDD_IO 自动分压
lisa_adc_channel_config_t ch3_config = {
    .reference = LISA_ADC_REF_VDD_IO_AUTO,
    .resolution = LISA_ADC_RESOLUTION_10BIT,
};
lisa_adc_channel_setup(adc, 3, &ch3_config);

// 读取 ADC 值
uint16_t raw3;
lisa_adc_read(adc, 3, &raw3);

// 电压转换: 用户需要知道实际的 VDD_IO 电压
// 假设硬件 VDD_IO = 3.3V (>2.4V)，硬件自动选择分压系数 1/3
// 则参考电压 = 3.3V / 3 = 1.1V
uint32_t voltage3 = LISA_ADC_RAW_TO_MV(raw3, 1100, 10);
printf("CH3: %u mV (VDD_IO/3 ref)\n", voltage3);

// 如果硬件 VDD_IO = 2.0V (≤2.4V)，硬件自动选择分压系数 1/2
// 则参考电压 = 2.0V / 2 = 1.0V
// uint32_t voltage3 = LISA_ADC_RAW_TO_MV(raw3, 1000, 10);
```

### 读取特殊通道（VBAT 和温度）

```c
#include "lisa_adc.h"

lisa_device_t *adc = lisa_device_get("adc0");

// 配置 VBAT 通道（通道 6）
lisa_adc_channel_config_t vbat_config = {
    .reference = LISA_ADC_REF_VDD_1V2,
    .resolution = LISA_ADC_RESOLUTION_10BIT,
};
lisa_adc_channel_setup(adc, 6, &vbat_config);

// 读取 VBAT 通道
uint16_t vbat_raw;
if (lisa_adc_read(adc, 6, &vbat_raw) == LISA_DEVICE_OK) {
    // VBAT 通道测量的是实际电池电压的 1/3
    uint32_t vbat_sense_mv = LISA_ADC_RAW_TO_MV(vbat_raw, 1200, 10);
    uint32_t vbat_actual_mv = vbat_sense_mv * 3;
    printf("Battery voltage: %u mV\n", vbat_actual_mv);
}

// 配置温度传感器通道（通道 7）
lisa_adc_channel_config_t temp_config = {
    .reference = LISA_ADC_REF_VDD_1V2,
    .resolution = LISA_ADC_RESOLUTION_10BIT,
};
lisa_adc_channel_setup(adc, 7, &temp_config);

// 读取温度传感器通道
uint16_t temp_raw;
if (lisa_adc_read(adc, 7, &temp_raw) == LISA_DEVICE_OK) {
    printf("Temperature sensor raw value: %u\n", temp_raw);
    // 温度计算公式请参考芯片数据手册
}
```

## 硬件配置

### 引脚复用配置

ADC 驱动在初始化时会自动调用板型目录中定义的 `lisa_adc_pinmux()` 函数，用于配置 ADC 通道的引脚复用。

**配置位置**:
- **定义**: `boards/<板型名>/pinmux.c` 中实现 `lisa_adc_pinmux()` 函数
- **声明**: `boards/<板型名>/pinmux.h` 中声明 `void lisa_adc_pinmux()`
- **调用时机**: ADC0 设备初始化时自动调用

**示例** (参考 `boards/arcs_evb/pinmux.c`):
```c
void lisa_adc_pinmux()
{
    // 配置 PB06 为 ADC 功能（功能码 3）
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 6, 3);
}
```

**注意**:
- 该函数由板型相关代码实现，不同板型的引脚配置可能不同
- 只需配置实际使用的 ADC 通道引脚
- 内部通道（VBAT、TEMP）无需配置引脚

### 支持的通道

| 通道号 | 对应引脚/功能 | 说明 |
|--------|--------------|------|
| 0 | PB02 | ADC 通道 0 |
| 1 | PB03 | ADC 通道 1 |
| 2 | PB04 | ADC 通道 2 |
| 3 | PB05 | ADC 通道 3 |
| 4 | PB06 | ADC 通道 4 |
| 5 | PB07 | ADC 通道 5 |
| 6 | 内部 VBAT | 电池电压监测通道（VBAT/3） |
| 7 | 内部 TEMP | 芯片温度传感器通道 |

**注意**:
- 通道 0-5 使用前需先通过 GPIO 驱动将对应引脚配置为 `LISA_GPIO_MODE_ANALOG` 模式
- 通道 6 (VBAT) 和通道 7 (TEMP) 为内部通道，无需配置引脚

### 参考电压选择

驱动支持以下参考电压（通过 `lisa_adc_channel_setup()` 配置）:

| 参考电压枚举 | 说明 | 电压计算 |
|-------------|------|--------|
| `LISA_ADC_REF_VDD_1V2` | 固定 1.2V |
| `LISA_ADC_REF_VDD_3V6` | 固定 3.6V (1.2V × 3) |
| `LISA_ADC_REF_VDD_IO_AUTO` | VDD_IO 自动分压 | VDD_IO/2 (VDD_IO≤2.4V) 或 VDD_IO/3 (VDD_IO>2.4V) |
| `LISA_ADC_REF_VDD_IO_AUTO_MUL3` | VDD_IO 自动分压 + 3倍缓冲 | (VDD_IO/2 或 VDD_IO/3) × 3 |
| `LISA_ADC_REF_EXTERNAL` | 外部参考电压 | 依赖外部 Vref_ext 电路 |

**重要说明**:

1. **固定电压类型** (1.2V, 3.6V): 电压值固定，用户直接使用
2. **VDD_IO 自动分压** (`LISA_ADC_REF_VDD_IO_AUTO`):
   - 硬件会**自动检测**外部 VDD_IO 电压并选择内部分压系数
   - VDD_IO ≤ 2.4V 时，分压系数 1/2，参考电压 = VDD_IO / 2
   - VDD_IO > 2.4V 时，分压系数 1/3，参考电压 = VDD_IO / 3
   - 用户需要根据实际 VDD_IO 电压计算参考电压值

> **硬件限制警告**: 实际可测量的外部电压不能超过 VDD_IO (默认 3.3V)。
> 超过此电压可能损坏芯片。请确保输入信号经过适当的分压或限幅处理。

## 电压转换

使用 `LISA_ADC_RAW_TO_MV` 宏进行转换：

```c
uint32_t voltage_mv = LISA_ADC_RAW_TO_MV(raw_value, reference_mv, resolution_bits);
```

**参数**:
- `raw_value`: ADC 原始采样值
- `reference_mv`: 参考电压（毫伏）
- `resolution_bits`: 分辨率（位数，如 10）

**示例**:
```c
// 10-bit ADC, 1.2V 参考电压，原始值 512
uint32_t voltage = LISA_ADC_RAW_TO_MV(512, 1200, 10);
// 结果: (512 × 1200) / 1024 = 600 mV
```

## 注意事项

1. **通道配置**: 建议在读取通道之前先调用 `lisa_adc_channel_setup()` 配置通道参数
2. **引脚配置**: 使用通道 0-5 前必须先将对应引脚配置为 ADC 模拟输入模式
3. **特殊通道**: 通道 6 (VBAT) 和通道 7 (TEMP) 为内部通道，无需配置引脚
4. **通道范围**: 支持通道 0-7（0-5：外部通道，6：VBAT，7：TEMP）
5. **VBAT 分压**: VBAT 通道测量的是实际电池电压的 1/3，计算实际电压时需乘以 3
6. **温度转换**: 温度传感器的原始值需根据芯片数据手册提供的公式进行温度转换
7. **参考电压计算**: 对于依赖硬件的参考电压（VDD_IO 系列、外部参考），用户需要根据实际电路确定电压值用于转换计算
8. **分辨率支持**: ARCS 平台目前仅支持 10-bit 分辨率
9. **线程安全**: 驱动内部已实现线程保护，可在多线程环境中使用
10. **转换时间**: 每次读取会触发一次 ADC 转换，需等待转换完成（通常几微秒到几毫秒）
11. **输入范围**: 外部通道输入电压应在 0V 到参考电压之间，超出范围可能损坏硬件或得到错误结果
12. **未配置通道**: 如果读取未配置的通道，将自动使用默认配置（1.2V 参考电压，10-bit 分辨率）
