# ARCS EVB 评估板

## 板型概述

ARCS EVB 是一款功能丰富的评估板，集成了多种外设接口，适用于完整的产品原型开发和功能评估。

**板型标识：** `arcs_evb`

## 板型外观

![ARCS EVB 评估板](../../assets/arcs_evb_board.png)
*ARCS EVB 评估板外观图（标注序号说明见下表）*

## 硬件接口说明

下表详细说明了开发板上的各个接口和组件：

| 序号 | 接口/组件 | 说明 |
|------|----------|------|
| 1 | USB接口 | TypeC 接口，提供供电和充电功能 |
| 2 | 烧录接口 | TypeC 接口，提供日志输出和固件烧录功能。注意：需要先烧录 boot 固件 |
| 3 | 开关 | 控制整个开发板的主电源开关 |
| 4 | 统一为I/O接口 | 引出 30 个 IO 口以及多组电源 |
| 5 | 扬声器接口 | 用于连接开发板默认附带的扬声器，用户也可以替换或外接其他扬声器 |
| 6 | 麦克风 | 用于连接开发板默认配套的驻极体麦克风，方便用户更换或外引麦克风。 |
| 7 | 硬回采开关 | 该开关支持单麦克风硬回采、双麦克风软回采 |
| 8 | I/O电源指示灯 | 可编程控制的 LED 灯，使用 B09 引脚 |
| 9 | 电源LED | 指示开发板的供电状态，供电正常时 LED 点亮 |
| 10 | ADC按键 | ADC 按键，通过 GPADC 检测电压来确认其值 |
| 11 | RST按键 | Reset 按键，短按该按键会对开发板执行复位操作 |
| 12 | BOOT按键 | 长按该按键上电会进入芯片烧录模式 |
| 13 | TF卡槽 | 用于插入 TF 存储卡 |
| 14 | 屏幕连接器 | 用于连接开发板默认附带的显示屏。说明：LCD 适配板一侧用于固定 LCD，另一侧兼容不同的 QSPI 或 SPI LCD 屏幕。不同屏幕需要对应不同的适配板 |
| 15 | 摄像头DVP接口 | 摄像头 FPC 连接器，用于连接开发板默认附带的摄像头 |

## 硬件特性

### 主要参数

- **芯片平台**：基于 ARCS 架构
- **板型尺寸**：标准评估板尺寸
- **适用场景**：产品原型开发、功能评估、方案验证

### 外设支持

#### 串口通信（UART）

| 串口 | 引脚 | 功能 | 功能码 | 说明 |
|------|------|------|--------|------|
| UART0 | PAD_A[3] | CP_LOG_TX | 2 | 主串口 TX（CP 日志输出） |
| UART0 | PAD_A[2] | CP_LOG_RX | 2 | 主串口 RX（CP 日志输出） |
| UART1 | PAD_A[21] | AP_LOG_TX | 3 | 辅助串口 TX（AP 日志输出） |

#### I2C 总线

| 总线 | 引脚 | 功能 | 功能码 | 说明 |
|------|------|------|--------|------|
| I2C0 | PAD_A[22] | SCL | 8 | 主 I2C 时钟线 |
| I2C0 | PAD_A[23] | SDA | 8 | 主 I2C 数据线 |

#### SPI 总线

| 总线 | 引脚 | 功能 | 功能码 | 说明 |
|------|------|------|--------|------|
| SPI1 | PAD_B[5] | CLK | 6 | 主 SPI 时钟线 |
| SPI1 | PAD_B[3] | MISO | 6 | 主 SPI 主入从出 |
| SPI1 | PAD_B[1] | MOSI | 6 | 主 SPI 主出从入 |

#### GPIO（通用输入/输出）

| 引脚组 | 引脚 | 功能名称 | 功能码 | 说明 |
|--------|------|----------|--------|------|
| GPIO_A | PAD_A[1] | LCD_RST | 1 | LCD 复位控制 |
| GPIO_A | PAD_A[24] | TP_INT | 0 | 触摸屏中断输入 |
| GPIO_A | PAD_A[25] | TP_RST | 0 | 触摸屏复位控制 |
| GPIO_A | PAD_A[27] | PA_EN | 0 | 功放使能控制 |
| GPIO_B | PAD_B[0] | LCD_CD | 0 | LCD 命令/数据选择 |
| GPIO_B | PAD_B[7] | CAMERA_PWDN | 0 | 摄像头电源控制 |
| GPIO_B | PAD_B[8] | LCD_TE | 0 | LCD 撕裂效应信号 |
| GPIO_B | PAD_B[9] | LED | 0 | LED 指示灯控制 |

#### PWM（脉宽调制）

| 功能名称 | 引脚 | 功能码 | 说明 |
|----------|------|--------|------|
| LCD_PWM | PAD_A[0] | 12 | LCD 背光亮度调节 |

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
| ADC | PAD_B[6] | 3 | 模拟信号采集（AON 域） |

### 引脚功能说明

下表为 LS26 芯片在 Arcs-EVB 开发板上的 GPIO 分配使用列表：

