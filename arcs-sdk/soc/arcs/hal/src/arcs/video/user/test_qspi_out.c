#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "chip.h"
#include "log_print.h"
#include "systick.h"
#include "ClockManager.h"
#include "PSRAMManager.h"
#include "Driver_GPDMA.h"
#include "Driver_QSPI_LCD.h"

#include "test_case.h"
#include "qspi_out_case_config.h"
#include "csk_driver.h"
#include "lcd.h"
#include "touch.h"
#include "gui_paint.h"

#include "sw_qspi.h"


/*
 *  TEST_QSPIOUT_01_01: RGB888, 412x412, IRQ, lane=1
    TEST_QSPIOUT_01_02: RGB888, 412x412, IRQ, lane=2
    TEST_QSPIOUT_01_03: RGB888, 412x412, IRQ, lane=4
    TEST_QSPIOUT_02_01: RGB888, 412x412, DMA, lane=1
    TEST_QSPIOUT_02_02: RGB888, 412x412, DMA, lane=2
    TEST_QSPIOUT_02_03: RGB888, 412x412, DMA, lane=4
    TEST_QSPIOUT_03_01: RGB565, 412x412, IRQ, lane=1
    TEST_QSPIOUT_03_02: RGB565, 412x412, IRQ, lane=2
    TEST_QSPIOUT_03_03: RGB565, 412x412, IRQ, lane=4
    TEST_QSPIOUT_04_01: RGB565, 412x412, DMA, lane=1
    TEST_QSPIOUT_04_02: RGB565, 412x412, DMA, lane=2
    TEST_QSPIOUT_04_03: RGB565, 412x412, DMA, lane=4
    TEST_QSPIOUT_05_01: RGB565, 320x240, IRQ, lane=1
    TEST_QSPIOUT_05_02: RGB565, 320x240, DMA, lane=1
    TEST_QSPIOUT_06_01: speed
    TEST_QSPIOUT_06_02: CPOL/CPOH
    TEST_QSPIOUT_06_03: MSB/LSB
    TEST_QSPIOUT_06_04: CS_ctrl
    TEST_QSPIOUT_07_01: irq and status
    TEST_QSPIOUT_08_01: sys clk and reset
 */

static int32_t test_qspi_out_reset(void);
static int32_t test_qspi_out_case(uint16_t id, bool fillcolor);

static int32_t qspi_out_spd2010_fillcolor(qspi_lcd_config_t *qspi_cfg, uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint32_t color, bool dma_en);
static int32_t qspi_out_spd2010_line_flush(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint8_t *pbuf, uint32_t line_size_byte, uint8_t lane_num, bool dma_en);
static int32_t qspi_out_spd2010_image_flush(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint8_t *pbuf, uint32_t line_size_byte, uint8_t lane_num, bool dma_en);

static uint32_t qspi_out_get_color(qspi_lcd_format_t format);
static uint8_t qspi_out_get_format_byte(qspi_lcd_format_t format);
static void image_fillcolor(uint8_t *image_buf, uint16_t pixel_width, uint16_t pixel_height, qspi_lcd_format_t format);
static void image_buf_fillcolor(uint8_t *pbuf, uint32_t pixel_num, uint32_t color, qspi_lcd_format_t format);

static int32_t test_psram_qspi_out_case(uint16_t id, bool fillcolor);

