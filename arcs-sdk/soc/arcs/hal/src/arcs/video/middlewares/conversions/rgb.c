#include "img_converters.h"


void swap_bit(uint8_t *pdata, uint32_t num)
{
    while(num--)
    {
        *pdata = ((*pdata & 0x01) << 7) | ((*pdata & 0x02) << 5) | ((*pdata & 0x04) << 3) | ((*pdata & 0x08) << 1) | \
                 ((*pdata & 0x10) >> 1) | ((*pdata & 0x20) >> 3) | ((*pdata & 0x40) >> 5) | ((*pdata & 0x80) >> 7);
        pdata++;
    }
}

void swap_uint16(uint16_t *pdata, uint32_t num)
{
    while(num--)
    {
        *pdata = ((*pdata & 0x00FF) << 8) | ((*pdata & 0xFF00) >> 8);
        pdata++;
    }
}

void swap_uint32(uint32_t *pdata, uint32_t num)
{
    while(num--)
    {
        *pdata = ((*pdata) >> 24) | ((*pdata & 0x00FF0000) >> 8) | ((*pdata & 0x0000FF00) << 8) | ((*pdata) << 24);
        pdata++;
    }
}


/*
 *   RGB565: [bit15--bit0] RRRRRGGG_GGGBBBBB
 *   BGR565: [bit15--bit0] BBBBBGGG_GGGRRRRR
 *   RGB888: [bit23--bit0] RRRRRRRR_GGGGGGGG_BBBBBBBB
 *   BGR888: [bit23--bit0] BBBBBBBB_GGGGGGGG_RRRRRRRR
 * ARGB8888: [bit31--bit0] AAAAAAAA_RRRRRRRR_GGGGGGGG_BBBBBBBB
 * ABGR8888: [bit31--bit0] AAAAAAAA_BBBBBBBB_GGGGGGGG_RRRRRRRR
 * RGBA8888: [bit31--bit0] RRRRRRRR_GGGGGGGG_BBBBBBBB_AAAAAAAA
 * BGRA8888: [bit31--bit0] BBBBBBBB_GGGGGGGG_RRRRRRRR_AAAAAAAA
 * little end, so [bit7--bit0] in addr+0, [bit15--bit8] in addr+1 ......
 */
void rgb565_to_bgr565(uint16_t *rgb565, uint32_t pixel)
{
    while(pixel--)
    {
        *rgb565 = ((*rgb565 & 0x001F) << 11) | ((*rgb565 & 0x07E0)) | ((*rgb565 & 0xF800) >> 11);
        rgb565++;
    }
}


void rgb888_to_bgr888(uint8_t *rgb888, uint32_t pixel)
{
    uint8_t tmp;

    while(pixel--)
    {
        tmp = *rgb888;
        *rgb888 = *(rgb888+2);
        *(rgb888+2) = tmp;
        rgb888+=3;
    }
}


void rgb565_to_rgb888(uint16_t *rgb565, uint8_t *rgb888, uint32_t pixel)
{
    while(pixel--)
    {
        *rgb888++ = ((*rgb565 & 0x001F) << 3) | ((*rgb565 & 0x001F) >> 2);  // B
        *rgb888++ = ((*rgb565 & 0x07E0) >> 3) | ((*rgb565 & 0x07E0) >> 9);  // G
        *rgb888++ = ((*rgb565 & 0xF800) >> 8) | ((*rgb565 & 0xF800) >> 13); // R
        rgb565++;
    }
}


void rgb888_to_rgb565(uint8_t *rgb888, uint16_t *rgb565, uint32_t pixel)
{
    while(pixel--)
    {
        *rgb565++ = ((*(rgb888+2) & 0xF8) << 8) | ((*(rgb888+1) & 0xFC) << 3) | ((*rgb888 & 0xF8) >> 3);
        rgb888+=3;
    }
}


void rgb565_to_argb8888(uint16_t *rgb565, uint8_t *argb8888, uint32_t pixel)
{
    while(pixel--)
    {
        *argb8888++ = ((*rgb565 & 0x001F) << 3) | ((*rgb565 & 0x001F) >> 2);  // B
        *argb8888++ = ((*rgb565 & 0x07E0) >> 3) | ((*rgb565 & 0x07E0) >> 9);  // G
        *argb8888++ = ((*rgb565 & 0xF800) >> 8) | ((*rgb565 & 0xF800) >> 13); // R
        *argb8888++ = 0xFF;
        rgb565++;
    }
}


void argb8888_to_rgb565(uint8_t *argb8888, uint16_t *rgb565, uint32_t pixel)
{
    while(pixel--)
    {
        *rgb565++ = ((*(argb8888+2) & 0xF8) << 8) | ((*(argb8888+1) & 0xFC) << 3) | ((*argb8888 & 0xF8) >> 3);
        rgb565++;
        argb8888+=4;
    }
}


