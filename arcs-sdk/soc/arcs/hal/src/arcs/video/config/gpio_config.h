#ifndef _GPIO_CONFIG_H_
#define _GPIO_CONFIG_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "IOMuxManager.h"
#include "pin_str.h"


#if 0
/*********************** PIN ***********************************/
static qspi_in_pin_t qspi_in_pin = {
    .mclk = {CSK_IOMUX_PAD_B, 5, CSK_IOMUX_FUNC_ALTER30, CSK_IOMUX_FUNC_DEFAULT},
    .pclk = {CSK_IOMUX_PAD_B, 3, CSK_IOMUX_FUNC_ALTER30, CSK_IOMUX_FUNC_DEFAULT},
    .d0   = {CSK_IOMUX_PAD_B, 7, CSK_IOMUX_FUNC_ALTER30, CSK_IOMUX_FUNC_DEFAULT},
    .d1   = {CSK_IOMUX_PAD_B, 6, 30, 0},
    .d2   = {CSK_IOMUX_PAD_B, 4, 30, 0},
    .d3   = {CSK_IOMUX_PAD_B, 2, 30, 0},
    .pwdn = {CSK_IOMUX_PAD_B, 5, 0, 0},
    .scl  = {CSK_IOMUX_PAD_B, 3, 0, 0},
    .sda  = {CSK_IOMUX_PAD_B, 3, 0, 0},
};

static qspi_out_pin_t qspi_out_pin = {
    .cs  = {CSK_IOMUX_PAD_B, 5, 30, 0},
    .clk = {CSK_IOMUX_PAD_B, 3, 30, 0},
    .d0  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .d1  = {CSK_IOMUX_PAD_B, 6, 30, 0},
    .d2  = {CSK_IOMUX_PAD_B, 4, 30, 0},
    .d3  = {CSK_IOMUX_PAD_B, 2, 30, 0},
    .rst = {CSK_IOMUX_PAD_B, 5, 0, 0},
    .bl  = {CSK_IOMUX_PAD_B, 3, 0, 0},
};

static dvp_pin_t dvp_pin = {
    .mclk = {CSK_IOMUX_PAD_B, 5, 30, 0},
    .pclk = {CSK_IOMUX_PAD_B, 3, 30, 0},
    .vs   = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .hs   = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .d0   = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .d1   = {CSK_IOMUX_PAD_B, 6, 30, 0},
    .d2   = {CSK_IOMUX_PAD_B, 4, 30, 0},
    .d3   = {CSK_IOMUX_PAD_B, 2, 30, 0},
    .d4   = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .d5   = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .d6   = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .d7   = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .d8   = {PIN_PAD_NULL, PIN_PAD_NULL, PIN_PAD_NULL, PIN_PAD_NULL},
    .d9   = {PIN_PAD_NULL, PIN_PAD_NULL, PIN_PAD_NULL, PIN_PAD_NULL},
    .d10  = {PIN_PAD_NULL, PIN_PAD_NULL, PIN_PAD_NULL, PIN_PAD_NULL},
    .d11  = {PIN_PAD_NULL, PIN_PAD_NULL, PIN_PAD_NULL, PIN_PAD_NULL},
    .rst  = {CSK_IOMUX_PAD_B, 5, 0, 0},
    .pwdn = {CSK_IOMUX_PAD_B, 5, 0, 0},
    .scl  = {CSK_IOMUX_PAD_B, 3, 0, 0},
    .sda  = {CSK_IOMUX_PAD_B, 3, 0, 0},
};

static rgb_pin_t rgb_pin = {
    .clk = {CSK_IOMUX_PAD_B, 5, 30, 0},
    .vs  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .hs  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .de  = {CSK_IOMUX_PAD_B, 3, 30, 0},
    .r0  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .r1  = {CSK_IOMUX_PAD_B, 6, 30, 0},
    .r2  = {CSK_IOMUX_PAD_B, 4, 30, 0},
    .r3  = {CSK_IOMUX_PAD_B, 2, 30, 0},
    .r4  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .r5  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .r6  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .r7  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .g0  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .g1  = {CSK_IOMUX_PAD_B, 6, 30, 0},
    .g2  = {CSK_IOMUX_PAD_B, 4, 30, 0},
    .g3  = {CSK_IOMUX_PAD_B, 2, 30, 0},
    .g4  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .g5  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .g6  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .g7  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .b0  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .b1  = {CSK_IOMUX_PAD_B, 6, 30, 0},
    .b2  = {CSK_IOMUX_PAD_B, 4, 30, 0},
    .b3  = {CSK_IOMUX_PAD_B, 2, 30, 0},
    .b4  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .b5  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .b6  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .b7  = {CSK_IOMUX_PAD_B, 7, 30, 0},
    .rst = {CSK_IOMUX_PAD_B, 5, 0, 0},
    .bl  = {CSK_IOMUX_PAD_B, 5, 0, 0},
};

#endif

#endif
