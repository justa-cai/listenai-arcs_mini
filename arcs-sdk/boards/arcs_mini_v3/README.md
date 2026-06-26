# ARCS Mini v3

## 板型概述

ARCS Mini v3 是 `arcs_mini` 系列的 v3 硬件板型，板型标识为 `arcs_mini_v3`。

该板型使用 `projects_data.json` 记录 Pinmux 工程数据，`pinmux.c` 和 `pinmux.h` 由 LISA Pinmux Tool 生成。

## 构建

```bash
./build.sh -S ./apps/arcs-mini -DBOARD=arcs_mini_v3
```

根目录 `build.sh` 会默认设置 `BOARD_SEARCH_PATH=$PROJECT_TOP/boards`。如果根目录没有同名外部板型，构建系统会自动回退到 SDK 内置板型 `arcs-sdk/boards/arcs_mini_v3`。

## 主要外设映射

| 外设 | 引脚 | 功能码 | 说明 |
|------|------|--------|------|
| UART0 | PA02/PA03 | 2 | CP 日志 RX/TX |
| UART1 | PB02 | 3 | AP 日志 TX |
| UART2 | PB06/PB07 | 4 | 外部串口 TX/RX |
| I2C1 | PB00/PB01 | 8 | 摄像头控制总线 |
| SPI0 | PA22/PA24/PA25 | 5 | LCD CS/MOSI/CLK |
| PWM CH1 | PA21 | 12 | LCD 背光 |
| DVP | PA10-PA20, PA26 | 16 | 摄像头并口 |
| ADC CH3 | PB05 | 3 | 电池电压采样 |
| GPIO | PA00/PA01 | 1 | 4G EN/RST |
| GPIO | PB03/PB04/PB08/PB09 | 0 | POWER_EN/POWER_KEY/USB_DET/LCD_RST |

## 兼容说明

- `board.h` 中保留了现有 `apps/arcs-mini` 代码需要的兼容别名，例如 `LCD_CS_PIN`、`CAM_D0_PIN`。
- PA mute 由 EXMCU 侧 EXGPIO 控制，主 MCU 不提供本地 `PA_mute_PIN`、`PA_mute_DEVICE_NAME`、`PA_EN_PIN`、`PA_EN_DEVICE_NAME`。
- v3 没有本地 LED；PB09 按 `LCD_RST` 保留并用于 LCD reset。
- 板型 `overlay.conf` 会覆盖应用默认配置，将触摸关闭，并将 I2C 切到 I2C1。
