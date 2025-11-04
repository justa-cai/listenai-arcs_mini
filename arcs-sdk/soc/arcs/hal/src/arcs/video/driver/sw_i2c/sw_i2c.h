#ifndef __SW_I2C_H
#define __SW_I2C_H

#ifdef __cplusplus
 extern "C" {
#endif 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>


typedef struct {
    void        (*gpio_init)       (void);
    void        (*scl_set)         (void);
    void        (*scl_clr)         (void);
    void        (*sda_set)         (void);
    void        (*sda_clr)         (void);
    uint8_t     (*sda_get)         (void);
    void        (*sda_dirout)      (void);
    void        (*sda_dirin)       (void);
    void        (*delay_ms)        (uint32_t nms);
    void        (*delay_us)        (uint32_t nus);
    int         (*log)             (const char* format, ...);
} sw_i2c_port_callback_t;


int sw_i2c_init(uint8_t i2c_index, sw_i2c_port_callback_t *callback);
int sw_i2c_write_reg8(uint8_t i2c_index, uint16_t addr, uint8_t reg, uint8_t value);
uint8_t sw_i2c_read_reg8(uint8_t i2c_index, uint16_t addr, uint8_t reg);
int sw_i2c_write_reg16(uint8_t i2c_index, uint16_t addr, uint16_t reg, uint8_t value);
uint8_t sw_i2c_read_reg16(uint8_t i2c_index, uint16_t addr, uint16_t reg);
uint8_t sw_i2c_detect(uint8_t i2c_index, uint16_t addr);
int sw_i2c_detect_all(uint8_t i2c_index);

#ifdef __cplusplus
}
#endif

#endif /* __CAMERA_I2C_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
