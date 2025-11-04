#ifndef _IMG_CONVERTERS_H_
#define _IMG_CONVERTERS_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "log_print.h"

#define RGB565_RED         0xF800
#define RGB565_GREEN       0x07E0
#define RGB565_BLUE        0x001F
#define RGB565_YELLOW      0xFFE0
#define RGB565_BRED        0XF81F
#define RGB565_GBLUE       0X07FF
#define RGB565_WHITE       0xFFFF
#define RGB565_BLACK       0x0000

#define RGB888_RED         0xFF0000
#define RGB888_GREEN       0x00FF00
#define RGB888_BLUE        0x0000FF
#define RGB888_YELLOW      0xFFFF00
#define RGB888_BRED        0XFF00FF
#define RGB888_GBLUE       0X00FFFF
#define RGB888_WHITE       0xFFFFFF
#define RGB888_BLACK       0x000000

#define CONSTRAIN_UINT8(ch)  ((ch) < 0) ? 0 : (((ch) > 255) ? 255 : (ch))
#define LIMIT_VAL(a,min,max) ((a) < (min) ? (min) : ((a) > (max) ? (max) : (a)))

#define PIXEL_RGB565_R5(rgb565)  ((rgb565 & 0xF800) >> 11)
#define PIXEL_RGB565_G6(rgb565)  ((rgb565 & 0x07E0) >> 5)
#define PIXEL_RGB565_B5(rgb565)  (rgb565 & 0x001F)
#define PIXEL_RGB565_TO_RGB888(rgb565)  (((rgb565 & 0xF800) << 8) | ((rgb565 & 0x07E0) << 5) | ((rgb565 & 0x001F) << 3))
#define PIXEL_RGB888_TO_RGB565(rgb888)  (((rgb888 & 0xF80000) >> 8) | ((rgb888 & 0xFC00) >> 5) | ((rgb888 & 0x00F8) >> 3))


void swap_bit(uint8_t *pdata, uint32_t num);
void swap_uint16(uint16_t *pdata, uint32_t num);
void swap_uint32(uint32_t *pdata, uint32_t num);

void rgb565_to_bgr565(uint16_t *rgb565, uint32_t pixel);
void rgb888_to_bgr888(uint8_t *rgb888, uint32_t pixel);
void rgb565_to_rgb888(uint16_t *rgb565, uint8_t *rgb888, uint32_t pixel);
void rgb888_to_rgb565(uint8_t *rgb888, uint16_t *rgb565, uint32_t pixel);
void rgb565_to_argb8888(uint16_t *rgb565, uint8_t *argb8888, uint32_t pixel);
void argb8888_to_rgb565(uint8_t *argb8888, uint16_t *rgb565, uint32_t pixel);

void rgb565_color_fill(uint16_t *rgb565, uint16_t color, uint16_t img_width, uint16_t img_height);
void rgb565_colorbar_create(uint16_t *rgb565, uint16_t img_width, uint16_t img_height, uint16_t bar_height);
void rgb565_grid_create(uint16_t *rgb565, uint16_t img_width, uint16_t img_height, uint16_t grid_height);
void rgb565_crop(uint16_t *dest_rgb565, uint16_t *src_rgb565, uint16_t src_width, uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y);
void rgb888_colorbar_create(uint8_t *rgb888, uint16_t img_width, uint16_t img_height, uint16_t bar_height);
void argb8888_colorbar_create(uint8_t *argb8888, uint16_t img_width, uint16_t img_height, uint16_t bar_height);
void rgb888_grid_create(uint8_t *rgb888, uint16_t img_width, uint16_t img_height, uint16_t grid_height);
void argb8888_grid_create(uint8_t *argb8888, uint16_t img_width, uint16_t img_height, uint16_t grid_height);
void rgb565_square_create(uint16_t *rgb565, uint16_t img_width, uint16_t xs, uint16_t ys, uint16_t length);

void rgb888_to_yuv422uyvy(uint8_t *rgb888, uint8_t *yuv422, uint32_t pixel);
void rgb565_to_yuv422yuyv(uint16_t *rgb565, uint8_t *yuv422, uint32_t pixel);
void rgb888_to_yuv444yuv(uint8_t *rgb888, uint8_t *yuv444, uint32_t pixel);
void yuv444yuv_to_yuv422yuyv(uint8_t *yuv444, uint8_t *yuv422, uint32_t pixel);
void yuv444yuv_create(uint8_t *yuv444, uint32_t pixel, uint32_t rgb888_color);

void yuv444_to_rgb888(uint8_t *Ydata, uint8_t *Udata, uint8_t *Vdata, uint8_t *RGBdata, uint32_t pixel);
void yuv422_to_rgb888(uint8_t *Ydata, uint8_t *Udata, uint8_t *Vdata, uint8_t *RGBdata, uint32_t pixel);
void yuv422_to_rgb565(uint8_t *Ydata, uint8_t *Udata, uint8_t *Vdata, uint8_t *RGBdata, uint32_t pixel);

void yuv422_uyvy_to_planar(uint8_t *YUVdata, uint8_t *Ydata, uint8_t *Udata, uint8_t *Vdata, uint32_t pixel);
void yyuv_to_yvyu(uint32_t *image_buf, uint32_t size_word);

void raw8_line_color(uint8_t *raw8, uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye, uint16_t img_width, uint32_t color);
void rgb565_line_color(uint16_t *rgb565, uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye, uint16_t img_width, uint32_t color);
void rgb888_line_color(uint8_t *rgb888, uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye, uint16_t img_width, uint32_t color);


#endif