| 引脚/端口 | 主功能 | 复用功能 | 说明/连接外设 | 分类 |
|-----------|--------|----------|---------------|------|
| GPIOA_00 | LCD_PWM | CJTAG_TCK | LCM模组背光控制 | 显示 |
| GPIOA_01 | LCD RST | CJTAG_TMS | LCM模组复位 | 显示 |
| GPIOA_02 | UART0 RX | | LOAD&打印CP日志 | 烧录&日志 |
| GPIOA_03 | UART0 TX | | LOAD&打印CP日志 | 烧录&日志 |
| GPIOA_04 | SD DAT1 | | TF CARD | 存储 |
| GPIOA_05 | SD DAT0 | | TF CARD | 存储 |
| GPIOA_06 | SD CLK | | TF CARD | 存储 |
| GPIOA_07 | SD CMD | | TF CARD | 存储 |
| GPIOA_08 | SD DAT3 | | TF CARD | 存储 |
| GPIOA_09 | SD DAT2 | | TF CARD | 存储 |
| GPIOA_10 | vic_h_sync | | DVP摄像头 | 兼容SPI摄像头 |
| GPIOA_11 | vic_v_sync | | DVP摄像头 | 兼容SPI摄像头 |
| GPIOA_12 | vic_pixel_clk | | DVP摄像头 | 兼容SPI摄像头 |
| GPIOA_13 | vic_pixel_data4 | | DVP摄像头 | 兼容SPI摄像头 |
| GPIOA_14 | vic_pixel_data5 | | DVP摄像头 | 兼容SPI摄像头 |
| GPIOA_15 | vic_pixel_data6 | | DVP摄像头 | 兼容SPI摄像头 |
| GPIOA_16 | vic_pixel_data7 | | DVP摄像头 | 兼容SPI摄像头 |
| GPIOA_17 | vic_pixel_data8 | | DVP摄像头 | 兼容SPI摄像头 |
| GPIOA_18 | vic_pixel_data9 | | DVP摄像头 | 兼容SPI摄像头 |
| GPIOA_19 | vic_pixel_data10 | | DVP摄像头 | 兼容SPI摄像头 |
| GPIOA_20 | vic_pixel_data11 | | DVP摄像头 | 兼容SPI摄像头 |
| GPIOA_21 | UART1_TX | | 打印AP日志 | 调试 |
| GPIOA_22 | I2C0 SDA | | 摄像头和TP复用 | LCM模组 |
| GPIOA_23 | I2C0 SCL | | 摄像头和TP复用 | LCM模组 |
| GPIOA_24 | TP INT | | 触摸中断 | LCM模组 |
| GPIOA_25 | TP RST | | 触摸复位 | LCM模组 |
| GPIOA_26 | VIC_CLK_OUT | | MCLK (主时钟) | 摄像头 |
| GPIOA_27 | PA_EN | | 功放MUTE | 音频 |
| GPIOA_28 | MIC1 INP | | 硅麦 | 音频 |
| GPIOA_29 | MIC1 INN | | 硅麦 | 音频 |
| GPIOA_30 | MIC0 INP | | 硅麦 | 音频 |
| GPIOA_31 | MIC0 INN | | 硅麦 | 音频 |
| GPIOB_00 | LCD SPI MISO | D1 | LCM模组 (数据线1) | 显示 (SPI) |
| GPIOB_01 | LCD SPI MOSI | D0 | LCM模组 (数据线0) | 显示 (SPI) |
| GPIOB_02 | LCD SPI HOLD | D3 | LCM模组 (数据线3) | 显示 (SPI) |
| GPIOB_03 | LCD SPI CLK | CLK | LCM模组 (时钟线) | 显示 (SPI) |
| GPIOB_04 | LCD SPI WP | D2 | LCM模组 (数据线2) | 显示 (SPI) |
| GPIOB_05 | LCD_SPI CS | CS | LCM模组 (片选) | 显示 (SPI) |
| GPIOB_06 | KEY1 | | ADC按键 | 输入/控制 |
| GPIOB_07 | KEY2 | | 触摸按键 | 输入/控制 |
| GPIOB_08 | LCD_TE | | LCD TE中断 | 显示/中断 |
| GPIOB_09 | LED | | 单色指示灯 | GPIO |
| FLASH_CS_N | FLASH_CS_N | FLASH_CS_N | Boot Flash | 存储 (Flash) |
| FLASH_MISO | FLASH_MISO | FLASH_MISO | Boot Flash | 存储 (Flash) |
| FLASH_WP_N | FLASH_WP_N | FLASH_WP_N | Boot Flash | 存储 (Flash) |
| FLASH_HOLD_N | FLASH_HOLD_N | FLASH_HOLD_N | Boot Flash | 存储 (Flash) |
| FLASH_CLK | FLASH_CLK | FLASH_CLK | Boot Flash | 存储 (Flash) |
| FLASH_MOSI | FLASH_MOSI | FLASH_MOSI | Boot Flash | 存储 (Flash) |
| USB_DP | USB_DP | | USB口 | 通信 (USB) |
| USB_DM | USB_DM | | USB口 | 通信 (USB) |
| LIN_OUTP | LIN_OUTP | | 差分输出 正 | 音频 |
| LIN_OUTN | LIN_OUTN | | 差分输出 反 | 音频 |


