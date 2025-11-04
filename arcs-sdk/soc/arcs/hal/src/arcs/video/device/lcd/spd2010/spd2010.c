#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "spd2010.h"


#define SPD2010_CS_SET                      LCD_CS_SET
#define SPD2010_CS_CLR                      LCD_CS_CLR
#define SPD2010_DELAY_MS                    LCD_DELAY_MS
#define SPD2010_WRITE_BUF(_pdata, _num)     LCD_SPI_WRITE(_pdata, _num)


/********************************************************************************/
static void inline spd2010_write_byte(uint8_t data)
{
    SPD2010_WRITE_BUF(&data, 1);
}

static void spd2010_write_reg(uint8_t reg)
{
    spd2010_write_byte(0x02);
    spd2010_write_byte(0x00);
    spd2010_write_byte(reg);
    spd2010_write_byte(0x00);
}

static void inline spd2010_write_data(uint8_t dat)
{
    spd2010_write_byte(dat);
}


/********************************************************************************/
// lane_num: 1 or 4
void spd2010_datalane_set(uint8_t lane_num)
{
    uint8_t data[4] = {0};

    switch(lane_num)
    {
        case 1:
            data[0] = (0x02);
            data[1] = (0x00);
            data[2] = (0x3c);
            data[3] = (0x00);
            SPD2010_WRITE_BUF(data, 4);
            break;

        case 4:
            data[0] = (0x32);
            data[1] = (0x00);
            data[2] = (0x3c);
            data[3] = (0x00);
            SPD2010_WRITE_BUF(data, 4);
            break;

        default:
            break;
    }
}


// start_x: 0~411
void spd2010_window_set(uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h)
{
    uint8_t data[8] = {0};

    SPD2010_CS_CLR();
    data[0] = (0x02);
    data[1] = (0x00);
    data[2] = (0x2A);
    data[3] = (0x00);
    data[4] = (start_x >> 8);
    data[5] = (start_x & 0xff);
    data[6] = ((start_x + image_w - 1) >> 8);
    data[7] = ((start_x + image_w - 1) & 0xff);
    SPD2010_WRITE_BUF(data, 8);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    data[0] = (0x02);
    data[1] = (0x00);
    data[2] = (0x2B);
    data[3] = (0x00);
    data[4] = (start_y >> 8);
    data[5] = (start_y & 0xff);
    data[6] = ((start_y + image_h - 1) >> 8);
    data[7] = ((start_y + image_h - 1) & 0xff);
    SPD2010_WRITE_BUF(data, 8);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    data[0] = (0x02);
    data[1] = (0x00);
    data[2] = (0x2C);
    data[3] = (0x00);
    SPD2010_WRITE_BUF(data, 4);
    SPD2010_CS_SET();
}


