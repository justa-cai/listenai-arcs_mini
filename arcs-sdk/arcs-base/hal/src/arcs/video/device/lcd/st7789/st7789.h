#ifndef __ST7789_H_
#define __ST7789_H_

#include <string.h>
#include "lcd.h"


void st7789_init(lcd_format_e format);
void st7789_datalane_set(uint8_t lane_num);
void st7789_window_set(uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h);


#endif
