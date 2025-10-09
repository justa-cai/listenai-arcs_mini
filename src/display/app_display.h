#ifndef __APP_DISPLAY_H__
#define __APP_DISPLAY_H__

#include "stdint.h"

int app_display_brightness_init(void);
uint8_t app_display_get_brightness(void);
int app_display_set_brightness(uint8_t val);

#endif
