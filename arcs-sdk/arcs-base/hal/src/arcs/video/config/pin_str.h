#ifndef _PIN_STR_H_
#define _PIN_STR_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define PIN_PAD_NULL    0xFF

typedef struct
{
    uint8_t pad;
    uint8_t index;
    uint8_t func;
    uint8_t gpio_func;
    void *dev;
}pin_t;

typedef struct
{
    pin_t mclk;
    pin_t pclk;
    pin_t d0;
    pin_t d1;
    pin_t d2;
    pin_t d3;
    pin_t pwdn;
    pin_t scl;
    pin_t sda;
}qspi_in_pin_t;

typedef struct
{
    pin_t cs;
    pin_t dc;
    pin_t clk;
    pin_t d0;
    pin_t d1;
    pin_t d2;
    pin_t d3;
    pin_t rst;
    pin_t bl;
}qspi_out_pin_t;

typedef struct
{
    pin_t mclk;
    pin_t pclk;
    pin_t vs;
    pin_t hs;
    pin_t d0;
    pin_t d1;
    pin_t d2;
    pin_t d3;
    pin_t d4;
    pin_t d5;
    pin_t d6;
    pin_t d7;
    pin_t rst;
    pin_t pwdn;
    pin_t scl;
    pin_t sda;
}dvp_pin_t;

typedef struct
{
    pin_t clk;
    pin_t vs;
    pin_t hs;
    pin_t de;
    pin_t r0;
    pin_t r1;
    pin_t r2;
    pin_t r3;
    pin_t r4;
    pin_t r5;
    pin_t r6;
    pin_t r7;
    pin_t g0;
    pin_t g1;
    pin_t g2;
    pin_t g3;
    pin_t g4;
    pin_t g5;
    pin_t g6;
    pin_t g7;
    pin_t b0;
    pin_t b1;
    pin_t b2;
    pin_t b3;
    pin_t b4;
    pin_t b5;
    pin_t b6;
    pin_t b7;
    pin_t rst;
    pin_t bl;
}rgb_pin_t;


#endif
