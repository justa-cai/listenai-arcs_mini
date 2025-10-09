#ifndef _TEST_CASE_H_
#define _TEST_CASE_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "log_print.h"
#include "csk_timer.h"

#include "img_converters.h"
#include "tiny_malloc.h"
#include "check.h"
#include "config.h"


void test_uart(void);
void test_sw_uart(void);
void test_big_little_endian(void);
void test_jlink(void);
void test_timer(void);
void test_i2c_detect(void);
void test_uart_tx(void);
void test_gpio_irq(void);
void test_img_converter(void);
void test_gpdma_regctrl(void);
void test_psram(void);
void test_segger_rtt(void);
void test_cp_run_in_flash(void);
void test_memcpy(void);
void test_usb_lsbist(void);
void test_usb_hsbist(void);
void test_usb_fsbist(void);
void test_usb_hshostdisc(void);
void test_gpdma_cpdma(void);

void test_dvp_xclk(void);
void test_dvp_api(void);
void test_camera_i2c(void);
void test_camera_dvp(void);
void test_camera_dvp_pin(void);
void test_camera_dvp_vs_hs(void);
void test_ov5640_jpeg(void);

void test_camera_spi_pin(void);
void test_camera_qspi_waveform(void);
void test_camera_spi0_rx(void);
void test_camera_qspi_in(void);

void test_lcd_color(void);
void test_lcd_display(void);
void test_lvgl_text(void);
void test_lvgl_demos(void);
void test_touch_i2c(void);
void test_touch(void);
void test_lcd_touch(void);
void test_lcd_vs_hs(void);

void test_lcd_sw_qspi(void);
void gpdma_reg_dump(uint8_t dma_ch);
void camera_reset(void);
void camera_pwd(void);
void camera_pwdn(void);
void blender_reg_dump(void);

void test_dvp(void);
void test_rgb(void);
void test_qspi_in(void);
void test_qspi_out(void);
void test_gpdma2d(void);
void test_blender(void);
void test_jpeg(void);
void test_pipe(void);
void test_2d(void);
void test_lvgl_dma2d_port(void);
void test_lvgl_dma2d_port2(void);
void test_lvgl_dma2d_port3(void);
void test_rgb888_rgb565(void);


#endif
