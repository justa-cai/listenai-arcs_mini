#ifndef __CSK_I2C_H
#define __CSK_I2C_H

#ifdef __cplusplus
 extern "C" {
#endif 

#include <stdint.h>
#include "Driver_I2C.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "systick.h"
#include "log_print.h"
#include "csk_timer.h"
#include "csk_driver.h"


 /* addr: 7bit or 10bit */
#define CSK_I2C_WRITE_REG8(index, addr, reg, value)   csk_i2c_write_reg8(index, addr, reg, value)
#define CSK_I2C_READ_REG8(index, addr, reg)           csk_i2c_read_reg8(index, addr, reg)
#define CSK_I2C_WRITE_REG16(index, addr, reg, value)  csk_i2c_write_reg16(index, addr, reg, value)
#define CSK_I2C_READ_REG16(index, addr, reg)          csk_i2c_read_reg16(index, addr, reg)
#define CSK_I2C_READ(index, addr, reg, pData, num)    csk_i2c_read(index, addr, reg, pData, num)
#define CSK_I2C_INIT(index)                           csk_i2c_init(index)

int csk_i2c_init(uint8_t i2c_index);
int csk_i2c_write_reg8(uint8_t i2c_index, uint16_t addr, uint8_t reg, uint8_t value);
uint8_t csk_i2c_read_reg8(uint8_t i2c_index, uint16_t addr, uint8_t reg);
int csk_i2c_write_reg16(uint8_t i2c_index, uint16_t addr, uint16_t reg, uint8_t value);
uint8_t csk_i2c_read_reg16(uint8_t i2c_index, uint16_t addr, uint16_t reg);
int csk_i2c_read(uint8_t i2c_index, uint16_t addr, uint8_t reg, uint8_t *data, uint16_t num);
int csk_i2c_detect(uint8_t i2c_index);

#ifdef __cplusplus
}
#endif

#endif /* __CAMERA_I2C_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