void test_qspi_out(void)
{
    int32_t ret = FAILURE;
    uint32_t i = 0;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    //CHECK_FUNC_EXIT(test_qspi_out_08_01(), error);       // clk enable and reset

    //test_qspi_out_case(0x0101, true);       // lane=1 RGB888 IRQ
    //test_qspi_out_case(0x0102, true);       // lane=2 RGB888 IRQ
    //test_qspi_out_case(0x0103, true);       // lane=4 RGB888 IRQ
    //test_qspi_out_case(0x0201, true);       // lane=1 RGB888 DMA
    //test_qspi_out_case(0x0202, true);       // lane=2 RGB888 DMA
    //test_qspi_out_case(0x0203, true);       // lane=4 RGB888 DMA
    //test_qspi_out_case(0x0301, true);       // lane=1 RGB565 IRQ
    //test_qspi_out_case(0x0302, true);       // lane=2 RGB565 IRQ
    //test_qspi_out_case(0x0303, true);       // lane=4 RGB565 IRQ
    //test_qspi_out_case(0x0401, true);       // lane=1 RGB565 DMA
    //test_qspi_out_case(0x0402, true);       // lane=2 RGB565 DMA
    //test_qspi_out_case(0x0403, true);       // lane=4 RGB565 DMA
    //test_qspi_out_case(0x0501, true);       // lane=4 RGB888 IRQ
    //test_qspi_out_case(0x0502, true);       // lane=4 RGB888 DMA
    //test_qspi_out_case(0x0601, true);       // lane=4 RGB888 DMA clk
    //test_qspi_out_case(0x0602, true);       // lane=4 RGB888 DMA CPOL/CPOH
    //test_qspi_out_case(0x0603, true);       // lane=4 RGB888 DMA MSB/LSB
    //test_qspi_out_case(0x0604, true);       // lane=4 RGB888 DMA CS_ctrl

    test_psram_qspi_out_case(0x0403, true);       // lane=4 RGB565 DMA

#if 0
    for(i = 0; i < (sizeof(qspi_out_case_tab) / sizeof(qspi_out_CaseTypeDef)); i++)
    {
        qspi_out_case_tab[i].ret = test_qspi_out_case(qspi_out_case_tab[i].id, true);
        //CHECK_RET_EQ_EXIT(qspi_out_case_tab[i].ret, SUCCESS, error);
    }

    for(i = 0; i < (sizeof(qspi_out_case_tab) / sizeof(qspi_out_CaseTypeDef)); i++)
    {
        if(SUCCESS == qspi_out_case_tab[i].ret) {
            VIDEO_LOG("test blender case %04x SUCCESS", qspi_out_case_tab[i].id);
        } else {
            VIDEO_LOG("test blender case %04x FAILED", qspi_out_case_tab[i].id);
        }
    }
#endif

    ret = SUCCESS;
    VIDEO_LOG("[%s:%d]  all case test SUCCESS\r\n", __func__, __LINE__);
    return;

error:
    ret = FAILURE;
    VIDEO_LOG("[%s:%d]  case test FAILED\r\n", __func__, __LINE__);
    return;
}


