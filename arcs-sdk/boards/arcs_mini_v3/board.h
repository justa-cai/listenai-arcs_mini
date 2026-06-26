/**
 * @file board.h
 * @brief arcs_mini_v3 board interface
 */

#pragma once

#include "pinmux.h"

/*
 * Compatibility aliases for existing arcs-mini application code.
 * Keep generated pinmux.h untouched so it can be replaced by the pin tool.
 */
#ifndef LCD_CS_PIN
#define LCD_CS_PIN 22
#endif
#ifndef LCD_SPI_DATA_PIN
#define LCD_SPI_DATA_PIN 24
#endif
#ifndef LCD_SPI_CLK_PIN
#define LCD_SPI_CLK_PIN 25
#endif
#ifndef CAM_HSYNC_PIN
#define CAM_HSYNC_PIN 10
#endif
#ifndef CAM_VSYNC_PIN
#define CAM_VSYNC_PIN 11
#endif
#ifndef CAM_PCLK_PIN
#define CAM_PCLK_PIN 12
#endif
#ifndef CAM_D0_PIN
#define CAM_D0_PIN 13
#endif
#ifndef CAM_D1_PIN
#define CAM_D1_PIN 14
#endif
#ifndef CAM_D2_PIN
#define CAM_D2_PIN 15
#endif
#ifndef CAM_D3_PIN
#define CAM_D3_PIN 16
#endif
#ifndef CAM_D4_PIN
#define CAM_D4_PIN 17
#endif
#ifndef CAM_D5_PIN
#define CAM_D5_PIN 18
#endif
#ifndef CAM_D6_PIN
#define CAM_D6_PIN 19
#endif
#ifndef CAM_D7_PIN
#define CAM_D7_PIN 20
#endif
#ifndef CAM_MCLK_PIN
#define CAM_MCLK_PIN 26
#endif
#ifndef AP_LOG_RX_PIN
#define AP_LOG_RX_PIN CP_LOG_RX0_PIN
#endif
#ifndef AP_LOG_TX_PIN
#define AP_LOG_TX_PIN CP_LOG_TX0_PIN
#endif
#ifndef CP_LOG_TX_PIN
#define CP_LOG_TX_PIN B2_APLOG_TX1_PIN
#endif
#define BOARD_ARCS_MINI_V3_HAS_LED 1
#define BOARD_ARCS_MINI_V3_HAS_LOCAL_PA_MUTE 0
#define BOARD_ARCS_MINI_V3_HAS_EXMCU_PA_MUTE 1
#define BOARD_ARCS_MINI_V3_HAS_PA BOARD_ARCS_MINI_V3_HAS_LOCAL_PA_MUTE
#define BOARD_ARCS_MINI_V3_HAS_LCD_RST 1

/* CH32V003 external MCU on SoC I2C1 (PB0/PB1), I2C address 0x6c. */
#define CH32V003_DEVICE_NAME      "ch32v003"
#define CH32V003_I2C_DEVICE_NAME  "i2c1"
#define CH32V003_I2C_ADDR         0x6c
#define CH32V003_SDA1_PIN         5  /* CH32V003 PC5, per arcs_mini_v3 schematic review */
#define CH32V003_SCL1_PIN         6  /* CH32V003 PC6, per arcs_mini_v3 schematic review */

#define EXGPIOA_DEVICE_NAME       "exgpioa"
#define EXGPIOB_DEVICE_NAME       "exgpiob"
#define EXGPIOC_DEVICE_NAME       "exgpioc"
#define EXGPIOD_DEVICE_NAME       "exgpiod"
#define EXADC_DEVICE_NAME         "exadc"
#define EXPWM_DEVICE_NAME         "expwm"

/* CH32V003 GPIO aliases. Polarity defaults below are board-review defaults. */
#define PA_MUTE_DEVICE_NAME       EXGPIOC_DEVICE_NAME
#define PA_MUTE_PIN               2
#define PA_MUTE_ACTIVE_LEVEL      0

#define CHARGE_DET_DEVICE_NAME    EXGPIOD_DEVICE_NAME
#define CHARGE_DET_PIN            0
#define CHARGE_DET_ACTIVE_LEVEL   1

#define CH_4G_DET_DEVICE_NAME     EXGPIOD_DEVICE_NAME
#define CH_4G_DET_PIN             4
#define CH_4G_DET_ACTIVE_LEVEL    1

/* PD5 is reserved as the CH32V003 log UART output signal. */
#define EXIO_LOG_DEVICE_NAME      EXGPIOD_DEVICE_NAME
#define EXIO_LOG_PIN              5

#define CAM_PWND_DEVICE_NAME      EXGPIOD_DEVICE_NAME
#define CAM_PWND_PIN              6
#define CAM_PWND_ACTIVE_LEVEL     1
#define CAM_PWDN_DEVICE_NAME      CAM_PWND_DEVICE_NAME
#define CAM_PWDN_PIN              CAM_PWND_PIN
#define CAM_PWDN_ACTIVE_LEVEL     CAM_PWND_ACTIVE_LEVEL

#define USER_LED0_DEVICE_NAME     EXGPIOA_DEVICE_NAME
#define USER_LED0_PIN             2
#define USER_LED1_DEVICE_NAME     EXGPIOC_DEVICE_NAME
#define USER_LED1_PIN             0
#define USER_LED2_DEVICE_NAME     EXGPIOC_DEVICE_NAME
#define USER_LED2_PIN             3
#define USER_LED_ACTIVE_LEVEL     0

#define TF_DET_DEVICE_NAME        EXGPIOD_DEVICE_NAME
#define TF_DET_PIN                3
#define TF_DET_ACTIVE_LEVEL       0

#define CHARGE_EN_DEVICE_NAME     EXGPIOD_DEVICE_NAME
#define CHARGE_EN_PIN             2
#define CHARGE_EN_ACTIVE_LEVEL    1

/* CH32V003 ADC channel map: CH2 -> PC4, used for battery temperature. */
#define BAT_TEMP_ADC_DEVICE_NAME  EXADC_DEVICE_NAME
#define BAT_TEMP_ADC_CHANNEL      2
#define BAT_TEMP_ADC_RESOLUTION   12

#ifdef __cplusplus
extern "C" {
#endif

const char *board_get_name(void);

#ifdef __cplusplus
}
#endif