/* 0x3A bit0~2: 7=RGB888  6=RGB666  5=RGB565   */
/* 0x36 bit3:   0:RGB     1:BGR   */
void spd2010_init(lcd_format_e format)
{
    //spd2010_reset();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20);spd2010_write_data(0x10);spd2010_write_data(0x10);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x62);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x61);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x5C);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x58);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x55);spd2010_write_data(0x55);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x54);spd2010_write_data(0x44);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x51);spd2010_write_data(0x11);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x4B);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x4A);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x49);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x47);spd2010_write_data(0x77);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x46);spd2010_write_data(0x66);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x45);spd2010_write_data(0x55);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x44);spd2010_write_data(0x44);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x43);spd2010_write_data(0x33);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x42);spd2010_write_data(0x22);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x41);spd2010_write_data(0x11);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x37);spd2010_write_data(0x12);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x36);spd2010_write_data(0x12);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x35);spd2010_write_data(0x11);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x34);spd2010_write_data(0x11);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x33);spd2010_write_data(0x20);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x32);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x31);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x30);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x27);spd2010_write_data(0x12);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x26);spd2010_write_data(0x12);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x25);spd2010_write_data(0x11);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x24);spd2010_write_data(0x11);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x23);spd2010_write_data(0x20);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x22);spd2010_write_data(0x72);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x21);spd2010_write_data(0x82);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x20);spd2010_write_data(0x81);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1B);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1A);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x16);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x15);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x11);spd2010_write_data(0x12);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x10);spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0C);spd2010_write_data(0x12);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0B);spd2010_write_data(0x43);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF);spd2010_write_data(0x20);spd2010_write_data(0x10);spd2010_write_data(0x11);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x6A);spd2010_write_data(0x10);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x69);spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x68);spd2010_write_data(0x34);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x67);spd2010_write_data(0x04);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x66);spd2010_write_data(0x38);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x65);spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x64);spd2010_write_data(0x34);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x63);spd2010_write_data(0x04);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x62);spd2010_write_data(0x38);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x61);spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x60);spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x55);spd2010_write_data(0x06);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x50);spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x30);spd2010_write_data(0xEE);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1E);spd2010_write_data(0x88);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1D);spd2010_write_data(0x88);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1C);spd2010_write_data(0x88);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x16);spd2010_write_data(0x99);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x15);spd2010_write_data(0x99);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x14);spd2010_write_data(0x34);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x13);spd2010_write_data(0xf0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0c);spd2010_write_data(0xF0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0B);spd2010_write_data(0xF0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0A);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x09);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x08);spd2010_write_data(0x70);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF);spd2010_write_data(0x20);spd2010_write_data(0x10);spd2010_write_data(0x12);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x36);spd2010_write_data(0xA0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2E);spd2010_write_data(0x1e);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2D);spd2010_write_data(0x2D);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2C);spd2010_write_data(0x26);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2B);spd2010_write_data(0x1e);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2A);spd2010_write_data(0x2D);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x21);spd2010_write_data(0x70);//vcom
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1F);spd2010_write_data(0xE6);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x18);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x15);spd2010_write_data(0x0F);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x12);spd2010_write_data(0x89);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x10);spd2010_write_data(0x0F);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0D);spd2010_write_data(0x66);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x06);spd2010_write_data(0x06);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x00);spd2010_write_data(0xCC);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF);spd2010_write_data(0x20);spd2010_write_data(0x10);spd2010_write_data(0x15);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2F);spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2E);spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2D);spd2010_write_data(0x35);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2C);spd2010_write_data(0x34);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2B);spd2010_write_data(0x32);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2A);spd2010_write_data(0x33);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x29);spd2010_write_data(0x1D);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x28); spd2010_write_data(0x1B);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x27); spd2010_write_data(0x19);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x26); spd2010_write_data(0x17);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x25); spd2010_write_data(0x09);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x24); spd2010_write_data(0x05);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x23); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x22); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x21); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x20); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0F); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0E); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0D); spd2010_write_data(0x35);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0C); spd2010_write_data(0x34);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0B); spd2010_write_data(0x32);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0A); spd2010_write_data(0x33);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x09); spd2010_write_data(0x16);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x08); spd2010_write_data(0x18);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x07); spd2010_write_data(0x1A);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x06); spd2010_write_data(0x1C);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x05); spd2010_write_data(0x04);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x04); spd2010_write_data(0x08);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x03); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x02); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x01); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x00); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x16);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2F); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2E); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2D); spd2010_write_data(0x35);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2C); spd2010_write_data(0x34);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2B); spd2010_write_data(0x32);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2A); spd2010_write_data(0x33);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x29); spd2010_write_data(0x1C);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x28); spd2010_write_data(0x1A);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x27); spd2010_write_data(0x18);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x26); spd2010_write_data(0x16);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x25); spd2010_write_data(0x08);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x24); spd2010_write_data(0x04);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x23); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x22); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x21); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x20); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0F); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0E); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0D); spd2010_write_data(0x35);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0C); spd2010_write_data(0x34);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0B); spd2010_write_data(0x32);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0A); spd2010_write_data(0x33);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x09); spd2010_write_data(0x17);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x08); spd2010_write_data(0x19);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x07); spd2010_write_data(0x1B);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x06); spd2010_write_data(0x1D);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x05); spd2010_write_data(0x05);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x04); spd2010_write_data(0x09);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x03); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x02); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x01); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x00); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x17);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x39); spd2010_write_data(0x3c);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x37); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1F); spd2010_write_data(0x80);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1A); spd2010_write_data(0x80);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x18); spd2010_write_data(0xA0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x16); spd2010_write_data(0x12);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x14); spd2010_write_data(0xAA);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x11); spd2010_write_data(0xAA);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x10); spd2010_write_data(0x0E);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0B); spd2010_write_data(0xC3);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x18);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x3A); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1F); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x01); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x00); spd2010_write_data(0x1E);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x2D);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x02); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x01); spd2010_write_data(0x3E);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xff); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x31);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x39); spd2010_write_data(0xf0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x38); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x37); spd2010_write_data(0xe8);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    switch(format)
    {
        case LCD_FORMAT_RGB565:
        case LCD_FORMAT_RGB666:
        case LCD_FORMAT_RGB888:
            spd2010_write_reg(0x36); spd2010_write_data(0x03);   // bit3: 0=RGB  1=BGR
            break;

        default:
            spd2010_write_reg(0x36); spd2010_write_data(0x0B);   // bit3: 0=RGB  1=BGR
            break;
    }
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x35); spd2010_write_data(0xCF);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x34); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x33); spd2010_write_data(0xBA);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x32); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x31); spd2010_write_data(0xA2);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x30); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2f); spd2010_write_data(0x95);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2e); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2d); spd2010_write_data(0x7e);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2c); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2b); spd2010_write_data(0x62);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2a); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x29); spd2010_write_data(0x44);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x28); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x27); spd2010_write_data(0xfc);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x26); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x25); spd2010_write_data(0xd0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x24); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x23); spd2010_write_data(0x98);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x22); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x21); spd2010_write_data(0x6f);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x20); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1f); spd2010_write_data(0x32);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1e); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1d); spd2010_write_data(0xf6);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1c); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1b); spd2010_write_data(0xb8);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1a); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x19); spd2010_write_data(0x6E);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x18); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x17); spd2010_write_data(0x41);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x16); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x15); spd2010_write_data(0xfd);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x14); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x13); spd2010_write_data(0xCf);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x12); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x11); spd2010_write_data(0x98);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x10); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0f); spd2010_write_data(0x89);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0e); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0d); spd2010_write_data(0x79);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0c); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0b); spd2010_write_data(0x67);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0a); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x09); spd2010_write_data(0x55);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x08); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x07); spd2010_write_data(0x3F);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x06); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x05); spd2010_write_data(0x28);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x04); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x03); spd2010_write_data(0x0E);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x02); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xff); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x32);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x39); spd2010_write_data(0xf0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x38); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x37); spd2010_write_data(0xe8);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    switch(format)
    {
        case LCD_FORMAT_RGB565:
        case LCD_FORMAT_RGB666:
        case LCD_FORMAT_RGB888:
            spd2010_write_reg(0x36); spd2010_write_data(0x03);   // bit3: 0=RGB  1=BGR
            break;

        default:
            spd2010_write_reg(0x36); spd2010_write_data(0x0B);   // bit3: 0=RGB  1=BGR
            break;
    }
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x35); spd2010_write_data(0xCF);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x34); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x33); spd2010_write_data(0xBA);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x32); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x31); spd2010_write_data(0xA2);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x30); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2f); spd2010_write_data(0x95);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2e); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2d); spd2010_write_data(0x7e);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2c); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2b); spd2010_write_data(0x62);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2a); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x29); spd2010_write_data(0x44);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x28); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x27); spd2010_write_data(0xfc);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x26); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x25); spd2010_write_data(0xd0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x24); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x23); spd2010_write_data(0x98);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x22); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x21); spd2010_write_data(0x6f);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x20); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1f); spd2010_write_data(0x32);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1e); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1d); spd2010_write_data(0xf6);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1c); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1b); spd2010_write_data(0xb8);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1a); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x19); spd2010_write_data(0x6E);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x18); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x17); spd2010_write_data(0x41);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x16); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x15); spd2010_write_data(0xfd);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x14); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x13); spd2010_write_data(0xCf);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x12); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x11); spd2010_write_data(0x98);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x10); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0f); spd2010_write_data(0x89);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0e); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0d); spd2010_write_data(0x79);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0c); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0b); spd2010_write_data(0x67);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0a); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x09); spd2010_write_data(0x55);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x08); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x07); spd2010_write_data(0x3F);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x06); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x05); spd2010_write_data(0x28);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x04); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x03); spd2010_write_data(0x0E);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x02); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xff); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x40);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x86); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x83); spd2010_write_data(0xC4);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x42);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x06); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x05); spd2010_write_data(0x3D);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x43);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x03); spd2010_write_data(0x04);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x45);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x03); spd2010_write_data(0x9C);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x01); spd2010_write_data(0x9C);
    SPD2010_CS_SET();


    /*
    // TP settings, if TP function is not used, the following lines of code will be cancelled

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x50);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x08); spd2010_write_data(0x55);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x05); spd2010_write_data(0x08);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x01); spd2010_write_data(0xA6);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x00); spd2010_write_data(0xA6);
    SPD2010_CS_SET();
    // TP code end
    */

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0xA0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x08); spd2010_write_data(0xE6);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x35); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();

    switch(format)
    {
        case LCD_FORMAT_RGB565:
        case LCD_FORMAT_BGR565:
            spd2010_write_reg(0x3A); spd2010_write_data(0x5); //0x07=RGB24, 0x06=RGB18, 0x05=RGB16
            break;

        case LCD_FORMAT_RGB666:
        case LCD_FORMAT_BGR666:
            spd2010_write_reg(0x3A); spd2010_write_data(0x6); //0x07=RGB24, 0x06=RGB18, 0x05=RGB16
            break;

        default:
            spd2010_write_reg(0x3A); spd2010_write_data(0x7); //0x07=RGB24, 0x06=RGB18, 0x05=RGB16
                        break;
    }
    SPD2010_CS_SET();


    SPD2010_CS_CLR();
    spd2010_write_reg(0x11);
    SPD2010_DELAY_MS(150);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x29);
    SPD2010_DELAY_MS(10);


}