/****************************************** CASE ************************************************/
static int32_t test_psram_qspi_out_case(uint16_t id, bool fillcolor)
{
    int32_t ret = FAILURE;
    uint32_t cnt = 0;
    uint32_t color = 0;
    uint32_t i = 0;
    qspi_lcd_config_t *pcfg = NULL;
    bool dma_en = false;
    uint32_t j = 0;

    uint8_t *image_buf = NULL;
    uint32_t image_size_byte = 0;
    uint8_t *back_buf = NULL;
    uint32_t back_size_byte = 0;

    for(i = 0; i < (sizeof(qspi_out_case_tab) / sizeof(qspi_out_CaseTypeDef)); i++)
    {
        if(id == qspi_out_case_tab[i].id) {
            pcfg = qspi_out_case_tab[i].pcfg;
            break;
        }
    }
    CHECK_POINT_NOT_NULL_EXIT(pcfg, error0);

    VIDEO_LOG("[%s:%d] test start id=0x%x", __func__, __LINE__, id);

#if CONFIG_AXS15231B_SUPPORT
    pcfg->cp = QSPI_LCD_CPOL0_CPOH0;
#endif

    back_size_byte = pcfg->width * pcfg->height * qspi_out_get_format_byte(pcfg->format);
    VIDEO_LOG("malloc size=0x%x byte, tiny_mem_perused=%d", back_size_byte, tiny_mem_perused());
    back_buf = tiny_malloc(back_size_byte);
    CHECK_POINT_NOT_NULL(back_buf);
    VIDEO_LOG("back_buf=0x%08x size=0x%x byte", back_buf, back_size_byte);
    image_buf_fillcolor(back_buf, pcfg->width * pcfg->height, color, pcfg->format);

    image_size_byte = pcfg->width * pcfg->height * qspi_out_get_format_byte(pcfg->format);
    VIDEO_LOG("malloc size=0x%x byte, tiny_mem_perused=%d", image_size_byte, tiny_mem_perused());
    image_buf = tiny_malloc(image_size_byte);
    CHECK_POINT_NOT_NULL(image_buf);
    VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, image_size_byte);
    //image_fillcolor(image_buf, pcfg->width, pcfg->height, pcfg->format);
    rgb565_colorbar_create((uint16_t *)image_buf, pcfg->width, pcfg->height, 20);
    rgb565_grid_create((uint16_t *)image_buf, pcfg->width, pcfg->height, pcfg->height);

    if(pcfg->txio == QSPI_LCD_TXIO_DMA)
    {
        dma_en = true;
        /* dma init */
        ret = qspi_lcd_gpdma_init(TEST_QSPI_OUT_GPDMA_CH);
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);
    }
    pcfg->txio = QSPI_LCD_TXIO_PIO;

    lcd_qspi_out_pinmux();
    lcd_qspi_out_reset();
    lcd_qspi_out_bl_enable();

    /* qspi_out init */
    ret = qspi_lcd_init(pcfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);

    /* LCD SPD2010 init */
    if(pcfg->format == QSPI_LCD_FORMAT_RGB565) {
        lcd_init(LCD_FORMAT_RGB565);
    } else {
        lcd_init(LCD_FORMAT_RGB888);
    }

    i = 0;
    while(1)
    {
        color = qspi_out_get_color(pcfg->format);
        image_buf_fillcolor(back_buf, pcfg->width * pcfg->height, color, pcfg->format);
        ret = qspi_out_spd2010_line_flush(0, 0, pcfg->width, pcfg->height, image_buf, back_size_byte, pcfg->lane_num, dma_en);
        VIDEO_LOG("[%s:%d] color=0x%x cnt=%d ret=%d", __func__, __LINE__, color, cnt++, ret);
        DELAY_MS(500);

        ret = qspi_out_spd2010_line_flush(0, 0, pcfg->width, pcfg->height, back_buf, image_size_byte, pcfg->lane_num, dma_en);
        VIDEO_LOG("[%s:%d] cnt=%d ret=%d", __func__, __LINE__, cnt++, ret);
        DELAY_MS(500);
    }

    while(0)
    {
        if((i % 5) == 0)
        {
            color = qspi_out_get_color(pcfg->format);
            image_buf_fillcolor(back_buf, pcfg->width * pcfg->height, color, pcfg->format);
            VIDEO_LOG("[%s:%d] color=0x%x cnt=%d", __func__, __LINE__, color, cnt++);
            //qspi_out_spd2010_image_flush(0, 0, pcfg->width, pcfg->height, back_buf, back_size_byte, pcfg->lane_num, dma_en);
            qspi_out_spd2010_line_flush(0, 0, pcfg->width, pcfg->height, back_buf, back_size_byte, pcfg->lane_num, dma_en);
        }
        else
        {
            qspi_out_spd2010_line_flush(0, 0, pcfg->width, pcfg->height, image_buf, image_size_byte, pcfg->lane_num, dma_en);
            VIDEO_LOG("[%s:%d] cnt=%d", __func__, __LINE__, cnt++);
        }
        i++;
        DELAY_MS(30);
    }

    ret = SUCCESS;
    goto error0;