void rgb565_crop(uint16_t *dest_rgb565, uint16_t *src_rgb565, uint16_t src_width, uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y)
{
    uint16_t x, y;

    for(y = start_y; y < end_y; y++)
    {
        for(x = start_x; x < end_x; x++)
        {
            *dest_rgb565++ = src_rgb565[y * src_width + x];
        }
    }
}



const uint16_t rgb565_color_tab[] = {
        RGB565_WHITE,
        RGB565_RED,
        RGB565_GREEN,
        RGB565_BLUE,
        RGB565_WHITE,
        RGB565_BLACK,
        RGB565_YELLOW,
        RGB565_BRED,
        RGB565_GBLUE,
};

const uint32_t rgb888_color_tab[] = {
        RGB888_WHITE,
        RGB888_RED,
        RGB888_GREEN,
        RGB888_BLUE,
        RGB888_WHITE,
        RGB888_BLACK,
        RGB888_YELLOW,
        RGB888_BRED,
        RGB888_GBLUE,
};

void rgb565_colorbar_create(uint16_t *rgb565, uint16_t img_width, uint16_t img_height, uint16_t bar_height)
{
    uint16_t x, y, color;
    uint16_t index;

    for(y = 0; y < img_height; y++)
    {
        index = ((y / bar_height)) % (sizeof(rgb565_color_tab) / sizeof(uint16_t));
        color = rgb565_color_tab[index];

        for(x = 0; x < img_width; x++)
        {
            if(index == 0) {
                color = rgb565_color_tab[((x / bar_height)) % (sizeof(rgb565_color_tab) / sizeof(uint16_t))];
            } else {
                if(rgb565_color_tab[index] == RGB565_WHITE) {
                    if(color >= 0x0841) {   // ((1<<11) | (1<<6) | 1)     2^5=32
                        color -= 0x0841;
                    } else {
                        color = RGB565_WHITE;
                    }
                } else if(rgb565_color_tab[index] == RGB565_BLACK) {
                    if(color <= (0xFFFF - 0x0841)) {
                        color += 0x0841;
                    } else {
                        color = RGB565_BLACK;
                    }
                } else {
                }
            }

            *rgb565++ = color;
        }
    }
}

void rgb565_color_fill(uint16_t *rgb565, uint16_t color, uint16_t img_width, uint16_t img_height)
{
    uint16_t x, y;

    for(y = 0; y < img_height; y++)
    {
        for(x = 0; x < img_width; x++)
        {
            *rgb565++ = color;
        }
    }
}

void rgb888_colorbar_create(uint8_t *rgb888, uint16_t img_width, uint16_t img_height, uint16_t bar_height)
{
    uint16_t x, y;
    uint16_t index;
    uint32_t color;

    for(y = 0; y < img_height; y++)
    {
        index = ((y / bar_height)) % (sizeof(rgb888_color_tab) / sizeof(uint32_t));
        color = rgb888_color_tab[index];

        for(x = 0; x < img_width; x++)
        {
            if(index == 0) {
                color = rgb888_color_tab[((x / bar_height)) % (sizeof(rgb888_color_tab) / sizeof(uint32_t))];
            } else {
                if(rgb888_color_tab[index] == RGB888_WHITE) {
                    if(color >= 0x010101) {   // ((1<<11) | (1<<6) | 1)     2^5=32
                        color -= 0x010101;
                    } else {
                        color = RGB888_WHITE;
                    }
                } else if(rgb888_color_tab[index] == RGB888_BLACK) {
                    if(color <= (0xFFFFFF - 0x010101)) {
                        color += 0x010101;
                    } else {
                        color = RGB888_BLACK;
                    }
                } else {
                }
            }

            *rgb888++ = color & 0xFF;
            *rgb888++ = (color >> 8) & 0xFF;
            *rgb888++ = (color >> 16) & 0xFF;
        }
    }
}

