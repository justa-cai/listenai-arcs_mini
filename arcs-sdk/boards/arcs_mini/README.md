# ARCS Mini 开发板

## 板型概述

ARCS Mini 是一款紧凑型开发板，适用于快速原型开发和嵌入式应用学习。

**板型标识：** `arcs_mini`

## 板型外观

![ARCS Mini 开发板](../../assets/arcs_mini_board.png)
*ARCS Mini 开发板外观图（标注序号说明见下表）*

## 硬件接口说明

下表详细说明了开发板上的各个接口和组件：

| 序号 | 接口/组件 | 说明 |
|------|----------|------|
| 1 | 预留烧录串口 | 引出可用于烧录 boot 固件和查看日志的引脚。注意：boot 固件已在工厂预烧录，一般不需要使用此接口 |
| 2 | 屏幕SPI接口 | 屏幕连接器，用于连接开发板默认附带的显示屏 |
| 3 | 摄像头DVP接口 | 摄像头 FPC 连接器，用于连接开发板默认附带的摄像头 |
| 4 | I/O拓展接口 | 引出 6 个可编程 GPIO 和一组电源/GND，可用于连接外部传感器或其他外部设备。<br>引脚定义：PCBA 放置如图所示时，从下到上依次为 VCC(3.3V)、GND、A04、A05、A06、A07、A08、A09 |
| 5 | 主功能按键 | 用于开发板开关机等交互触发操作 |
| 6 | 充放电LED | 充电状态指示灯，充电时红色常亮 |
| 7 | USB接口 | TypeC 接口，提供供电、充电、固件烧录功能（需要已烧录 boot 固件） |
| 8 | 可编程LED | 支持通过编程控制的 LED，使用 B01 引脚 |
| 9 | RST按钮 | Reset 按键，短按该按键会对开发板执行复位操作 |
| 10 | 麦克风接口 | 用于连接开发板默认附带的驻极体麦克风，用户也可以替换或外接其他麦克风 |
| 11 | 扬声器接口 | 用于连接开发板默认附带的扬声器，用户也可以替换或外接其他扬声器 |
| 12 | 锂电池接口 | 用于连接开发板默认附带的锂电池 |

## 硬件特性

### 主要参数

- **芯片平台**：基于 ARCS 架构
- **板型尺寸**：紧凑型设计
- **适用场景**：快速原型开发、教学演示、功能验证

### 外设支持

#### 串口通信（UART）

| 串口 | 引脚 | 功能 | 功能码 | 说明 |
|------|------|------|--------|------|
| UART0 | PAD_A[3] | AP_LOG_TX | 2 | 主串口 TX（AP 日志输出） |
| UART0 | PAD_A[2] | AP_LOG_RX | 2 | 主串口 RX（AP 日志输出） |
| UART2 | PAD_B[2] | CP_LOG_TX | 3 | 辅助串口 TX（CP 日志输出） |

#### I2C 总线

| 总线 | 引脚 | 功能 | 功能码 | 说明 |
|------|------|------|--------|------|
| I2C0 | PAD_B[6] | SCL | 8 | 主 I2C 时钟线（摄像头） |
| I2C0 | PAD_B[7] | SDA | 8 | 主 I2C 数据线（摄像头） |

#### SPI 总线

| 总线 | 引脚 | 功能 | 功能码 | 说明 |
|------|------|------|--------|------|
| SPI0 | PAD_A[22] | CS | 5 | LCD SPI 片选 |
| SPI0 | PAD_A[24] | MOSI | 5 | LCD SPI 主出从入 |
| SPI0 | PAD_A[25] | CLK | 5 | LCD SPI 时钟线 |

#### GPIO（通用输入/输出）

