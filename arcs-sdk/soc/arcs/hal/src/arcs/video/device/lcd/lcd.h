#ifndef _LCD_H_
#define _LCD_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "log_print.h"
#include "csk_timer.h"
#include "csk_gpio.h"
#include "csk_spi.h"
#include "sw_qspi.h"
#include "csk_qspi_lcd.h"


#define LCD_DELAY_MS(_nms)              DELAY_MS(_nms)
#define LCD_DELAY_US(_nus)              DELAY_US(_nus)
#define LCD_LOG(fmt, ...)               VIDEO_LOG(fmt, ##__VA_ARGS__)

#define LCD_CS_SET()                    sw_qspi_out_cs_gpio_set()
#define LCD_CS_CLR()                    sw_qspi_out_cs_gpio_clr()

#define LCD_DC_SET()                    sw_qspi_out_dc_gpio_set()
#define LCD_DC_CLR()                    sw_qspi_out_dc_gpio_clr()

#define LCD_SPI_WRITE(_pdata, _num)     qspi_lcd_write(_pdata, _num)
//#define LCD_SPI_WRITE(_pdata, _num)     csk_spi_write_hold(_pdata, _num)
//#define LCD_SPI_WRITE(_pdata, _num)     sw_qspi_write_buf_1lane(_pdata, _num)


typedef enum {
    LCD_FORMAT_RGB565 = 0x0,
    LCD_FORMAT_RGB666 = 0x1,
    LCD_FORMAT_RGB888 = 0x2,
    LCD_FORMAT_BGR565 = 0x3,
    LCD_FORMAT_BGR666 = 0x4,
    LCD_FORMAT_BGR888 = 0x5,
    LCD_FORMAT_BUTT,
} lcd_format_e;


#if CONFIG_ST7789_SUPPORT
#include "st7789.h"
#define lcd_init                st7789_init
#define lcd_datalane_set        st7789_datalane_set
#define lcd_window_set          st7789_window_set
#endif
#if CONFIG_SPD2010_SUPPORT
#include "spd2010.h"
#define lcd_init                spd2010_init
#define lcd_datalane_set        spd2010_datalane_set
#define lcd_window_set          spd2010_window_set
#endif
#if CONFIG_AXS15231B_SUPPORT
#include "axs15231b.h"
#define lcd_init                axs15231b_init
#define lcd_datalane_set        axs15231b_datalane_set
#define lcd_window_set          axs15231b_window_set
#endif


#endif