error1:
    tiny_free(image_buf);
    qspi_lcd_reg_dump();
    gpdma_reg_dump(TEST_QSPI_OUT_GPDMA_CH);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t test_qspi_out_case(uint16_t id, bool fillcolor)
{
    int32_t ret = FAILURE;
    uint32_t cnt = 0;
    uint32_t color = 0;
    uint32_t i = 0;
    qspi_lcd_config_t *pcfg = NULL;
    bool dma_en = false;
    uint32_t j = 0;

    uint32_t image_size_byte = 0;
    uint8_t *image_buf = NULL;

    for(i = 0; i < (sizeof(qspi_out_case_tab) / sizeof(qspi_out_CaseTypeDef)); i++)
    {
        if(id == qspi_out_case_tab[i].id) {
            pcfg = qspi_out_case_tab[i].pcfg;
            break;
        }
    }
    CHECK_POINT_NOT_NULL_EXIT(pcfg, error0);

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

#if CONFIG_AXS15231B_SUPPORT
    pcfg->cp = QSPI_LCD_CPOL0_CPOH0;
#endif

    if(pcfg->txio == QSPI_LCD_TXIO_DMA)
    {
        dma_en = true;
        /* dma init */
        ret = qspi_lcd_gpdma_init(TEST_QSPI_OUT_GPDMA_CH);
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);
    }
    pcfg->txio = QSPI_LCD_TXIO_PIO;

    lcd_qspi_out_pinmux();
    lcd_qspi_out_reset();
    lcd_qspi_out_bl_enable();

    /* qspi_out init */
    ret = qspi_lcd_init(pcfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);

    /* LCD SPD2010 init */
    if(pcfg->format == QSPI_LCD_FORMAT_RGB565) {
        lcd_init(LCD_FORMAT_RGB565);
    } else {
        lcd_init(LCD_FORMAT_RGB888);
    }

    //gpdma_reg_dump(TEST_QSPI_OUT_GPDMA_CH);

    image_size_byte = TEST_QSPI_OUT_IMAGE_SIZE_W * TEST_QSPI_OUT_IMAGE_SIZE_H * qspi_out_get_format_byte(pcfg->format);
    VIDEO_LOG("malloc size=0x%x byte, tiny_mem_perused=%d", image_size_byte, tiny_mem_perused());
    image_buf = tiny_malloc(image_size_byte);
    CHECK_POINT_NOT_NULL(image_buf);
    VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, image_size_byte);
    image_fillcolor(image_buf, TEST_QSPI_OUT_IMAGE_SIZE_W, TEST_QSPI_OUT_IMAGE_SIZE_H, pcfg->format);

    i = 0;
    while(1)
    {
        if((i % 5) == 0)
        {
            //qspi_lcd_init(pcfg);
            color = qspi_out_get_color(pcfg->format);
            VIDEO_LOG("[%s:%d] color=0x%x cnt=%d", __func__, __LINE__, color, cnt++);
            qspi_out_spd2010_fillcolor(pcfg, 0, 0, pcfg->width, pcfg->height, color, dma_en);
        }
        else
        {
            //qspi_lcd_init(pcfg);
            //qspi_out_spd2010_fillcolor(pcfg, 0, 0, pcfg->width, pcfg->height, RGB888_BLACK, dma_en);
            qspi_out_spd2010_image_flush(TEST_QSPI_OUT_IMAGE_START_X, TEST_QSPI_OUT_IMAGE_START_Y, \
                    TEST_QSPI_OUT_IMAGE_SIZE_W, TEST_QSPI_OUT_IMAGE_SIZE_H, \
                    image_buf, TEST_QSPI_OUT_IMAGE_SIZE_W * qspi_out_get_format_byte(pcfg->format), pcfg->lane_num, dma_en);
        }
        i++;
        DELAY_MS(10);
    }

    ret = SUCCESS;
    goto error0;

error1:
    tiny_free(image_buf);
    qspi_lcd_reg_dump();
    gpdma_reg_dump(TEST_QSPI_OUT_GPDMA_CH);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t qspi_out_spd2010_fillcolor(qspi_lcd_config_t *qspi_cfg, uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h, uint32_t color, bool dma_en)
{
    int32_t ret = FAILURE;
    uint16_t i,j;
    uint8_t *line_buf = NULL;
    uint32_t line_size_byte = 0;
    uint8_t *pbuf = NULL;
    uint32_t timeout = 0;
    uint32_t rdata, cl_txfifo, cl_rxfifo;

    VIDEO_LOG("[%s:%d] format=%d color=0x%x dma=%d", __func__, __LINE__, qspi_cfg->format, color, dma_en);

    /* malloc */
    line_size_byte = image_w * qspi_out_get_format_byte(qspi_cfg->format);
    VIDEO_LOG("malloc size=0x%x byte, tiny_mem_perused=%d", line_size_byte, tiny_mem_perused());
    if(0 != (line_size_byte % 4)) {
        VIDEO_LOG("[%s:%d] No 4-byte alignment", __func__, __LINE__);
    }
    line_buf = tiny_malloc(line_size_byte);
    CHECK_POINT_NOT_NULL(line_buf);
    VIDEO_LOG("line_buf=0x%08x size=0x%x byte", line_buf, line_size_byte);
    image_buf_fillcolor(line_buf, image_w, color, qspi_cfg->format);

#if 0
    if(true == dma_en)
    {
        /* qspi_out init */
        qspi_lcd_reset();
        ret = qspi_lcd_init(qspi_cfg);
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);
        ret = qspi_lcd_set_lane_num(1);
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);
    }