| 引脚组 | 引脚 | 功能名称 | 功能码 | 说明 |
|--------|------|----------|--------|------|
| GPIO_A | PAD_A[0] | CAMERA_RST | 1 | 摄像头复位控制 |
| GPIO_A | PAD_A[1] | PA_EN | 1 | 功放使能控制 |
| GPIO_A | PAD_A[4-9] | 可编程 GPIO | 0 | IO 扩展接口 |
| GPIO_A | PAD_A[23] | LCD_CD | 0 | LCD 命令/数据选择 |
| GPIO_A | PAD_A[27] | LCD_TE | 0 | LCD 撕裂效应信号 |
| GPIO_B | PAD_B[0] | USB_DET | 0 | USB 检测 |
| GPIO_B | PAD_B[1] | LED | 0 | LED 指示灯控制 |
| GPIO_B | PAD_B[3] | POWER_EN | 0 | 电源使能控制 |
| GPIO_B | PAD_B[4] | POWER_KEY | 0 | 电源按键输入 |
| GPIO_B | PAD_B[8] | CHARGE_DET | 0 | 充电检测 |
| GPIO_B | PAD_B[9] | LCD_RST | 0 | LCD 复位控制 |

#### PWM（脉宽调制）

| 功能名称 | 引脚 | 功能码 | 说明 |
|----------|------|--------|------|
| LCD_PWM | PAD_A[21] | 12 | LCD 背光亮度调节 |

#### SDIO

| 接口 | 引脚范围 | 引脚数量 | 功能码 | 说明 |
|------|----------|----------|--------|------|
| SDIO | PAD_A[4-9] | 6 | 15 | SD 卡/eMMC 接口（数据/控制线） |

#### DVP（数字视频接口）

| 接口 | 引脚范围 | 引脚数量 | 功能码 | 说明 |
|------|----------|----------|--------|------|
| DVP | PAD_A[10-20, 26] | 12 | 16 | 摄像头接口（数据线） |

#### ADC（模数转换）

| 功能 | 通道引脚 | 功能码 | 说明 |
|------|----------|--------|------|
| ADC | PAD_B[5] | 3 | 电池电压检测（AON 域） |

## 引脚配置说明

所有外设的引脚复用配置定义在 [pinmux.h](pinmux.h) 和 [pinmux.c](pinmux.c) 中。这些文件由 LISA Pinmux Tool 自动生成，建议使用工具进行修改。

### 引脚别名

