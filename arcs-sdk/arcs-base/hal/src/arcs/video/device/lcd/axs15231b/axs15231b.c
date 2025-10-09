#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "axs15231b.h"


/********************************************************************************/
#define AXS15231B_CS_SET                      LCD_CS_SET
#define AXS15231B_CS_CLR                      LCD_CS_CLR
#define AXS15231B_DELAY_MS                    LCD_DELAY_MS
#define AXS15231B_WRITE_BUF(_pdata, _num)     LCD_SPI_WRITE(_pdata, _num)


/********************************************************************************/
#define AXS15231B_CMD_SLEEP_IN  0x10
#define AXS15231B_CMD_SLEEP_OUT 0x11
#define AXS15231B_CMD_INV_OFF   0x20
#define AXS15231B_CMD_INV_ON    0x21
#define AXS15231B_CMD_GAMSET    0x26
#define AXS15231B_CMD_DISP_OFF  0x28
#define AXS15231B_CMD_DISP_ON   0x29
#define AXS15231B_CMD_CASET     0x2A
#define AXS15231B_CMD_RASET     0x2B
#define AXS15231B_CMD_RAMWR     0x2C


/********************************************************************************/
static inline void LCD_WR_BUS(uint8_t data)
{
	AXS15231B_WRITE_BUF(&data, 1);
}

static inline void LCD_WR_REG(uint8_t reg)
{
	LCD_WR_BUS(0x02);
	LCD_WR_BUS(0x00);
	LCD_WR_BUS(reg);
	LCD_WR_BUS(0x00);
}

static inline void LCD_WR_REG_4DATA_MODE(uint8_t reg)
{
	LCD_WR_BUS(0x32);
	LCD_WR_BUS(0x00);
	LCD_WR_BUS(reg);
	LCD_WR_BUS(0x00);
}

static inline void LCD_WR_DATA8(uint8_t dat)
{
	LCD_WR_BUS(dat);
}


/********************************************************************************/
static int axs15231b_display_blanking_off(void)
{
    AXS15231B_CS_CLR();
    LCD_WR_REG(AXS15231B_CMD_DISP_ON);
    AXS15231B_CS_SET();
    return 0;
}


static int axs15231b_display_blanking_on(void)
{
    AXS15231B_CS_CLR();
    LCD_WR_REG(AXS15231B_CMD_DISP_OFF);
    AXS15231B_CS_SET();

    return 0;
}


void axs15231b_init(lcd_format_e format)
{
	// reset

	// SLEEP OUT
    AXS15231B_CS_CLR();
	LCD_WR_REG(AXS15231B_CMD_SLEEP_OUT);
	AXS15231B_CS_SET();
	AXS15231B_DELAY_MS(100);

	axs15231b_display_blanking_off();
}


void axs15231b_datalane_set(uint8_t lane_num)
{
    return;
}


void axs15231b_window_set(uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h)
{
    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    AXS15231B_CS_CLR();
	LCD_WR_REG(AXS15231B_CMD_CASET);
	LCD_WR_DATA8(start_x >> 8);
	LCD_WR_DATA8(start_x & 0xff);
	LCD_WR_DATA8((start_x + image_w - 1) >> 8);
	LCD_WR_DATA8((start_x + image_w - 1) & 0xff);
	AXS15231B_CS_SET();

	VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

	AXS15231B_CS_CLR();
	LCD_WR_REG(AXS15231B_CMD_RASET);
	LCD_WR_DATA8(start_y >> 8);
	LCD_WR_DATA8(start_y & 0xff);
	LCD_WR_DATA8((start_y + image_h - 1) >> 8);
	LCD_WR_DATA8((start_y + image_h - 1) & 0xff);
	AXS15231B_CS_SET();

	VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

	AXS15231B_CS_CLR();
    LCD_WR_REG_4DATA_MODE(AXS15231B_CMD_RAMWR);
    //AXS15231B_CS_SET();
}