#endif

    lcd_window_set(start_x, start_y, image_w, image_h);

    LCD_CS_CLR();
    lcd_datalane_set(qspi_cfg->lane_num);
    qspi_lcd_set_lane_num(qspi_cfg->lane_num);
    //DELAY_US(100);

    if(true == dma_en)
    {
        qspi_lcd_set_dma_size(line_size_byte * image_h);

        for (i = start_y; i < (start_y + image_h); i++)
        {
            qspi_lcd_dma_enable();

            ret = qspi_lcd_gpdma_start(TEST_QSPI_OUT_GPDMA_CH, line_buf, line_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);

            timeout = 3000000;  // wait blender done, timeout=1000ms
            while(!qspi_lcd_gpdma_get_cnt())
            {
                DELAY_US(1);
                if(timeout-- == 0)
                {
                    VIDEO_LOG("[%s:%d] wait timeout line=%d", __func__, __LINE__, i);
                    ret = FAILURE;
                    //break;
                    goto error0;
                }
            }
            //qspi_lcd_dma_disable();
        }
    }
    else
    {
        for (i = start_y; i < (start_y + image_h); i++)
        {
            ret = qspi_lcd_write(line_buf, image_w * qspi_out_get_format_byte(qspi_cfg->format));
            if(ret != CSK_DRIVER_OK) {
                VIDEO_LOG("[%s:%d] i=%d", __func__, __LINE__, i);
            }

            timeout = 3000000;  // wait done, timeout=1000ms
            while(!is_qspi_lcd_irq_tx_done())
            {
                DELAY_US(1);
                if(timeout-- == 0)
                {
                    VIDEO_LOG("[%s:%d] wait timeout", __func__, __LINE__);
                    ret = FAILURE;
                    goto error0;
                }
            }
        }
    }
    ret = SUCCESS;

error0:
    qspi_lcd_wait_done();
    qspi_lcd_dma_disable();
    LCD_CS_SET();
    qspi_lcd_set_lane_num(1);
    tiny_free(line_buf);

    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    return ret;
}


static int32_t qspi_out_spd2010_line_flush(uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h, uint8_t *pbuf, uint32_t line_size_byte, uint8_t lane_num, bool dma_en)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;

    VIDEO_LOG("[%s:%d] start(%d,%d) w=%d h=%d", __func__, __LINE__, start_x, start_y, image_w, image_h);
    VIDEO_LOG("[%s:%d] pbuf=0x%x size=%d Byte", __func__, __LINE__, pbuf, line_size_byte);
    CHECK_POINT_NOT_NULL(pbuf);

    /* malloc */
    if (0 != (line_size_byte % 4)) {
        VIDEO_LOG("[%s:%d] line_size_byte=%d No 4-byte alignment", __func__, __LINE__, line_size_byte);
    }
    if (line_size_byte > 0xFFFFFF) {
        VIDEO_LOG("[%s:%d] line_size_byte=%d is over 0xFFFFFF", __func__, __LINE__, line_size_byte);
    }

    lcd_window_set(start_x, start_y, image_w, image_h);
    //LCD_CS_CLR();
    lcd_datalane_set(lane_num);
    qspi_lcd_set_lane_num(lane_num);
    //DELAY_US(100);

    if(true == dma_en)
    {
        qspi_lcd_set_dma_size(line_size_byte);
        qspi_lcd_dma_enable();

        ret = qspi_lcd_gpdma_start(TEST_QSPI_OUT_GPDMA_CH, pbuf, line_size_byte / sizeof(uint32_t));
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);

        timeout = 3000000;  // wait GPDMA done, timeout=1000ms
        while(!qspi_lcd_gpdma_get_cnt())
        {
            DELAY_US(1);
            if(timeout-- == 0)
            {
                VIDEO_LOG("[%s:%d] wait timeout", __func__, __LINE__);
                ret = FAILURE;
                goto error0;
            }
        }
    }
    else
    {
        uint32_t i = 0;
//        for (i = ys; i < ye; i++)
//        {
//            ret = qspi_lcd_write(pbuf + (xe - xs) * 3 * (i - ys), (xe - xs) * 3);
//            if(ret != CSK_DRIVER_OK) {
//                VIDEO_LOG("[%s:%d] i=%d", __func__, __LINE__, i);
//            }
//            while(!is_qspi_lcd_irq_tx_done());
//        }
//        for (i = 0; i < 30; i++)
//        {
//            VIDEO_LOG("i=%d pbuf=0x%x", i, pbuf[i]);
//        }

        for (i = 0; i < line_size_byte; i++)
        {
            ret = qspi_lcd_write(pbuf + i, 1);
            //ret = qspi_lcd_write(pbuf + i + 12, 1);
            if(ret != CSK_DRIVER_OK) {
                VIDEO_LOG("[%s:%d] i=%d", __func__, __LINE__, i);
            }

            timeout = 3000000;  // wait done, timeout=1000ms
            while(!is_qspi_lcd_irq_tx_done())
            {
                DELAY_US(1);
                if(timeout-- == 0)
                {
                    VIDEO_LOG("[%s:%d] wait timeout", __func__, __LINE__);
                    ret = FAILURE;
                    goto error0;
                }
            }
        }
    }

    ret = SUCCESS;

