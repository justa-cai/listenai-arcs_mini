# LISA ADC 基础读取示例

## 功能说明

本示例演示如何使用 LISA ADC 驱动读取模拟信号并转换为电压值。

## 硬件连接

- **PB04**: ADC 通道2输入引脚（连接模拟信号源，电压范围 0-3.6V）

## 示例内容

1. 初始化 ADC 设备
2. 配置 PB04 引脚为 ADC 模拟输入
3. 配置 ADC 通道2参数（3.6V 参考电压，10-bit 分辨率）
4. 循环读取 ADC 通道2的原始值
5. 将原始值转换为电压值（毫伏）

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
[I][sample] === LISA ADC read example ===
[I][sample] PB04 configured as ADC input
[I][sample] adc0 device ready
[I][sample] Channel 2 configured: 3.6V reference, 10-bit resolution
[I][sample] Start reading ADC channel 2...

[I][sample] Channel 2: raw=0x0123 ( 291), voltage=1022 mV
[I][sample] Channel 2: raw=0x0125 ( 293), voltage=1029 mV
[I][sample] Channel 2: raw=0x0124 ( 292), voltage=1025 mV
...
```

## 参数说明

- **参考电压**: 3.6V（`LISA_ADC_REF_VDD_3V6`）
- **分辨率**: 10-bit（0-1023）
- **测量范围**: 0-3.6V
- **电压计算**: 使用 `LISA_ADC_RAW_TO_MV(raw_value, 3600, 10)` 宏转换
- **计算公式**: voltage(mV) = (raw_value × 3600) / 1024

## 关键代码

```c
/* 配置通道2使用3.6V参考电压 */
lisa_adc_channel_config_t ch_config = {
    .reference = LISA_ADC_REF_VDD_3V6,
    .resolution = LISA_ADC_RESOLUTION_10BIT,
};
lisa_adc_channel_setup(adc_dev, ADC_CHANNEL, &ch_config);

/* 读取ADC值 */
uint16_t raw_value = 0;
lisa_adc_read(adc_dev, ADC_CHANNEL, &raw_value);

/* 转换为电压值 */
uint32_t voltage = LISA_ADC_RAW_TO_MV(raw_value, 3600, 10);
```

## 注意事项

1. **输入电压范围**: 使用 3.6V 参考电压时，输入信号应在 0-3.6V 范围内
2. **通道配置**: 必须先调用 `lisa_adc_channel_setup()` 配置通道参数后再读取
3. **引脚配置**: 使用外部通道前必须先将引脚配置为 ADC 模拟输入模式
4. **其他参考电压**: 驱动还支持 1.2V、VDD_IO 等参考电压，详见驱动文档
