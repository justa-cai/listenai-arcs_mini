#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "chip.h"
#include "log_print.h"
#include "PSRAMManager.h"

#include "test_case.h"
#include "csk_timer.h"
#include "csk_gpio.h"
#include "csk_dvp.h"
#include "csk_i2c.h"
#include "csk_spi.h"

#include "lcd.h"
#include "touch.h"
#include "gui_paint.h"
//#include "image.h"

#include "ft5336.h"


void test_touch_i2c(void)
{
    uint8_t i2c_index = 1;
    uint8_t value = 0;
    sw_i2c_port_callback_t i2c_port;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* Initialize I2C */
    i2c_port.gpio_init    = sw_qspi_in_i2c_gpio_init;
    i2c_port.scl_set      = sw_qspi_in_i2c_scl_gpio_set;
    i2c_port.scl_clr      = sw_qspi_in_i2c_scl_gpio_clr;
    i2c_port.sda_set      = sw_qspi_in_i2c_sda_gpio_set;
    i2c_port.sda_clr      = sw_qspi_in_i2c_sda_gpio_clr;
    i2c_port.sda_get      = sw_qspi_in_i2c_sda_gpio_get;
    i2c_port.sda_dirout   = sw_qspi_in_i2c_sda_gpio_setdir_output;
    i2c_port.sda_dirin    = sw_qspi_in_i2c_sda_gpio_setdir_input;
    sw_i2c_init(QSPI_IN_I2C_INDEX, &i2c_port);

    while(1)
    {
        value = sw_i2c_read_reg8(i2c_index, FT5336_IIC_SLAVE_ADDRESS, FT5336_CHIP_ID_REG);
        VIDEO_LOG("[%s:%d] FT5336 CHIP ID = 0x%X", __func__, __LINE__, value);
        if(value != 0x11)
        {
            VIDEO_LOG("[%s:%d] FT5336 read chip id error, not 0x%x", __func__, __LINE__, 0x11);
        }
        DELAY_MS(10);

        value = sw_i2c_read_reg8(i2c_index, FT5336_IIC_SLAVE_ADDRESS, FT5336_GEST_ID_REG);
        VIDEO_LOG("[%s:%d] FT5336 GEST ID = 0x%X", __func__, __LINE__, value);
        DELAY_MS(10);

        //sw_i2c_read_reg8(FT5336_IIC_SLAVE_ADDRESS, FT5336_DEV_MODE_REG, 0x5A);
        value = sw_i2c_read_reg8(i2c_index, FT5336_IIC_SLAVE_ADDRESS, FT5336_DEV_MODE_REG);
        VIDEO_LOG("[%s:%d] FT5336 MODE = 0x%X", __func__, __LINE__, value);
        DELAY_MS(10);

        DELAY_MS(1000);
    }
}


void test_touch(void)
{
    uint8_t chip_id = 0;
    uint8_t touch_cnt = 0;
    uint8_t i = 0;
    touch_info_t info = {0};

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    touch_init();
    touch_control(TOUCH_ACTIVE_MODE);

    chip_id = TOUCH_I2C_READ_BYTE(FT5336_IIC_SLAVE_ADDRESS, FT5336_CHIP_ID_REG);
    VIDEO_LOG("[%s:%d] FT5336 CHIP ID = 0x%X", __func__, __LINE__, chip_id);

    while(1)
    {
        touch_cnt = touch_read(&info);
        VIDEO_LOG("[%s:%d] touch cnt=%d", __func__, __LINE__, touch_cnt);

        for(i = 0; i < touch_cnt; i++)
        {
            VIDEO_LOG("[%s:%d] info.Xpos[%d]=%d", __func__, __LINE__, i, info.Xpos[i]);
            VIDEO_LOG("[%s:%d] info.Ypos[%d]=%d", __func__, __LINE__, i, info.Ypos[i]);
            VIDEO_LOG("[%s:%d] info.Weight[%d]=%d", __func__, __LINE__, i, info.Weight[i]);
            VIDEO_LOG("[%s:%d] info.Area[%d]=%d", __func__, __LINE__, i, info.Area[i]);
            VIDEO_LOG("[%s:%d] info.Event[%d]=%d", __func__, __LINE__, i, info.Event[i]);
        }


        DELAY_MS(1000);
    }
}