error0:
    qspi_lcd_wait_done();
    qspi_lcd_dma_disable();
    LCD_CS_SET();
    qspi_lcd_set_lane_num(1);

    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    return ret;
}


static int32_t qspi_out_spd2010_image_flush(uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h, uint8_t *pbuf, uint32_t line_size_byte, uint8_t lane_num, bool dma_en)
{
    int32_t ret = FAILURE;
    uint32_t i = 0;

    for (i = 0; i < image_h; i++)
    {
        ret = qspi_out_spd2010_line_flush(start_x, start_y + i, image_w, 1, pbuf + line_size_byte * i, line_size_byte, lane_num, dma_en);
    }

    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    return ret;
}



static uint8_t qspi_out_get_format_byte(qspi_lcd_format_t format)
{
    switch(format)
    {
        case QSPI_LCD_FORMAT_RGB565:
            return 2;

        case QSPI_LCD_FORMAT_RGB888:
            return 3;

        default:
            return 0;
    }
}


const static uint16_t rgb565_color_tab[] = {
        RGB565_WHITE,
        RGB565_RED,
        RGB565_GREEN,
        RGB565_BLUE,
        RGB565_YELLOW,
        RGB565_BRED,
        RGB565_GBLUE,
        RGB565_BLACK,
};

const static uint32_t rgb888_color_tab[] = {
        RGB888_WHITE,
        RGB888_RED,
        RGB888_GREEN,
        RGB888_BLUE,
        RGB888_YELLOW,
        RGB888_BRED,
        RGB888_GBLUE,
        RGB888_BLACK,
};

static uint32_t qspi_out_get_color(qspi_lcd_format_t format)
{
    static uint8_t cnt = 0;

    switch(format)
    {
        case QSPI_LCD_FORMAT_RGB565:
            return rgb565_color_tab[cnt++ % (sizeof(rgb565_color_tab) / sizeof(uint16_t))];

        case QSPI_LCD_FORMAT_RGB888:
            return rgb888_color_tab[cnt++ % (sizeof(rgb888_color_tab) / sizeof(uint32_t))];

        default:
            return 0xFFFFFFFF;
    }
}


static void image_buf_fillcolor(uint8_t *pbuf, uint32_t pixel_num, uint32_t color, qspi_lcd_format_t format)
{
    uint32_t i = 0;

    if (format == QSPI_LCD_FORMAT_RGB565)
    {
        for (i = 0; i < pixel_num; i++)
        {
            *pbuf++ = color & 0xFF;
            *pbuf++ = (color >> 8) & 0xFF;
        }
    }
    else
    {
        for (i = 0; i < pixel_num; i++)
        {
            *pbuf++ = color & 0xFF;
            *pbuf++ = (color >> 8) & 0xFF;
            *pbuf++ = (color >> 16) & 0xFF;
        }
    }
}


static void image_fillcolor(uint8_t *image_buf, uint16_t pixel_width, uint16_t pixel_height, qspi_lcd_format_t format)
{
    switch(format)
    {
        case QSPI_LCD_FORMAT_RGB888:
            rgb888_colorbar_create(image_buf, pixel_width, pixel_height, pixel_height/5);
            rgb888_grid_create(image_buf, pixel_width, pixel_height, pixel_height);
            break;

        case QSPI_LCD_FORMAT_RGB565:
            rgb565_colorbar_create((uint16_t *)image_buf, pixel_width, pixel_height, pixel_height/5);
            rgb565_grid_create((uint16_t *)image_buf, pixel_width, pixel_height, pixel_height);
            break;

        default:
            VIDEO_LOG("[%s:%d] format=%d is error", __func__, __LINE__, format);
            break;
    }
}


