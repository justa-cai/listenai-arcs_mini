/**
 * @file board.h
 * @brief arcs_mini3 board interface
 */

#pragma once

#include "pinmux.h"

/* CH32V003 external MCU on SoC I2C1 (PB0/PB1), I2C address 0x6c. */
#define CH32V003_DEVICE_NAME      "ch32v003"
#define CH32V003_I2C_DEVICE_NAME  "i2c1"
#define CH32V003_I2C_ADDR         0x6c
#define CH32V003_SDA1_PIN         5  /* CH32V003 PC5, per arcs_mini3 schematic review */
#define CH32V003_SCL1_PIN         6  /* CH32V003 PC6, per arcs_mini3 schematic review */

#define EXGPIOA_DEVICE_NAME       "exgpioa"
#define EXGPIOB_DEVICE_NAME       "exgpiob"
#define EXGPIOC_DEVICE_NAME       "exgpioc"
#define EXGPIOD_DEVICE_NAME       "exgpiod"
#define EXADC_DEVICE_NAME         "exadc"
#define EXPWM_DEVICE_NAME         "expwm"

/* USB-C role controller on the shared SoC I2C1 bus. */
#define USB_ROLE_I2C_DEVICE_NAME  "i2c1"
#define USB_ROLE_I2C_ADDRESS      0x31
#define USB_ROLE_DEVICE_ID_REGISTER 0x01
#define USB_ROLE_DEVICE_TYPE_REGISTER 0x02
#define USB_ROLE_PORTROLE_REGISTER 0x03
#define USB_ROLE_CONTROL_REGISTER  0x04
#define USB_ROLE_CONTROL1_REGISTER 0x05
#define USB_ROLE_STATUS_REGISTER  0x11
#define USB_ROLE_TYPE_REGISTER    0x13
#define USB_ROLE_DEVICE_ID_EXPECTED 0x10
#define USB_ROLE_DEVICE_TYPE_EXPECTED 0x03
#define USB_ROLE_CONTROL_RP_MASK    0x06
#define USB_ROLE_CONTROL_RP_DEFAULT 0x02
#define USB_ROLE_CONTROL1_REQUIRED_MASK 0x08
#define USB_ROLE_PORTROLE_MASK    0x07
#define USB_ROLE_PORTROLE_SOURCE_ONLY 0x01
#define USB_ROLE_PORTROLE_SINK_ONLY   0x02
#define USB_ROLE_PORTROLE_DRP         0x04
#define USB_ROLE_PORTROLE_AUDIOACC    0x08
#define USB_ROLE_PORTROLE_TRY_SINK    0x10
#define USB_ROLE_PORTROLE_TRY_SOURCE  0x20
#define USB_ROLE_PORTROLE_TRY_MASK    (USB_ROLE_PORTROLE_TRY_SINK | USB_ROLE_PORTROLE_TRY_SOURCE)
#define USB_ROLE_PORTROLE_ORIENTDEB   0x40
#define USB_ROLE_STATUS_CONNECTED_MASK 0x01
#define USB_ROLE_STATUS_VBUS_OK_MASK   0x08
#define USB_ROLE_TYPE_SOURCE_MASK     0x08
#define USB_ROLE_TYPE_SINK_MASK       0x10

/* CH32V003 GPIO aliases. Polarity defaults below are board-review defaults. */
#define PA_MUTE_DEVICE_NAME       EXGPIOC_DEVICE_NAME
#define PA_MUTE_PIN               2
#define PA_MUTE_ACTIVE_LEVEL      0

#define CHARGE_DET_DEVICE_NAME    EXGPIOD_DEVICE_NAME
#define CHARGE_DET_PIN            0
#define CHARGE_DET_ACTIVE_LEVEL   0

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