为方便使用，常用引脚已定义别名（参见 [pinmux.h:26-41](pinmux.h#L26-L41)）：
```c
CAMERA_RST_PIN   // PAD_A[0]
PA_EN_PIN        // PAD_A[1]
AP_LOG_RX_PIN    // PAD_A[2]
AP_LOG_TX_PIN    // PAD_A[3]
LCD_PWM_PIN      // PAD_A[21]
LCD_CD_PIN       // PAD_A[23]
LCD_TE_PIN       // PAD_A[27]
CP_LOG_TX_PIN    // PAD_B[2]
POWER_EN_PIN     // PAD_B[3]
POWER_KEY_PIN    // PAD_B[4]
BAT_ADC_PIN      // PAD_B[5]
CHARGE_DET_PIN   // PAD_B[8]
LCD_RST_PIN      // PAD_B[9]
USB_DET_PIN      // PAD_B[0]
LED_PIN          // PAD_B[1]
```

### 引脚功能说明

下表为 LS26 芯片在 Arcs-Mini 开发板上的 GPIO 分配使用列表：

| 引脚/端口 | 主功能 | 说明/连接外设 | 分类 |
|-----------|--------|---------------|------|
| GPIOA_00 | CAM_RST | 摄像头复位 | 摄像头 |
| GPIOA_01 | PA_EN | 功放使能控制 | 音频 |
| GPIOA_02 | UART0 RX | 预留烧录串口 | 烧录&日志 |
| GPIOA_03 | UART0 TX | 预留烧录串口 | 烧录&日志 |
| GPIOA_04 | 可编程 GPIO | IO 扩展接口 | I/O 扩展 |
| GPIOA_05 | 可编程 GPIO | IO 扩展接口 | I/O 扩展 |
| GPIOA_06 | 可编程 GPIO | IO 扩展接口 | I/O 扩展 |
| GPIOA_07 | 可编程 GPIO | IO 扩展接口 | I/O 扩展 |
| GPIOA_08 | 可编程 GPIO | IO 扩展接口 | I/O 扩展 |
| GPIOA_09 | 可编程 GPIO | IO 扩展接口 | I/O 扩展 |
| GPIOA_10 | HSYNC | DVP 摄像头水平同步 | 摄像头 |
| GPIOA_11 | VSYNC | DVP 摄像头垂直同步 | 摄像头 |
| GPIOA_12 | DVP_CLK | DVP 摄像头像素时钟 | 摄像头 |
| GPIOA_13 | DVP_D4 | DVP 摄像头数据线 4 | 摄像头 |
| GPIOA_14 | DVP_D5 | DVP 摄像头数据线 5 | 摄像头 |
| GPIOA_15 | DVP_D6 | DVP 摄像头数据线 6 | 摄像头 |
| GPIOA_16 | DVP_D7 | DVP 摄像头数据线 7 | 摄像头 |
| GPIOA_17 | DVP_D8 | DVP 摄像头数据线 8 | 摄像头 |
| GPIOA_18 | DVP_D9 | DVP 摄像头数据线 9 | 摄像头 |
| GPIOA_19 | DVP_D10 | DVP 摄像头数据线 10 | 摄像头 |
| GPIOA_20 | DVP_D11 | DVP 摄像头数据线 11 | 摄像头 |
| GPIOA_21 | LCD_PWM | LCD 背光 PWM 控制 | 显示 |
| GPIOA_22 | LCD_SPI0_CS | LCD SPI 片选 | 显示 |
| GPIOA_23 | LCD_GPIO_WR | LCD 命令/数据选择 | 显示 |
| GPIOA_24 | LCD_SPI0_MOSI | LCD SPI 数据输出 | 显示 |
| GPIOA_25 | LCD_SPI0_CLK | LCD SPI 时钟 | 显示 |
| GPIOA_26 | DVP_MCLK | DVP 摄像头主时钟 | 摄像头 |
| GPIOA_27 | LCD_TE | LCD 撕裂效应信号 | 显示 |
| GPIOA_28 | MIC1 AEC_P | 硬回采正极 | 音频 |
| GPIOA_29 | MIC1 AEC_N | 硬回采负极 | 音频 |
| GPIOA_30 | MIC0 INP | 麦克风输入正极 | 音频 |
| GPIOA_31 | MIC0 INN | 麦克风输入负极 | 音频 |
| GPIOB_00 | USB_DET | USB 插入检测 | 通信 (USB) |
| GPIOB_01 | LED | 用户 LED | GPIO |
| GPIOB_02 | uart2_txd | 预留烧录串口 TX | 烧录&日志 |
| GPIOB_03 | POW_EN | 电源使能 | 电源 |
| GPIOB_04 | power_KEY | 主功能按键 | 输入/控制 |
| GPIOB_05 | ADC_Bat_Vol | 电池电量检测 | 电源 |
| GPIOB_06 | SDA0 | I2C0 数据线（摄像头） | 通信 (I2C) |
| GPIOB_07 | CLK0 | I2C0 时钟线（摄像头） | 通信 (I2C) |
| GPIOB_08 | CHARGE_DET | 充电状态检测 | 电源 |
| GPIOB_09 | LCD_RST | LCD 复位 | 显示 |
| FLASH_CS_N | FLASH_CS_N | Boot Flash 片选 | 存储 (Flash) |
| FLASH_MISO | FLASH_MISO | Boot Flash 数据输入 | 存储 (Flash) |
| FLASH_WP_N | FLASH_WP_N | Boot Flash 写保护 | 存储 (Flash) |
| FLASH_HOLD_N | FLASH_HOLD_N | Boot Flash 保持 | 存储 (Flash) |
| FLASH_CLK | FLASH_CLK | Boot Flash 时钟 | 存储 (Flash) |
| FLASH_MOSI | FLASH_MOSI | Boot Flash 数据输出 | 存储 (Flash) |
| USB_DP | USB_DP | USB 差分正极 | 通信 (USB) |
| USB_DM | USB_DM | USB 差分负极 | 通信 (USB) |
| MIC_BIAS | MIC_BIAS | 麦克风偏置 | 音频 |
| LIN_OUTP | LIN_OUTP | 差分输出正极 | 音频 |
| LIN_OUTN | LIN_OUTN | 差分输出负极 | 音频 |