/* test qspi_out : clk enable and reset */
static int32_t test_qspi_out_reset(void)
{
    int32_t ret = FAILURE;

    qspi_lcd_config_t qspi_out_cfg = {
            .clk_hz = 12000000,
            .txio = QSPI_LCD_TXIO_PIO,          // QSPI_LCD_TXIO_DMA
            .cp = QSPI_LCD_CPOL1_CPOH1,
            .is_msb = true,
            .lane_num = 4,
            .width = 412,
            .height = 412,
            .format = QSPI_LCD_FORMAT_RGB888,
    };

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    qspi_lcd_reset();
    qspi_lcd_init(&qspi_out_cfg);
    qspi_lcd_set_lane_num(qspi_out_cfg.lane_num);
    qspi_lcd_reg_dump();
    VIDEO_LOG("[%s:%d]\r\n", __func__, __LINE__);

    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_IDREV.all,       0x02002044, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_TRANSFMT.all,    0x00020703, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_DIRECTIO.all,    0x0000313f, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_TRANSCTRL.all,   0x00800000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_CMD.all,         0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_ADDR.all,        0x00000000, error);
    //CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_DATA.all,        0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_CTRL.all,        0x00010100, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_STATUS.all,      0x00404000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_INTREN.all,      0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_INTRST.all,      0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_TIMING.all,      0x00001201, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_MEMCTRL.all,     0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_SLVST.all,       0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_SLVDATACNT.all,  0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_LCD_TX.all,      0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_CONFIG.all,      0x00004b33, error);

    qspi_lcd_reset();
    qspi_lcd_reg_dump();
    VIDEO_LOG("[%s:%d]\r\n", __func__, __LINE__);

    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_IDREV.all,       0x02002044, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_TRANSFMT.all,    0x00020780, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_DIRECTIO.all,    0x0000313d, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_TRANSCTRL.all,   0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_CMD.all,         0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_ADDR.all,        0x00000000, error);
    //CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_DATA.all,        0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_CTRL.all,        0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_STATUS.all,      0x00404000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_INTREN.all,      0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_INTRST.all,      0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_TIMING.all,      0x00000201, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_MEMCTRL.all,     0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_SLVST.all,       0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_SLVDATACNT.all,  0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_LCD_TX.all,      0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_LCD->REG_CONFIG.all,      0x00004b33, error);

    ret = SUCCESS;

error:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


#if 0
static void spd2010_fillcolor_sw(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint32_t color, uint8_t lane_num)
{
    uint16_t i, j;

    spd2010_window_set(xs, ys, xe, ye);

    LCD_CS_Clr();
    if(lane_num == 4)
    {
        spd2010_datalane_set(4);
        for (i=ys; i<ye; i++)
        {
            for (j=xs; j<xe; j++)
            {
                sw_qspi_write8_4lane((color >> 16) & 0xFF);
                sw_qspi_write8_4lane((color >> 8) & 0xFF);
                sw_qspi_write8_4lane(color & 0xFF);
            }
        }
    }
    else
    {
        spd2010_datalane_set(1);
        for (i=ys; i<ye; i++)
        {
            for (j=xs; j<xe; j++)
            {
                sw_qspi_write8_1lane((color >> 16) & 0xFF);
                sw_qspi_write8_1lane((color >> 8) & 0xFF);
                sw_qspi_write8_1lane(color & 0xFF);
            }
        }
    }
    LCD_CS_Set();
}

void test_lcd_sw_qspi(void)
{
    uint32_t cnt = 0;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    sw_qspi_init();

    lcd_spd2010_init(LCD_SPD2010_FORMAT_RGB888);
    DELAY_MS(100);

    while(0)        // 1lane
    {
        VIDEO_LOG("[%s:%d] test %d", __func__, __LINE__, cnt++);
        lcd_spd2010_fillcolor_sw(0, 0, LCD_SPD2010_WIDTH_MAX-1, LCD_SPD2010_HEIGH_MAX-1, RGB888_RED, 1);
        DELAY_MS(500);
        lcd_spd2010_fillcolor_sw(0, 0, LCD_SPD2010_WIDTH_MAX-1, LCD_SPD2010_HEIGH_MAX-1, RGB888_GREEN, 1);
        DELAY_MS(500);
        lcd_spd2010_fillcolor_sw(0, 0, LCD_SPD2010_WIDTH_MAX-1, LCD_SPD2010_HEIGH_MAX-1, RGB888_BLUE, 1);
        DELAY_MS(1500);
    }

    while(1)        // 4lane
    {
        VIDEO_LOG("[%s:%d] test %d", __func__, __LINE__, cnt++);
        lcd_spd2010_fillcolor_sw(0, 0, LCD_SPD2010_WIDTH_MAX-1, LCD_SPD2010_HEIGH_MAX-1, RGB888_RED, 4);
        DELAY_MS(500);
        lcd_spd2010_fillcolor_sw(0, 0, LCD_SPD2010_WIDTH_MAX-1, LCD_SPD2010_HEIGH_MAX-1, RGB888_GREEN, 4);
        DELAY_MS(500);
        lcd_spd2010_fillcolor_sw(0, 0, LCD_SPD2010_WIDTH_MAX-1, LCD_SPD2010_HEIGH_MAX-1, RGB888_BLUE, 4);
        DELAY_MS(1500);
    }
}


