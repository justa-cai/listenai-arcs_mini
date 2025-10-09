/*
 * display_uc8253c.h
 */
#pragma once
#ifndef __DISPLAY_UC8253C_H__
#define __DISPLAY_UC8253C_H__

#include <stdint.h>
#include "lisa_display.h"

extern const struct display_device display_uc8253c;

typedef enum {
    EPD_REFRESH_MODE_GC = 0, // Gobal Clear(Global Update)
    EPD_REFRESH_MODE_DU = 1, // Direct Update
} epd_refresh_mode_e;

#endif // __DISPLAY_UC8253C_H__
