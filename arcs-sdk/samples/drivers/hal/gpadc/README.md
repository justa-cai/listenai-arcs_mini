# GPADC 示例

本示例演示 GPADC（General Purpose ADC）外设的模拟信号采样功能，包括外部引脚采样和芯片内部电压测量。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **外部引脚采样**：对 PB4、PB6、PB7 引脚进行模拟电压采样
- **内部电压测量**：测量芯片内部电压（VBAT）
- **引脚复用配置**：配置 GPIO 引脚为 GPADC 输入功能
- **周期性采样**：每秒进行一次 ADC 采样
- **电压转换**：将 ADC 数字值转换为毫伏（mV）

### GPADC 通道说明

GPADC 总共有 10 个采样通道：

| 通道类型 | 通道编号 | 引脚/功能 | 说明 |
|----------|----------|-----------|------|
| 外部通道 | VIN0 | PB2 | 外部 GPADC 采样引脚 |
| 外部通道 | VIN1 | PB3 | 外部 GPADC 采样引脚 |
| 外部通道 | VIN2 | PB4 | 外部 GPADC 采样引脚（本示例使用） |
| 外部通道 | VIN3 | PB5 | 外部 GPADC 采样引脚 |
| 外部通道 | VIN4 | PB6 | 外部 GPADC 采样引脚（本示例使用） |
| 外部通道 | VIN5 | PB7 | 外部 GPADC 采样引脚（本示例使用） |
| 内部通道 | VBAT | - | 芯片内部电压测量（本示例使用） |
| 内部通道 | TEMP | - | 芯片温度测量 |
| Keysense | - | PB2 | Keysense 采样通道 |
| Keysense | - | PB3 | Keysense 采样通道 |

### 硬件要求

- ARCS 系列开发板
- USB 转串口工具（用于查看日志输出）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/gpadc
./build.sh
```

或者在 SDK 根目录执行：

```bash
./build.sh -S samples/drivers/gpadc -C
```

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/arcs.bin -C arcs
```

参数说明：
- `-s /dev/ttyUSB0`：串口设备路径（根据实际情况修改）
- `-b 3000000`：烧录波特率
- `0x0`：烧录起始地址
- `build/arcs.bin`：烧录固件路径

> 💡 详细烧录步骤请参考 {ref}`快速开始 - 烧录运行 <flashing>`。

### 3. 查看输出

烧录完成后，复位开发板，通过串口工具（如 minicom）查看日志输出。

## 🔧 配置说明

### 项目配置

本示例在 `prj.conf` 中配置：

```conf
CONFIG_MEM_CONFIG=y
```

### GPADC 参数配置

| 参数 | 值 | 说明 |
|------|-----|------|
| 参考电压（ref） | 0 | 使用 Vbg 作为参考电压（1.2V） |
| VinBuf 使能 | 1 | 实际采样值为 ADC 值的 3 倍 |
| 采样通道 | VIN2, VIN4, VIN5, VBAT | 选择的采样通道 |
| DMA 模式 | 禁用 | 不使用 DMA 传输 |
| 采样周期 | 1000ms | 每秒采样一次 |

### 参考电压选项

```c
/*
 * ref=0：参考电压为 Vbg, 1.2V
 * ref=1：参考电压为 VDD_VA, 1.2V
 * ref=2：参考电压为 VDD_IO / 2 或者 VDD_IO / 3
 * ref=3：参考电压为 Vref_ext, 即外部参考电压
 */
```

## 📋 代码解析

### 关键代码段

#### 1. 引脚复用配置

```c
/* 配置 PB4 作为 GPADC 的输入引脚 */
/* AON_MUX 设置 PB4 为 ANA 引脚 */
AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPADC_VIN2_PIN_NUM, 
                              CSK_AON_IOMUX_FUNC_ALTER3);
/* ANA_MUX 设置 PB4 为 GPADC 输入 */
IP_AON_IOMUX->REG_PAD_AON_GPIOB_04.bit.PAD_AON_GPIOB_04_ANA_SEL = 
    CSK_ANA_IOMUX_FUNC_DEFAULT;

/* 设置 PB6 为 GPADC 输入引脚 */
AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPADC_VIN4_PIN_NUM, 
                              CSK_AON_IOMUX_FUNC_ALTER3);
IP_AON_IOMUX->REG_PAD_AON_GPIOB_06.bit.PAD_AON_GPIOB_06_ANA_SEL = 
    CSK_ANA_IOMUX_FUNC_DEFAULT;

/* 设置 PB7 为 GPADC 输入引脚 */
AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPADC_VIN5_PIN_NUM, 
                              CSK_AON_IOMUX_FUNC_ALTER3);
IP_AON_IOMUX->REG_PAD_AON_GPIOB_07.bit.PAD_AON_GPIOB_07_ANA_SEL = 
    CSK_ANA_IOMUX_FUNC_DEFAULT;
```

#### 2. GPADC 初始化

```c
/* 初始化 GPADC */
HAL_GPADC_Initialize(GPADC());

/* 设置 ADC 的采样通道 */
HAL_GPADC_Control(GPADC(), CSK_GPADC_CHANNEL_SEL_2 |      // VIN2(PB4)
                           CSK_GPADC_CHANNEL_SEL_4 |      // VIN4(PB6)
                           CSK_GPADC_CHANNEL_SEL_5 |      // VIN5(PB7)
                           CSK_GPADC_CHANNEL_SEL_VBAT |   // 内部电压
                           CSK_GPADC_DMA_ENABLE(0));      // 禁用 DMA

/* 设置参考电压为 Vbg, 1.2V */
HAL_GPADC_SetVrefSel(GPADC(), 0);

/* 使能 VinBuf，实际采样值为 ADC 值的 3 倍 */
HAL_GPADC_SetVinBuf_Enable(GPADC(), 1);
```

