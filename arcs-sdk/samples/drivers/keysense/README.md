# Keysense 示例

本示例演示 Keysense（模拟按键接口）外设的使用，通过 GPADC 检测按键电压来识别按键值。

## 📖 示例说明

### Keysense 特性

Keysense 是模拟按键接口，共 2 个通道，具有以下特性：

- 每个通道最多支持 8 个按键（取决于按键电阻序列的精度）
- 通过 GPADC 检测电压后确认按键值
- 可选 kHz 计数时钟源：32KHz RC 和从 24MHz XTAL 分频的时钟
- 可配置的 wakeup 阈值和 key-measure 阈值
- 支持按键感应：wakeup 和中断
- 支持按键测量：硬件 SAR ADC 触发控制
- 支持按键 press 和按键 release 的中断请求

### 功能演示

本示例演示以下功能：

- **按键中断**：注册 key press 和 key release 中断
- **按键电压测量**：key press 中断触发时，通过 GPADC 测量按键值
- **引脚复用配置**：配置 PB2 为 Keysense0 功能

### 硬件要求

- ARCS 系列开发板
- USB 转串口工具（用于查看日志输出）

### Keysense 引脚

| 引脚 | Keysense | 备注 |
|------|----------|------|
| PB2 | keysense0 | keysense0 外设的输入通道 |
| PB3 | keysense1 | keysense1 外设的输入通道 |

本示例只使用 keysense0，可以通过触摸 PB2 引脚来触发按键事件。

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/keysense
./build.sh
```

构建成功后，会在 `build` 目录下生成 `arcs.bin` 文件。

### 2. 烧录运行

将开发板连接到 PC，执行烧录命令：

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

烧录完成后，复位开发板，通过串口工具查看日志输出。触摸 PB2 引脚可触发按键事件。

## 🔧 配置说明

### 项目配置

本示例在 `prj.conf` 中配置：

```conf
CONFIG_MEM_CONFIG=y
```

### Keysense 参数配置

| 参数 | 值 | 说明 |
|------|-----|------|
| Keysense 通道 | KEYSENSE0 | 使用 Keysense0 通道 |
| 引脚 | PB2（GPIOB_02） | Keysense0 输入引脚 |
| GPADC 通道 | KEYSENSE0 | 用于测量按键电压 |
| 中断模式 | PRESS, RELEASE | 按键按下和释放中断 |

## 📋 代码解析

### 关键代码段

#### 1. 引脚复用配置

```c
#define KEYSENSE0_PIN_NUM   2   // GPIOB_02

/* PB2 复用为 Keysense0 */
AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, KEYSENSE0_PIN_NUM, CSK_AON_IOMUX_FUNC_ALTER3);
IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL = CSK_ANA_IOMUX_FUNC_ALTER5;
```

#### 2. 按键事件回调函数

```c
static void keysense_press_event(void* param){
    printf("keysense press event generate\n");
    
    /* key press 事件触发后，可以通过 ADC 读取 keysense0 的电压，来区分和识别外部不同的按键 */
    HAL_GPADC_Start(GPADC());
    HAL_GPADC_PollForConversion(GPADC(), 0);
    uint32_t adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL_SEL_KEYSENSE0);
    printf("channel type id %d, adc value 0x%lx/%dmV\n", 
           CSK_GPADC_KEYSENSE0, adc_value, 
           (uint16_t)(adc_value*1000.0/1024*1.2));
    
    /* 关闭按键 press 的中断 */
    HAL_KEYSENSE_InterruptDisable(keysense_handler, CSK_KEYSENSE_INTERRUPT_MODE_PRESS);
}

static void keysense_release_event(void* param){
    printf("keysense release event generate\n");
    
    /* 关闭按键 release 的中断 */
    HAL_KEYSENSE_InterruptDisable(keysense_handler, CSK_KEYSENSE_INTERRUPT_MODE_RELEASE);
}
```

#### 3. Keysense 初始化

```c
static void *keysense_handler = NULL;

