#ifndef _CSK_GPIO_H_
#define _CSK_GPIO_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>


int32_t gpio_init(void);

void camera_qspi_in_pinmux(void);
void camera_qspi_in_power_down(void);
void camera_qspi_in_power_on(void);
void camera_qspi_in_gpio_pwm(void);
qspi_in_pin_t* camera_qspi_in_pin_attr(void);

void camera_dvp_pinmux(void);
void camera_dvp_reset(void);
void camera_dvp_power_down(void);
void camera_dvp_power_on(void);
void camera_dvp_gpio_pwm(void);
dvp_pin_t* camera_dvp_pin_attr(void);

void lcd_qspi_out_pinmux(void);
void lcd_qspi_out_reset(void);
void lcd_qspi_out_bl_enable(void);
void lcd_qspi_out_gpio_pwm(void);
qspi_out_pin_t* lcd_qspi_out_pin_attr(void);

void lcd_rgb_pinmux(void);
void lcd_rgb_reset(void);
void lcd_rgb_bl_enable(void);
void lcd_rgb_gpio_pwm(void);
rgb_pin_t* lcd_rgb_pin_attr(void);

void sw_qspi_out_gpio_init(void);
void sw_qspi_out_cs_gpio_set(void);
void sw_qspi_out_dc_gpio_set(void);
void sw_qspi_out_clk_gpio_set(void);
void sw_qspi_out_d0_gpio_set(void);
void sw_qspi_out_d1_gpio_set(void);
void sw_qspi_out_d2_gpio_set(void);
void sw_qspi_out_d3_gpio_set(void);
void sw_qspi_out_rst_gpio_set(void);
void sw_qspi_out_cs_gpio_clr(void);
void sw_qspi_out_dc_gpio_clr(void);
void sw_qspi_out_clk_gpio_clr(void);
void sw_qspi_out_d0_gpio_clr(void);
void sw_qspi_out_d1_gpio_clr(void);
void sw_qspi_out_d2_gpio_clr(void);
void sw_qspi_out_d3_gpio_clr(void);
void sw_qspi_out_rst_gpio_clr(void);

void sw_dvp_i2c_gpio_init(void);
void sw_dvp_i2c_scl_gpio_set(void);
void sw_dvp_i2c_scl_gpio_clr(void);
void sw_dvp_i2c_sda_gpio_set(void);
void sw_dvp_i2c_sda_gpio_clr(void);
uint8_t sw_dvp_i2c_sda_gpio_get(void);
void sw_dvp_i2c_sda_gpio_setdir_output(void);
void sw_dvp_i2c_sda_gpio_setdir_input(void);

void sw_qspi_in_i2c_gpio_init(void);
void sw_qspi_in_i2c_scl_gpio_set(void);
void sw_qspi_in_i2c_scl_gpio_clr(void);
void sw_qspi_in_i2c_sda_gpio_set(void);
void sw_qspi_in_i2c_sda_gpio_clr(void);
uint8_t sw_qspi_in_i2c_sda_gpio_get(void);
void sw_qspi_in_i2c_sda_gpio_setdir_output(void);
void sw_qspi_in_i2c_sda_gpio_setdir_input(void);


#endif