void argb8888_colorbar_create(uint8_t *argb8888, uint16_t img_width, uint16_t img_height, uint16_t bar_height)
{
    uint16_t x, y;
    uint16_t index;
    uint32_t color;

    for(y = 0; y < img_height; y++)
    {
        index = ((y / bar_height)) % (sizeof(rgb888_color_tab) / sizeof(uint32_t));
        color = rgb888_color_tab[index];

        for(x = 0; x < img_width; x++)
        {
            if(index == 0) {
                color = rgb888_color_tab[((x / bar_height)) % (sizeof(rgb888_color_tab) / sizeof(uint32_t))];
            } else {
                if(rgb888_color_tab[index] == RGB888_WHITE) {
                    if(color >= 0x010101) {   // ((1<<11) | (1<<6) | 1)     2^5=32
                        color -= 0x010101;
                    } else {
                        color = RGB888_WHITE;
                    }
                } else if(rgb888_color_tab[index] == RGB888_BLACK) {
                    if(color <= (0xFFFFFF - 0x010101)) {
                        color += 0x010101;
                    } else {
                        color = RGB888_BLACK;
                    }
                } else {
                }
            }

            *argb8888++ = color & 0xFF;
            *argb8888++ = (color >> 8) & 0xFF;
            *argb8888++ = (color >> 16) & 0xFF;
            *argb8888++ = 0xFF;
        }
    }
}

void rgb565_grid_create(uint16_t *rgb565, uint16_t img_width, uint16_t img_height, uint16_t grid_height)
{
    uint16_t x, y;

    for(y = 0; y < img_height; y++)
    {
        for(x = 0; x < img_width; x++)
        {
            if((y % grid_height) == 0) {
                rgb565[y * img_width + x] = RGB565_BLUE;
            } else if((y % grid_height) == (grid_height-1)) {
                rgb565[y * img_width + x] = RGB565_GREEN;
            } else if(y  == (img_height-1)) {
                rgb565[y * img_width + x] = RGB565_GREEN;
            } else if((x % grid_height) == 0) {
                rgb565[y * img_width + x] = RGB565_BLUE;
            } else if((x % grid_height) == (grid_height-1)) {
                rgb565[y * img_width + x] = RGB565_GREEN;
            } else if(x == (img_width-1)) {
                rgb565[y * img_width + x] = RGB565_GREEN;
            } else {
            }
        }
    }
}

void rgb888_grid_create(uint8_t *rgb888, uint16_t img_width, uint16_t img_height, uint16_t grid_height)
{
    uint16_t x, y;

    for(y = 0; y < img_height; y++)
    {
        for(x = 0; x < img_width; x++)
        {
            if((y % grid_height) == 0) {
                rgb888[(y * img_width + x) * 3 + 0] = RGB888_BLUE & 0xFF;
                rgb888[(y * img_width + x) * 3 + 1] = (RGB888_BLUE >> 8) & 0xFF;
                rgb888[(y * img_width + x) * 3 + 2] = (RGB888_BLUE >> 16) & 0xFF;
            } else if((y % grid_height) == (grid_height-1)) {
                rgb888[(y * img_width + x) * 3 + 0] = RGB888_GREEN & 0xFF;
                rgb888[(y * img_width + x) * 3 + 1] = (RGB888_GREEN >> 8) & 0xFF;
                rgb888[(y * img_width + x) * 3 + 2] = (RGB888_GREEN >> 16) & 0xFF;
            } else if(y == (img_height-1)) {
                rgb888[(y * img_width + x) * 3 + 0] = RGB888_GREEN & 0xFF;
                rgb888[(y * img_width + x) * 3 + 1] = (RGB888_GREEN >> 8) & 0xFF;
                rgb888[(y * img_width + x) * 3 + 2] = (RGB888_GREEN >> 16) & 0xFF;
            } else if((x % grid_height) == 0) {
                rgb888[(y * img_width + x) * 3 + 0] = RGB888_BLUE & 0xFF;
                rgb888[(y * img_width + x) * 3 + 1] = (RGB888_BLUE >> 8) & 0xFF;
                rgb888[(y * img_width + x) * 3 + 2] = (RGB888_BLUE >> 16) & 0xFF;
            } else if((x % grid_height) == (grid_height-1)) {
                rgb888[(y * img_width + x) * 3 + 0] = RGB888_GREEN & 0xFF;
                rgb888[(y * img_width + x) * 3 + 1] = (RGB888_GREEN >> 8) & 0xFF;
                rgb888[(y * img_width + x) * 3 + 2] = (RGB888_GREEN >> 16) & 0xFF;
            } else if(x == (img_width-1)) {
                rgb888[(y * img_width + x) * 3 + 0] = RGB888_GREEN & 0xFF;
                rgb888[(y * img_width + x) * 3 + 1] = (RGB888_GREEN >> 8) & 0xFF;
                rgb888[(y * img_width + x) * 3 + 2] = (RGB888_GREEN >> 16) & 0xFF;
            } else {
            }
        }
    }
}