static void keysense_test(void)
{
    keysense_handler = KEYSENSE0();
    
    /* 配置引脚复用 */
    /* ... */
    
    /* 初始化 keysense0 */
    HAL_KEYSENSE_Initialize(keysense_handler);
    
    /* 设置阈值 */
    HAL_KEYSENSE_Control(keysense_handler, CSK_KEYSENSE_THD);
}
```

#### 4. 注册回调并使能中断

```c
/* 注册按键 release 和 press 的事件回调 */
HAL_KEYSENSE_RegisterCallback(keysense_handler, CSK_KEYSENSE_RELEASE, keysense_release_event);
HAL_KEYSENSE_RegisterCallback(keysense_handler, CSK_KEYSENSE_PRESS, keysense_press_event);

/* 使能按键 release 和 press 的中断 */
HAL_KEYSENSE_InterruptEnable(keysense_handler, CSK_KEYSENSE_INTERRUPT_MODE_RELEASE);
HAL_KEYSENSE_InterruptEnable(keysense_handler, CSK_KEYSENSE_INTERRUPT_MODE_PRESS);
```

#### 5. GPADC 初始化

```c
/* 初始化 GPADC */
HAL_GPADC_Initialize(GPADC());

/* 设置 ADC 的 KEYSENSE0 采样通道 */
HAL_GPADC_Control(GPADC(), CSK_GPADC_CHANNEL_SEL_KEYSENSE0 | CSK_GPADC_DMA_ENABLE(0)); 

HAL_GPADC_SetVrefSel(GPADC(), 0);
HAL_GPADC_SetVinBuf_Enable(GPADC(), 0);
```

#### 6. 使能 Keysense

```c
/* 使能 keysense0 */
HAL_KEYSENSE_Enable(keysense_handler);

while(1){
    vTaskDelay(pdMS_TO_TICKS(100));
}

/* 关闭 keysense */
HAL_KEYSENSE_Uninitialize(keysense_handler);
```

#### 7. 主函数

```c
int main(int argc, char **argv)
{
    printf("Hello, world! KEYSENSE\n");
    
    keysense_test();
    
    return 0;
}
```

## 🔍 预期输出

程序运行后，触摸 PB2 引脚时，串口将输出以下日志：

```
Running on hart-id: 1
Hello, world! KEYSENSE
keysense press event generate
channel type id 1, adc value 0x336/963mV
keysense release event generate
```

输出说明：
- `Running on hart-id: 1`：程序运行在 HART 1（CP 核）
- `Hello, world! KEYSENSE`：程序启动信息
- `keysense press event generate`：检测到按键按下事件
- `channel type id 1, adc value 0x336/963mV`：GPADC 测量的按键电压值
  - `0x336`：ADC 原始值
  - `963mV`：换算后的电压值
- `keysense release event generate`：检测到按键释放事件

## ⚠️ 注意事项

1. **引脚触摸测试**：
   - 本开发板 keysense0 引脚没有接到具体按键
   - 可以通过触摸 PB2 引脚来触发按键事件
   - 按键采样值可能不准，仅作为演示

2. **引脚复用配置**：
   - 需要配置 AON_IOMUX（ALTER3）
   - 需要配置 ANA_IOMUX（ALTER5）

3. **中断事件类型**：
   - `CSK_KEYSENSE_PRESS`：按键按下事件
   - `CSK_KEYSENSE_RELEASE`：按键释放事件
   - 代码中注释的还有 `CSK_KEYSENSE_WAKEUP` 和 `CSK_KEYSENSE_ADCTRIG`

4. **中断禁用**：
   - 本示例在按键事件触发后禁用对应中断
   - 实际应用中可根据需要保持中断使能

5. **GPADC 配置**：
   - 使用 KEYSENSE0 通道
   - 参考电压选择：0
   - VinBuf 使能：0（不使用 VinBuf）

6. **电压换算**：
   - 公式：`adc_value * 1000.0 / 1024 * 1.2`
   - ADC 分辨率：10 位（0-1023）
   - 参考电压：1.2V

7. **Keysense 通道**：
   - Keysense0：PB2 引脚
   - Keysense1：PB3 引脚（本示例未使用）
