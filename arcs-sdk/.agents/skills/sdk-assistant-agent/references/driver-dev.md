<!-- type: worker -->
<!-- 开发规则: README.md#开发规则 -->

# ARCS 驱动开发辅助

## 设备识别

从用户描述中识别目标设备：

| 设备类型 | 关键词 |
|----------|--------|
| GPIO | gpio、引脚、输入输出、中断引脚、按键、led |
| UART | uart、串口、收发、波特率、serial |
| SPI | spi、spi master、spi slave |
| I2C | i2c、i2c master、从机扫描 |
| Flash | flash、nor flash、读写擦除 |
| ADC | adc、模数转换、采样、温度传感器 |
| PWM | pwm、占空比、脉宽调制 |
| HWTIMER | hwtimer、硬件定时器、gpt、aon timer |
| RTC | rtc、实时时钟、闹钟、日历 |
| WDT | wdt、watchdog、看门狗 |
| SDMMC | sdmmc、sd卡、mmc、tf卡 |
| I2S | i2s、音频接口、pcm 流 |
| Display | display、显示屏、lcd 刷屏、lvgl 显示 |
| Touch | touch、触摸、触控 |
| Audio | audio、录音、播放、codec、aec |
| QSPI LCD | qspilcd、qspi lcd、四线 lcd |
| DVP | dvp、摄像头接口、camera 接口 |
| Camera | camera、摄像头、sensor、拍照 |
| RGB | rgb、led灯带、ws2812、sk6812 |
| Pinmux | pinmux、引脚复用、iomux、引脚功能 |

若用户描述涉及多个设备或设备不明确：列出候选设备，询问用户确认。

## 执行

确定 {driver_name} 后，按 `references/driver-template.md` 的流程执行：

1. 动态发现该驱动的所有资源（头文件、实现、文档、Kconfig、示例、pinmux）
2. 加载 experience 经验文件（如存在）
3. 分析资源，结合用户需求执行任务
4. 只对无法从资源推断的信息向用户确认

## 经验捕获

驱动开发任务完成后，评估是否有值得记录的经验（非显而易见的根因、用户纠正、隐含约束等）。如有，调用 `experience-capture` skill。
