# LISA ADC 温度传感器读取示例

## 功能说明

本示例演示如何使用 LISA ADC 驱动读取内部温度传感器并进行温度计算。

## 硬件连接

无需外部硬件连接。

- **通道 7**: 芯片内部温度传感器（无需配置PINMUX）

## 示例内容

1. 初始化 ADC 设备
2. 配置温度传感器通道参数（1.2V 参考电压，10-bit 分辨率）
3. 设置温度校准参数
4. 循环读取温度传感器通道的原始值
5. 将原始值转换为电压值并计算温度

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
[I][sample] === LISA ADC Temperature Sensor Read Example ===
[I][sample] adc0 device ready
[I][sample] Temperature sensor channel 7 configured: 1.2V reference, 10-bit resolution
[I][sample] Start reading temperature sensor (channel 7)...

[I][sample] Temperature: raw=0x258 (600), voltage=0.703125V, temp=29.45 C
[I][sample] Temperature: raw=0x259 (601), voltage=0.704297V, temp=29.73 C
[I][sample] Temperature: raw=0x25a (602), voltage=0.705469V, temp=30.01 C
...
```

## 参数说明

- **参考电压**: 1.2V（`LISA_ADC_REF_VDD_1V2`）
- **分辨率**: 10-bit（0-1023）
- **测量范围**: 0-1.2V
- **校准温度**: 29.8℃（示例值，需根据实际环境调整）
- **校准电压**: 0.73125V（示例值，需根据实际测量调整）
- **温度计算公式**: `temperature = temp_cal + (temp_cal + 273.15) / voltage_cal × (voltage - voltage_cal)`

## 关键代码

```c
/* 配置温度传感器通道 */
lisa_adc_channel_config_t temp_config = {
    .reference = LISA_ADC_REF_VDD_1V2,
    .resolution = LISA_ADC_RESOLUTION_10BIT,
};
lisa_adc_channel_setup(adc_dev, 7, &temp_config);

/* 设置校准参数 */
temp_calibration_t temp_cal = {
    .temp_cal = 29.8f,          // 校准温度
    .voltage_cal = 0.73125f     // 校准点电压
};

/* 读取并计算温度 */
uint16_t temp_raw = 0;
lisa_adc_read(adc_dev, 7, &temp_raw);

float voltage = adc_raw_to_voltage(temp_raw, 1.2f, 10);
float die_temp = calc_temperature(&temp_cal, voltage);
```

## 注意事项

1. **内部通道**: 温度传感器是内部通道 7，无需配置 GPIO 引脚
2. **推荐参考电压**: 温度传感器建议使用 1.2V 参考电压
3. **校准参数**: 示例中的校准参数需要根据实际测量环境进行调整
4. **自热效应**: 芯片工作时会产生热量，测得的温度高于环境温度
5. **单点校准**: 本示例使用单点校准，精度有限，如需更高精度可使用多点校准