void test_lcd_touch(void)
{
    uint8_t chip_id = 0;
    uint8_t touch_cnt = 0;
    uint8_t i = 0;
    touch_info_t info = {0};

    uint16_t image[25] = {0};
    uint16_t Xstart;
    uint16_t Ystart;
    uint16_t Xend;
    uint16_t Yend;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    for(i = 0; i < 25; i++)
    {
        image[i] = RED;
        image[i] = (((image[i] & 0x00FF) << 8) | ((image[i] & 0xFF00) >> 8));
    }

    lcd_init(LCD_FORMAT_RGB565);

#if 0
    VIDEO_LOG("Paint_NewImage");
    Paint_NewImage(LCD_WIDTH, LCD_HEIGHT, ROTATE_180, WHITE);

    VIDEO_LOG("Set Clear and Display Funtion");
    Paint_SetClearFuntion(gui_clear);
    Paint_SetDisplayFuntion(gui_draw_paint);

    VIDEO_LOG("Paint_Clear");
    Paint_Clear(WHITE);
    DELAY_MS(100);
#endif

    touch_init();
    touch_control(TOUCH_ACTIVE_MODE);

    chip_id = TOUCH_I2C_READ_BYTE(FT5336_IIC_SLAVE_ADDRESS, FT5336_CHIP_ID_REG);
    VIDEO_LOG("[%s:%d] FT5336 CHIP ID = 0x%X", __func__, __LINE__, chip_id);

    while(1)
    {
        touch_cnt = touch_read(&info);
        //VIDEO_LOG("[%s:%d] touch cnt=%d", __func__, __LINE__, touch_cnt);

        for(i = 0; i < touch_cnt; i++)
        {
            VIDEO_LOG("[%s:%d] info.Xpos[%d]=%d", __func__, __LINE__, i, info.Xpos[i]);
            VIDEO_LOG("[%s:%d] info.Ypos[%d]=%d", __func__, __LINE__, i, info.Ypos[i]);
            VIDEO_LOG("[%s:%d] info.Weight[%d]=%d", __func__, __LINE__, i, info.Weight[i]);
            VIDEO_LOG("[%s:%d] info.Area[%d]=%d", __func__, __LINE__, i, info.Area[i]);
            VIDEO_LOG("[%s:%d] info.Event[%d]=%d", __func__, __LINE__, i, info.Event[i]);

            //Paint_DrawCircle(info.Xpos[i], 240 - info.Ypos[i], 25, RED , DOT_PIXEL_2X2, DRAW_FILL_FULL);

            info.Xpos[i] = TOUCH_WIDTH - info.Xpos[i];
            Xstart = (info.Xpos[i] <= 1) ? 0 : (info.Xpos[i] - 2);
            Ystart = (info.Ypos[i] <= 1) ? 0 : (info.Ypos[i] - 2);
            Xend = (info.Xpos[i] >= (TOUCH_WIDTH -2 )) ? (TOUCH_WIDTH -1 ) : (info.Xpos[i] + 2);
            Yend = (info.Ypos[i] >= (TOUCH_HEIGHT - 2)) ? (TOUCH_HEIGHT - 1) : (info.Ypos[i] + 2);
            lcd_display(Xstart, Ystart, Xend, Yend, image);
        }

        DELAY_MS(1);
    }
}


void test_lcd_color(void)
{
    unsigned int cnt = 0;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    lcd_init(LCD_FORMAT_RGB565);

    while(1)
    {
        VIDEO_LOG("[%s:%d] test %d", __func__, __LINE__, cnt++);
        lcd_clear(BLACK);
        DELAY_MS(100);
        lcd_clear(RED);
        DELAY_MS(100);
        lcd_clear(GREEN);
        DELAY_MS(100);
        lcd_clear(BLUE);
        DELAY_MS(100);
        lcd_clear(YELLOW);
        DELAY_MS(100);
        lcd_clear(WHITE);
        DELAY_MS(100);
        DELAY_MS(100);
    }
}


static void gui_clear(uint16_t color)
{
    lcd_clear(color);
}