void argb8888_grid_create(uint8_t *argb8888, uint16_t img_width, uint16_t img_height, uint16_t grid_height)
{
    uint16_t x, y;

    for(y = 0; y < img_height; y++)
    {
        for(x = 0; x < img_width; x++)
        {
            if((y % grid_height) == 0) {
                argb8888[(y * img_width + x) * 4 + 0] = RGB888_BLUE & 0xFF;
                argb8888[(y * img_width + x) * 4 + 1] = (RGB888_BLUE >> 8) & 0xFF;
                argb8888[(y * img_width + x) * 4 + 2] = (RGB888_BLUE >> 16) & 0xFF;
                argb8888[(y * img_width + x) * 4 + 3] = 0xFF;
            } else if((y % grid_height) == (grid_height-1)) {
                argb8888[(y * img_width + x) * 4 + 0] = RGB888_GREEN & 0xFF;
                argb8888[(y * img_width + x) * 4 + 1] = (RGB888_GREEN >> 8) & 0xFF;
                argb8888[(y * img_width + x) * 4 + 2] = (RGB888_GREEN >> 16) & 0xFF;
                argb8888[(y * img_width + x) * 4 + 3] = 0xFF;
            } else if(y == (img_height-1)) {
                argb8888[(y * img_width + x) * 4 + 0] = RGB888_GREEN & 0xFF;
                argb8888[(y * img_width + x) * 4 + 1] = (RGB888_GREEN >> 8) & 0xFF;
                argb8888[(y * img_width + x) * 4 + 2] = (RGB888_GREEN >> 16) & 0xFF;
                argb8888[(y * img_width + x) * 4 + 3] = 0xFF;
            } else if((x % grid_height) == 0) {
                argb8888[(y * img_width + x) * 4 + 0] = RGB888_BLUE & 0xFF;
                argb8888[(y * img_width + x) * 4 + 1] = (RGB888_BLUE >> 8) & 0xFF;
                argb8888[(y * img_width + x) * 4 + 2] = (RGB888_BLUE >> 16) & 0xFF;
                argb8888[(y * img_width + x) * 4 + 3] = 0xFF;
            } else if((x % grid_height) == (grid_height-1)) {
                argb8888[(y * img_width + x) * 4 + 0] = RGB888_GREEN & 0xFF;
                argb8888[(y * img_width + x) * 4 + 1] = (RGB888_GREEN >> 8) & 0xFF;
                argb8888[(y * img_width + x) * 4 + 2] = (RGB888_GREEN >> 16) & 0xFF;
                argb8888[(y * img_width + x) * 4 + 3] = 0xFF;
            } else if(x == (img_width-1)) {
                argb8888[(y * img_width + x) * 4 + 0] = RGB888_GREEN & 0xFF;
                argb8888[(y * img_width + x) * 4 + 1] = (RGB888_GREEN >> 8) & 0xFF;
                argb8888[(y * img_width + x) * 4 + 2] = (RGB888_GREEN >> 16) & 0xFF;
                argb8888[(y * img_width + x) * 4 + 3] = 0xFF;
            } else {
            }
        }
    }
}

void rgb565_square_create(uint16_t *rgb565, uint16_t img_width, uint16_t xs, uint16_t ys, uint16_t length)
{
    uint16_t i;

    i = length;
    while(i--)
    {
        rgb565[img_width * ys + xs + i] = RGB565_RED;
    }

    i = length;
    while(i--)
    {
        rgb565[img_width * (ys + length) + xs + i] = RGB565_RED;
    }

    i = length;
    while(i--)
    {
        rgb565[img_width * (ys + i) + xs] = RGB565_RED;
    }

    i = length;
    while(i--)
    {
        rgb565[img_width * (ys + i) + xs + length] = RGB565_RED;
    }
}

void raw8_line_color(uint8_t *raw8, uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye, uint16_t img_width, uint32_t color)
{
    uint16_t x = 0;
    uint16_t y = 0;

    for(y = ys; y <= ye; y++)
    {
        for(x = xs; x <= xe; x++)
        {
            raw8[(y * img_width + x)] = color;
        }
    }
}

void rgb565_line_color(uint16_t *rgb565, uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye, uint16_t img_width, uint32_t color)
{
    uint16_t x = 0;
    uint16_t y = 0;

    for(y = ys; y <= ye; y++)
    {
        for(x = xs; x <= xe; x++)
        {
            rgb565[y * img_width + x] = color;
        }
    }
}

void rgb888_line_color(uint8_t *rgb888, uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye, uint16_t img_width, uint32_t color)
{
    uint16_t x = 0;
    uint16_t y = 0;

    for(y = ys; y <= ye; y++)
    {
        for(x = xs; x <= xe; x++)
        {
            rgb888[(y * img_width + x) * 3 + 0] = color & 0xFF;
            rgb888[(y * img_width + x) * 3 + 1] = (color >> 8) & 0xFF;
            rgb888[(y * img_width + x) * 3 + 2] = (color >> 16) & 0xFF;
        }
    }
}