#### 3. ADC 采样

```c
/* 启动 GPADC 采样 */
HAL_GPADC_Start(GPADC());

/* 等待采样完成 */
HAL_GPADC_PollForConversion(GPADC(), 0);
```

#### 4. 读取采样结果

```c
uint32_t adc_value;

/* 获取 VIN2 的采样结果 */
adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL_SEL_2);
printf("channel type id %d, adc value 0x%lx/%dmV\n", 
       CSK_GPADC_CHANNEL2, adc_value, 
       (uint16_t)(adc_value*1000.0/1024*1.2*3));

/* 获取 VIN4 的采样结果 */
adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL_SEL_4);
printf("channel type id %d, adc value 0x%lx/%dmV\n", 
       CSK_GPADC_CHANNEL4, adc_value, 
       (uint16_t)(adc_value*1000.0/1024*1.2*3));

/* 获取 VIN5 的采样结果 */
adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL_SEL_5);
printf("channel type id %d, adc value 0x%lx/%dmV\n", 
       CSK_GPADC_CHANNEL5, adc_value, 
       (uint16_t)(adc_value*1000.0/1024*1.2*3));

/* 获取芯片内部电压的采样结果 */
adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL_SEL_VBAT);
printf("channel type id %d, adc value 0x%lx/%dmV\n", 
       CSK_GPADC_VBAT, adc_value, 
       (uint16_t)(adc_value*1000.0/1024*1.2*3));
```

#### 5. 主函数

```c
int main(int argc, char **argv)
{
    printf("Hello, world! GPADC\n");
    
    gpadc_test();
    
    return 0;
}

static void gpadc_test(void)
{
    /* 引脚配置 */
    /* ... */
    
    /* GPADC 初始化 */
    /* ... */
    
    /* 周期性采样 */
    for(;;){
        printf("start adc ..............................................\n");
        
        /* 启动采样并等待完成 */
        HAL_GPADC_Start(GPADC());
        HAL_GPADC_PollForConversion(GPADC(), 0);
        
        /* 读取并打印采样结果 */
        /* ... */
        
        vTaskDelay(pdMS_TO_TICKS(1000));  // 延时 1 秒
    }
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志（具体采样值与实际情况有关）：

```
Running on hart-id: 1
Hello, world! GPADC
start adc ..............................................
channel type id 6, adc value 0x359/3012mV
channel type id 8, adc value 0x2eb/2626mV
channel type id 9, adc value 0xa0/562mV
channel type id 0, adc value 0x3a8/3290mV
start adc ..............................................
channel type id 6, adc value 0x13a/1103mV
channel type id 8, adc value 0x1ec/1729mV
channel type id 9, adc value 0xa0/562mV
channel type id 0, adc value 0x3a7/3287mV
start adc ..............................................
channel type id 6, adc value 0x13b/1107mV
channel type id 8, adc value 0x1eb/1726mV
channel type id 9, adc value 0xa1/566mV
channel type id 0, adc value 0x3a8/3290mV
...
```

输出说明：
- `Running on hart-id: 1`：程序运行在 HART 1（CP 核）
- `Hello, world! GPADC`：程序启动信息
- `start adc ...`：开始新一轮 ADC 采样
- `channel type id 6`：VIN2 通道（PB4）采样结果
- `channel type id 8`：VIN4 通道（PB6）采样结果
- `channel type id 9`：VIN5 通道（PB7）采样结果
- `channel type id 0`：VBAT 通道（芯片内部电压）采样结果
- 格式：`0x359/3012mV` 表示 ADC 原始值为 0x359，换算后电压为 3012mV

## ⚠️ 注意事项

1. **引脚复用配置**：
   - 需要同时配置 AON_MUX 和 ANA_MUX
   - 具体配置见芯片手册的 APPENDIX 章节

2. **电压换算公式**：
   ```
   电压(mV) = ADC值 × (1000 / 1024) × 参考电压 × VinBuf倍数
            = ADC值 × (1000 / 1024) × 1.2 × 3
   ```
   - ADC 分辨率：10 位（0-1023）
   - 参考电压：1.2V
   - VinBuf 使能时倍数为 3

3. **VinBuf 功能**：
   - `VinBuf_Enable = 1`：实际采样值是 ADC 采样值的 3 倍
   - 用于扩展 ADC 的测量范围

4. **通道选择**：
   - 使用位或运算（`|`）同时选择多个通道
   - 可根据需要选择其他通道

5. **采样方式**：
   - 使用轮询方式（`PollForConversion`）等待采样完成
   - 本示例未使用 DMA 模式

6. **周期性采样**：
   - 使用 FreeRTOS 延时函数实现周期性采样
   - 延时时间为 1000ms（1 秒）

7. **通道 ID 映射**：
   - 代码中的通道 ID 与日志输出的 ID 可能不同
   - 例如：VIN2 对应 channel type id 6

8. **采样精度**：
   - GPADC 为 10 位分辨率
   - 实际采样值受参考电压、VinBuf 配置影响