static void gui_draw_paint(uint16_t x, uint16_t y, uint16_t color)
{
    color = (((color & 0x00FF) << 8) | ((color & 0xFF00) >> 8));
    lcd_display(x, y, x, y, &color);
}

void test_lcd_display(void)
{
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    lcd_init(NULL);

    VIDEO_LOG("Paint_NewImage");
    Paint_NewImage(LCD_WIDTH, LCD_HEIGHT, ROTATE_180, WHITE);

    VIDEO_LOG("Set Clear and Display Funtion");
    Paint_SetClearFuntion(gui_clear);
    Paint_SetDisplayFuntion(gui_draw_paint);

    VIDEO_LOG("Paint_Clear");
    Paint_Clear(WHITE);
    DELAY_MS(100);

    VIDEO_LOG("Painting...");
    Paint_SetRotate(ROTATE_180);
    Paint_DrawString_EN (5, 10, "DEMO:",        &Font24,    YELLOW,  RED);
    Paint_DrawString_EN (5, 34, "Hello World",  &Font24,    BLUE,    CYAN);
    Paint_DrawFloatNum  (5, 150, 987.12345, 5, &Font20,    WHITE,   BLUE);
    Paint_DrawString_EN (5, 170, "LISTENAI",    &Font24,    BLUE,   WHITE);
    Paint_DrawString_CN (5, 190, "闂備胶鍘у畷顒�螞濞嗘挻鏅搁柤鎭掑劤缁犳ɑ銇勯锝囧⒌妤犵偛閰ｉ弫鎾绘晸閿燂拷",     &Font24CN,  WHITE,   RED);

    Paint_DrawRectangle (125+80, 10, 225+80, 58,    RED     ,DOT_PIXEL_2X2,DRAW_FILL_EMPTY);
    Paint_DrawLine      (125+80, 10, 225+80, 58,    MAGENTA ,DOT_PIXEL_2X2,LINE_STYLE_SOLID);
    Paint_DrawLine      (225+80, 10, 125+80, 58,    MAGENTA ,DOT_PIXEL_2X2,LINE_STYLE_SOLID);

    Paint_DrawCircle(150+50,100,  25,        BLUE    ,DOT_PIXEL_2X2,DRAW_FILL_EMPTY);
    Paint_DrawCircle(180+50,100,  25,        BLACK   ,DOT_PIXEL_2X2,DRAW_FILL_EMPTY);
    Paint_DrawCircle(210+50,100,  25,        RED     ,DOT_PIXEL_2X2,DRAW_FILL_EMPTY);
    Paint_DrawCircle(165+50,125,  25,        YELLOW  ,DOT_PIXEL_2X2,DRAW_FILL_EMPTY);
    Paint_DrawCircle(195+50,125,  25,        GREEN   ,DOT_PIXEL_2X2,DRAW_FILL_EMPTY);

    //Paint_DrawImage(gImage_1,5,70,60,60);

    DELAY_MS(3000);

    VIDEO_LOG("quit...");

    //lcd_deinit();
}



#if 0
#include "lvgl.h"
#include "lv_port_indev_template.h"
#include "lv_port_disp_template.h"
#include "lv_demos.h"


lv_ui guider_ui;
void test_lvgl_demos(void)
{
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();

    //lv_demo_stress();
    //lv_demo_music();
    setup_ui(&guider_ui);
    events_init(&guider_ui);

    while(1)
    {
        lv_task_handler();
        DELAY_MS(5);
    }
}


void test_lvgl_text(void)
{
    lv_init();
    lv_port_disp_init();

    lv_obj_t *label = lv_label_create(lv_scr_act());
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    lv_label_set_text(label,"Hello LISTENAI !!!");
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    lv_obj_center(label);
    //lv_obj_align(label, LV_ALIGN_OUT_TOP_LEFT, 0, 0);
    //lv_obj_align(label, LV_ALIGN_OUT_TOP_MID, 0, 0);
    //lv_obj_align(label, LV_ALIGN_OUT_TOP_RIGHT, 0, 0);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(1)
    {
        lv_task_handler();
        DELAY_MS(5);
    }
}

#endif



