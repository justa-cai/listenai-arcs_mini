#include "st7789.h"


#define ST7789_CS_SET                      LCD_CS_SET
#define ST7789_CS_CLR                      LCD_CS_CLR
#define ST7789_DC_SET                      LCD_DC_SET
#define ST7789_DC_CLR                      LCD_DC_CLR
#define ST7789_WRITE_BUF(_pdata, _num)     LCD_SPI_WRITE(_pdata, _num)


/*******************************************************************************
function:
        Write data and commands
*******************************************************************************/
static void st7789_write_command(uint8_t data)
{    
    ST7789_CS_CLR();
    ST7789_DC_CLR();
    ST7789_WRITE_BUF(&data, 1);
    ST7789_CS_SET();
}

static void st7789_writedata_byte(uint8_t data)
{    
    ST7789_CS_CLR();
    ST7789_DC_SET();
    ST7789_WRITE_BUF(&data, 1);
    ST7789_CS_SET();
}

/* RGB565    uint16_t    big endian */
static void st7789_writebuf(void *pdata, uint32_t num)
{
    ST7789_CS_CLR();
    ST7789_DC_SET();
    ST7789_WRITE_BUF(pdata, num);
    ST7789_CS_SET();
}


/******************************************************************************
function:    
        Common register initialization
******************************************************************************/
void st7789_datalane_set(uint8_t lane_num)
{
    return;
}


void st7789_window_set(uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h)
{
    st7789_write_command(0x2A);
    st7789_writedata_byte(start_x >> 8);
    st7789_writedata_byte(start_x & 0xff);
    st7789_writedata_byte((start_x + image_w - 1) >> 8);
    st7789_writedata_byte((start_x + image_w - 1) & 0xff);

    st7789_write_command(0x2B);
    st7789_writedata_byte(start_y >> 8);
    st7789_writedata_byte(start_y & 0xff);
    st7789_writedata_byte((start_y + image_h - 1) >> 8);
    st7789_writedata_byte((start_y + image_h - 1) & 0xff);

    st7789_write_command(0x2C);
}


void st7789_init(lcd_format_e format)
{
    //st7789_reset();

    st7789_write_command(0x36);
    st7789_write_command(0x36);
    st7789_writedata_byte(0xA0);  // 0x20  bit7:MY  bit6:MX  bit5:MV  bit4:ML

    st7789_write_command(0x3A); 
    st7789_writedata_byte(0x05); // 0x03:RGB444 0x05:RGB565

    //st7789_write_command(0x21);  // INV_on
    st7789_write_command(0x20);  // INV_off

    st7789_write_command(0x2A);
    st7789_writedata_byte(0x00);
    st7789_writedata_byte(0x00);
    st7789_writedata_byte(0x01);
    st7789_writedata_byte(0x3F);

    st7789_write_command(0x2B);
    st7789_writedata_byte(0x00);
    st7789_writedata_byte(0x00);
    st7789_writedata_byte(0x00);
    st7789_writedata_byte(0xEF);

    st7789_write_command(0xB2);
    st7789_writedata_byte(0x0C);
    st7789_writedata_byte(0x0C);
    st7789_writedata_byte(0x00);
    st7789_writedata_byte(0x33);
    st7789_writedata_byte(0x33);

    st7789_write_command(0xB3);
    st7789_writedata_byte(0x00);
    st7789_writedata_byte(0x00); // 0x00:119fps 0x0F:60fps
    st7789_writedata_byte(0x00); // 0x00:119fps 0x0F:60fps

    st7789_write_command(0xB7);
    st7789_writedata_byte(0x35); 

    st7789_write_command(0xBB);
    st7789_writedata_byte(0x1F);

    st7789_write_command(0xC0);
    st7789_writedata_byte(0x2C);

    st7789_write_command(0xC2);
    st7789_writedata_byte(0x01);

    st7789_write_command(0xC3);
    st7789_writedata_byte(0x12);   

    st7789_write_command(0xC4);
    st7789_writedata_byte(0x20);

    st7789_write_command(0xC6);
    st7789_writedata_byte(0x0F); 

    st7789_write_command(0xD0);
    st7789_writedata_byte(0xA4);
    st7789_writedata_byte(0xA1);

    st7789_write_command(0xE0);
    st7789_writedata_byte(0xD0);
    st7789_writedata_byte(0x08);
    st7789_writedata_byte(0x11);
    st7789_writedata_byte(0x08);
    st7789_writedata_byte(0x0C);
    st7789_writedata_byte(0x15);
    st7789_writedata_byte(0x39);
    st7789_writedata_byte(0x33);
    st7789_writedata_byte(0x50);
    st7789_writedata_byte(0x36);
    st7789_writedata_byte(0x13);
    st7789_writedata_byte(0x14);
    st7789_writedata_byte(0x29);
    st7789_writedata_byte(0x2D);

    st7789_write_command(0xE1);
    st7789_writedata_byte(0xD0);
    st7789_writedata_byte(0x08);
    st7789_writedata_byte(0x10);
    st7789_writedata_byte(0x08);
    st7789_writedata_byte(0x06);
    st7789_writedata_byte(0x06);
    st7789_writedata_byte(0x39);
    st7789_writedata_byte(0x44);
    st7789_writedata_byte(0x51);
    st7789_writedata_byte(0x0B);
    st7789_writedata_byte(0x16);
    st7789_writedata_byte(0x14);
    st7789_writedata_byte(0x2F);
    st7789_writedata_byte(0x31);

    st7789_write_command(0x11);

    st7789_write_command(0x29);
}





#if 0

/******************************************************************************
function:    Refresh a certain area to Show a picture
parameter    :
      Xstart:   Start uint16_t x coordinate
      Ystart:    Start uint16_t y coordinate
      Xend  :    End uint16_t coordinates
      Yend  :    End uint16_t coordinates
      image :   Picture buffer (RGB565/uint16_t/big endian)
******************************************************************************/
void st7789_display(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend, uint16_t *pimage)
{
	uint32_t size = (Xend + 1 - Xstart) * (Yend + 1 - Ystart) * sizeof(uint16_t);

    //VIDEO_LOG("[%s:%d] start(%d, %d)  end(%d, %d)  pixel=%d", __func__, __LINE__, Xstart, Ystart, Xend, Yend, pixel_size);

	st7789_window_set(Xstart, Ystart, Xend, Yend);

#if 1
    st7789_writebuf((void *)pimage, size);
#else
    lcd_spi_data_bit_set(16);
    st7789_writebuf((void *)pimage, size);
    lcd_spi_data_bit_set(8);
#endif
}


/******************************************************************************
function:    Refresh a certain area to the same color
parameter    :
      Xstart: Start uint16_t x coordinate
      Ystart:    Start uint16_t y coordinate
      Xend  :    End uint16_t coordinates
      Yend  :    End uint16_t coordinates
      color :    Set the color (RGB565/uint16_t/big endian)
******************************************************************************/
void st7789_display_color(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend, uint16_t color)
{
    uint8_t buf[2];
    uint32_t pixel_size = (Xend + 1 - Xstart) * (Yend + 1 - Ystart);

    /* RGB565/uint16_t/big endian */
    buf[0] = (color >> 8) & 0xff;
    buf[1] = color & 0xff;

    st7789_window_set(Xstart, Ystart, Xend, Yend);

	while(pixel_size--)
	{
	    st7789_writebuf(buf, 2);
	}
}

#endif