void test_lcd_normal_spi(void)
{
    uint32_t cnt = 0;
    uint32_t x,y;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    lcd_spd2010_init(LCD_SPD2010_FORMAT_RGB888);

    lcd_spi_init();
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 5, 0);   // CS
    enable_GINT();
    DELAY_MS(10);

    while(1)          // test normal SPI
    {
        VIDEO_LOG("[%s:%d] test %d", __func__, __LINE__, cnt++);

        lcd_spd2010_window_set(0, 0, LCD_SPD2010_WIDTH_MAX-1, LCD_SPD2010_HEIGH_MAX-1);
        LCD_CS_CLR();
        lcd_spd2010_datalane_set(1);
        /*
         * spi init
         * send data
         */
        LCD_CS_SET();

        DELAY_MS(500);
    }
}


static void test_qspi_out_gpio(void)
{
    void *gGpioADev = NULL;
    void *gGpioBDev = NULL;

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, 0);   // CS
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22, 0);   // CLK
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 23, 0);   // D0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 24, 0);   // D1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 25, 0);   // D2
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 26, 0);   // D3

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 8, 0);   // GPIO LCD_RESET
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 1, 0);   // GPIO LCD_TE
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 0, 0);   // GPIO LCD_BK_EN

    gGpioADev = GPIOA();
    //GPIO_Initialize(gGpioBDev, NULL, NULL);
    GPIO_SetDir(gGpioADev, (1UL << 21) | (1UL << 22) | (1UL << 23) | (1UL << 24) | (1UL << 25) | (1UL << 26), CSK_GPIO_DIR_OUTPUT);

    gGpioBDev = GPIOB();
    //GPIO_Initialize(gGpioBDev, NULL, NULL);
    GPIO_SetDir(gGpioBDev, (1UL << 0) | (1UL << 1) | (1UL << 8), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(gGpioBDev, (1UL << 8), 1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(1)
    {
        GPIO_PinWrite(gGpioBDev, (1UL << 1), 1);            // GPIO LCD_BK_EN
        DELAY_MS(1000);

        GPIO_PinWrite(gGpioBDev, (1UL << 1), 0);            // GPIO LCD_BK_EN
        DELAY_MS(1000);

        VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    }

    while(0)
    {
        GPIO_PinWrite(gGpioADev, (1UL << 21), 1);
        GPIO_PinWrite(gGpioADev, (1UL << 22), 1);
        GPIO_PinWrite(gGpioADev, (1UL << 23), 1);
        GPIO_PinWrite(gGpioADev, (1UL << 24), 1);
        GPIO_PinWrite(gGpioADev, (1UL << 25), 1);
        GPIO_PinWrite(gGpioADev, (1UL << 26), 1);
        GPIO_PinWrite(gGpioBDev, (1UL << 0), 1);
        GPIO_PinWrite(gGpioBDev, (1UL << 1), 1);
        GPIO_PinWrite(gGpioBDev, (1UL << 8), 1);
        DELAY_MS(20);

        GPIO_PinWrite(gGpioADev, (1UL << 21), 0);
        GPIO_PinWrite(gGpioADev, (1UL << 22), 0);
        GPIO_PinWrite(gGpioADev, (1UL << 23), 0);
        GPIO_PinWrite(gGpioADev, (1UL << 24), 0);
        GPIO_PinWrite(gGpioADev, (1UL << 25), 0);
        GPIO_PinWrite(gGpioADev, (1UL << 26), 0);
        GPIO_PinWrite(gGpioBDev, (1UL <<  0), 0);
        GPIO_PinWrite(gGpioBDev, (1UL <<  1), 0);
        GPIO_PinWrite(gGpioBDev, (1UL <<  8), 0);
        DELAY_MS(10);

        VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    }
}


static int32_t test_qspi_out_spd2010(void)
{
    int32_t ret = FAILURE;

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    /* SPD2010 GPIO init */
    sw_qspi_init();

    /* LCD SPD2010 init */
    lcd_spd2010_init(LCD_SPD2010_FORMAT_RGB888);

    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    while(1)
    {
        VIDEO_LOG("[%s:%d] ", __func__, __LINE__);
        lcd_spd2010_fillcolor_sw(0, 0, 400, 400, RGB888_RED, 1);
        VIDEO_LOG("[%s:%d] ", __func__, __LINE__);
        DELAY_MS(1000);
    }

    while(1);

    return ret;
}

#endif


