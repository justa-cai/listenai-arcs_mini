#ifndef __SPD2010_H_
#define __SPD2010_H_

#include <string.h>
#include "lcd.h"

void spd2010_init(lcd_format_e format);
void spd2010_datalane_set(uint8_t lane_num);
void spd2010_window_set(uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h);

#endif
