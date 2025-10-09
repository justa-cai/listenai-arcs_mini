#include "Driver_GPDMA.h"
#include "log_print.h"
#include "chip.h"
#include "systick.h"
#include "ClockManager.h"
#include "PSRAMManager.h"

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "test_case.h"
#include "rgb_case_config.h"
#include "csk_rgb.h"

/*
 *  TEST_RGB_01_01  sync-de, RGB888,   RGB888,   800x480
 *  TEST_RGB_01_02  sync-de, RGB888,   RGB666,   800x480
 *  TEST_RGB_01_03  sync-de, RGB888,   BGR888,   800x480
 *  TEST_RGB_01_04  sync-de, RGB888,   BGR666,   800x480
 *  TEST_RGB_01_05  sync-de, XRGB8888, RGB888,   800x480
 *  TEST_RGB_01_06  sync-de, XRGB8888, RGB666,   800x480
 *  TEST_RGB_01_07  sync-de, XRGB8888, BGR888,   800x480
 *  TEST_RGB_01_08  sync-de, XRGB8888, BGR666,   800x480
 *  TEST_RGB_01_09  sync-de, RGB565,   RGB565,   800x480
 *  TEST_RGB_01_10  sync-de, RGB565,   BGR565,   800x480
 *
 *  TEST_RGB_02_01  sync,    RGB888,   RGB888,   800x480
 *  TEST_RGB_02_02  sync,    RGB565,   RGB565,   800x480
 *
 *  TEST_RGB_03_01  8-wires sync-de, RGB888,   RGB888,   800x480
 *  TEST_RGB_03_02  8-wires sync-de, RGB565,   RGB565,   800x480
 *
 *  TEST_RGB_04_01  sync-de, RGB888,   BGR666,   800x480  out_lsb
 *  TEST_RGB_04_02  sync-de, RGB565,   RGB565,   800x480  out_lsb
 *
 *  TEST_RGB_05_01  Polarity: VS/HS/DE/PCLK
 *  TEST_RGB_06_01  PCLK_Hz
 *  TEST_RGB_07_01  irq mask clear and status
 *  TEST_RGB_08_01  sys clk and reset
 */

static int32_t test_rgb_clk_out(void);
static int32_t test_rgb_reset(void);
static int32_t test_rgb_case(uint16_t id);
static void test_rgb_lcd_gpio(void);
static void test_rgb_reg_ctrl(void);
static void image_fillcolor(uint8_t *image_buf, uint16_t pixel_width, uint16_t pixel_height, rgb_emFormatIn format);
static uint8_t rgb_get_format_byte(rgb_emFormatIn format);
static void get_rgb_cfg(uint16_t id, RGB_InitTypeDef **pcfg);

void test_rgb(void)
{
    int32_t ret = FAILURE;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

//    while(1)
    {
        CHECK_FUNC_EXIT(test_rgb_case(0x0101), error);    // pass
//        CHECK_FUNC_EXIT(test_rgb_case(0x0102), error);    // pass
//        CHECK_FUNC_EXIT(test_rgb_case(0x0103), error);    // pass
//        CHECK_FUNC_EXIT(test_rgb_case(0x0104), error);    // pass
//        CHECK_FUNC_EXIT(test_rgb_case(0x0105), error);    // pass
//        CHECK_FUNC_EXIT(test_rgb_case(0x0106), error);    // pass
//        CHECK_FUNC_EXIT(test_rgb_case(0x0107), error);    // pass
//        CHECK_FUNC_EXIT(test_rgb_case(0x0108), error);    // pass
//        CHECK_FUNC_EXIT(test_rgb_case(0x0109), error);    // pass
//        CHECK_FUNC_EXIT(test_rgb_case(0x0110), error);    // pass
//        CHECK_FUNC_EXIT(test_rgb_case(0x0201), error);    // pass
//        CHECK_FUNC_EXIT(test_rgb_case(0x0202), error);    // pass
//        CHECK_FUNC_EXIT(test_rgb_case(0x0301), error);    // 8-Wire RGB888
//        CHECK_FUNC_EXIT(test_rgb_case(0x0302), error);    // 8-Wire RGB565
//        CHECK_FUNC_EXIT(test_rgb_case(0x0401), error);    // pass
//        CHECK_FUNC_EXIT(test_rgb_case(0x0402), error);    // pass
//        CHECK_FUNC_EXIT(test_rgb_case(0x0501), error);    // pass
//        CHECK_FUNC_EXIT(test_rgb_clk_out(), error);       // clk div     // pass
//        CHECK_FUNC_EXIT(test_rgb_reset(), error);         // clk enable and reset     // pass
    }

    //test_rgb_reg_ctrl();
    //test_rgb_lcd_gpio();

    ret = SUCCESS;
    VIDEO_LOG("[%s:%d]  all case test SUCCESS\r\n", __func__, __LINE__);
    return;

error:
    ret = FAILURE;
    VIDEO_LOG("[%s:%d]  case test FAILED\r\n", __func__, __LINE__);
    return;
}


static int32_t test_rgb_case(uint16_t id)
{
    int32_t ret = FAILURE;
    uint32_t image_size_byte = 0;
    uint8_t *image_buf = NULL;
    RGB_InitTypeDef *pcfg = NULL;
    uint32_t gpdma_cnt = 0;
    uint32_t times = 0;
    uint32_t timeout = 0;
    RGB_irq_cnt_t *prgb_irq_cnt = NULL;

    VIDEO_LOG("[%s:%d] case_id=0x%04x", __func__, __LINE__, id);

    get_rgb_cfg(id, &pcfg);
    CHECK_POINT_NOT_NULL(pcfg);

    /* malloc */
#ifdef TEST_RGB_PSRAM_ADDR      // must do PSRAM_Initialize(NULL, NULL, 1);
    image_size_byte = pcfg->img_width * pcfg->img_height * rgb_get_format_byte(pcfg->format_in);
    image_buf = (uint8_t *)TEST_RGB_PSRAM_ADDR;
    CHECK_POINT_NOT_NULL(image_buf);
    VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, image_size_byte);
    image_fillcolor(image_buf, pcfg->img_width, pcfg->img_height, pcfg->format_in);
#else
    image_size_byte = pcfg->img_width * TEST_RGB_MALLOC_LINE * rgb_get_format_byte(pcfg->format_in);
    VIDEO_LOG("malloc size=0x%x byte, tiny_mem_perused=%d", image_size_byte, tiny_mem_perused());
    image_buf = tiny_malloc(image_size_byte);
    CHECK_POINT_NOT_NULL(image_buf);
    VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, image_size_byte);
    image_fillcolor(image_buf, pcfg->img_width, TEST_RGB_MALLOC_LINE, pcfg->format_in);
#endif

    lcd_rgb_pinmux();
    lcd_rgb_reset();
    lcd_rgb_bl_enable();

    ret = rgb_gpdma_init(TEST_RGB_GPDMA_CH);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    ret = rgb_init(pcfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    gpdma_cnt = rgb_gpdma_get_cnt();
    ret = rgb_gpdma_start(TEST_RGB_GPDMA_CH, image_buf, image_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    rgb_reg_dump();
    gpdma_reg_dump(TEST_RGB_GPDMA_CH);
    VIDEO_LOG("[%s:%d] case_id=0x%04x", __func__, __LINE__, id);

    ret = rgb_start();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    while(1)
    {
        DELAY_MS(1);
        times++;
        timeout++;

        if(gpdma_cnt != rgb_gpdma_get_cnt())
        {
            gpdma_cnt = rgb_gpdma_get_cnt();
            timeout = 0;
            VIDEO_LOG("gpdma_cnt=%d", gpdma_cnt);
        }

        if(timeout >= 100)
        {
            VIDEO_LOG("[%s:%d] timeout", __func__, __LINE__);
            ret = FAILURE;
            goto error;
        }

//        if(times >= 5000)
//        {
//            VIDEO_LOG("[%s:%d] test finish", __func__, __LINE__);
//            ret = SUCCESS;
//            break;
//        }
    }

error:
    ap_cfg_reg_dump();
    rgb_reg_dump();
    gpdma_reg_dump(TEST_RGB_GPDMA_CH);

    prgb_irq_cnt = rgb_irq_cnt_get();
    VIDEO_LOG("[%s:%d] sof_cnt=%d", __func__, __LINE__, prgb_irq_cnt->sof);
    VIDEO_LOG("[%s:%d] eof_cnt=%d", __func__, __LINE__, prgb_irq_cnt->eof);
    VIDEO_LOG("[%s:%d] read_empty_cnt=%d", __func__, __LINE__, prgb_irq_cnt->fifo_read_empty);
    VIDEO_LOG("[%s:%d] read_full_cnt=%d", __func__, __LINE__, prgb_irq_cnt->fifo_read_full);
    VIDEO_LOG("[%s:%d] write_empty_cnt=%d", __func__, __LINE__, prgb_irq_cnt->fifo_write_empty);
    VIDEO_LOG("[%s:%d] write_full_cnt=%d", __func__, __LINE__, prgb_irq_cnt->fifo_write_full);

    ret = rgb_stop();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    ret = rgb_deinit();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    ret = rgb_gpdma_stop(TEST_RGB_GPDMA_CH);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

#ifndef TEST_RGB_PSRAM_ADDR
    tiny_free(image_buf);
#endif

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] case_id=0x%04x SUCCESS", __func__, __LINE__, id);
    } else {
        VIDEO_LOG("[%s:%d] case_id=0x%04x FAILED", __func__, __LINE__, id);
    }
    return ret;
}


/* test rgb : clk div */
static int32_t test_rgb_clk_out(void)
{
    int32_t ret = FAILURE;

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    rgb_reset();
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  6, 31);  // GPIOA_06   CLK

    //RGB_EnableClockout(1600000);
    //RGB_EnableClockout(2000000);
    //RGB_EnableClockout(2400000);
    //RGB_EnableClockout(4000000);
    //RGB_EnableClockout(4800000);
    //RGB_EnableClockout(6000000);
    //RGB_EnableClockout(8000000);
    //RGB_EnableClockout(12000000);
    //RGB_EnableClockout(24000000);
    RGB_EnableClockout(24600000);
    //RGB_EnableClockout(100000000);

    while(1);

    return ret;
}


/* test rgb : clk enable and reset */
static int32_t test_rgb_reset(void)
{
    int32_t ret = FAILURE;

    RGB_InitTypeDef rgb_cfg = {
        .frms               = RGB_FRAME_CONTINUE,
        .wires              = RGB_OUTPUT_WIRES_8,
        .sync               = RGB_SYNC_MODE_SYNC_DE,
        .de_continue        = true,
        .format_in          = RGB_INPUT_FORMAT_RGB565,
        .format_out         = RGB_OUTPUT_FORMAT_BGR565,
        .out_lsb            = true,
        .VSPolarity         = RGB_POLARITY_NEGATIVE,
        .HSPolarity         = RGB_POLARITY_NEGATIVE,
        .DEPolarity         = RGB_POLARITY_NEGATIVE,
        .CLKPolarity        = RGB_POLARITY_NEGATIVE,
        .clk_hz             = TEST_RGB_OUT_CLK_HZ,
        .img_width          = 1280,
        .img_height         = 800,
        .v_pulse_width      = 10,
        .h_pulse_width      = 10,
        .v_front_blanking   = 15,
        .h_front_blanking   = 72,
        .v_back_blanking    = 23,
        .h_back_blanking    = 88,
    };

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    rgb_reset();
    rgb_init(&rgb_cfg);
    rgb_start();
    rgb_reg_dump();
    VIDEO_LOG("[%s:%d]\r\n", __func__, __LINE__);

    CHECK_RET_EQ_EXIT(IP_RGB->REG_RGB_CONTROL0.all,         0x00010011, error);
    CHECK_RET_EQ_EXIT(IP_RGB->REG_RGB_CONTROL1.all,         0x000005a0, error);
    CHECK_RET_EQ_EXIT(IP_RGB->REG_SEQUENTIAL_CONTROL0.all,  0x7faa1758, error);
    CHECK_RET_EQ_EXIT(IP_RGB->REG_SEQUENTIAL_CONTROL1.all,  0x00000f48, error);
    CHECK_RET_EQ_EXIT(IP_RGB->REG_IMAGE_SIZE.all,           0x03200500, error);
    CHECK_RET_EQ_EXIT(IP_RGB->REG_RGB_IRQ.all,              0x00000000, error);
    //CHECK_RET_EQ_EXIT(IP_RGB->REG_RGB_INTR_MSK.all,         0x0000003c, error);
    //CHECK_RET_EQ_EXIT(IP_RGB->REG_RGB_INTR_CLR.all,         0x00000000, error);
    //CHECK_RET_EQ_EXIT(IP_RGB->REG_RGB_INTR_STATUS.all,      0x00000000, error);
    //CHECK_RET_EQ_EXIT(IP_RGB->REG_RGB_INTR_RAW.all,         0x00000014, error);

    rgb_reset();
    rgb_reg_dump();
    VIDEO_LOG("[%s:%d]\r\n", __func__, __LINE__);

    CHECK_RET_EQ_EXIT(IP_RGB->REG_RGB_CONTROL0.all,         0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_RGB->REG_RGB_CONTROL1.all,         0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_RGB->REG_SEQUENTIAL_CONTROL0.all,  0x00440c2b, error);
    CHECK_RET_EQ_EXIT(IP_RGB->REG_SEQUENTIAL_CONTROL1.all,  0x00000c2b, error);
    CHECK_RET_EQ_EXIT(IP_RGB->REG_IMAGE_SIZE.all,           0x002b002b, error);
    CHECK_RET_EQ_EXIT(IP_RGB->REG_RGB_IRQ.all,              0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_RGB->REG_RGB_INTR_MSK.all,         0x0000003f, error);
    CHECK_RET_EQ_EXIT(IP_RGB->REG_RGB_INTR_CLR.all,         0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_RGB->REG_RGB_INTR_STATUS.all,      0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_RGB->REG_RGB_INTR_RAW.all,         0x00000014, error);

    ret = SUCCESS;

error:
    rgb_reset();

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static uint8_t rgb_get_format_byte(rgb_emFormatIn format)
{
    switch(format)
    {
        case RGB_INPUT_FORMAT_RGB565:
            return 2;

        case RGB_INPUT_FORMAT_RGB888:
            return 3;

        case RGB_INPUT_FORMAT_XRGB8888:
            return 4;

        default:
            return 0;
    }
}

static void get_rgb_cfg(uint16_t id, RGB_InitTypeDef **pcfg)
{
    uint32_t i = 0;

    for(i = 0; i < (sizeof(rgb_case_tab) / sizeof(rgb_CaseTypeDef)); i++)
    {
        if(id == rgb_case_tab[i].id)
        {
            *pcfg = rgb_case_tab[i].pcfg;
            return;
        }
    }
    VIDEO_LOG("[%s:%d] error id=0x%x ", __func__, __LINE__, id);
}


static void image_fillcolor(uint8_t *image_buf, uint16_t pixel_width, uint16_t pixel_height, rgb_emFormatIn format)
{
    switch(format)
    {
        case RGB_INPUT_FORMAT_RGB888:
            rgb888_colorbar_create(image_buf, pixel_width, pixel_height, pixel_height/5);
            rgb888_grid_create(image_buf, pixel_width, pixel_height, pixel_height);
            break;

        case RGB_INPUT_FORMAT_XRGB8888:
            argb8888_colorbar_create(image_buf, pixel_width, pixel_height, pixel_height/5);
            argb8888_grid_create(image_buf, pixel_width, pixel_height, pixel_height);
            break;

        case RGB_INPUT_FORMAT_RGB565:
            rgb565_colorbar_create((uint16_t *)image_buf, pixel_width, pixel_height, pixel_height/5);
            rgb565_grid_create((uint16_t *)image_buf, pixel_width, pixel_height, pixel_height);
            break;

        default:
            VIDEO_LOG("[%s:%d] format=%d is error", __func__, __LINE__, format);
            break;
    }
}


static void test_rgb_reg_ctrl(void)
{
    uint32_t times = 0;
    uint32_t image_size_byte = 32*32*3;
    uint8_t image_tab[32*32*3] = {0};
    uint8_t *image_buf = NULL;

    //VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    image_buf = image_tab;
    times = image_size_byte;
    while(times--)
    {
        *image_buf++ = 0xFF;
    }
    image_buf = image_tab;

    lcd_rgb_pinmux();
    lcd_rgb_reset();

    /* CLK ENABLE */
    IP_AP_CFG->REG_CLK_CFG0.all = 0xFFFFFFFF;
    IP_AP_CFG->REG_CLK_CFG1.all = 0xFFFFFFFF;
    IP_AP_CFG->REG_DMA_SEL.all = 0x0;      // 1:qspi  0:rgb
    IP_AP_CFG->REG_CLK_CFG0.all &= ~(1<<24);    // 0:24MHz  1:syspll_peri_clk
    RGB_EnableClockout(24000000);

    /* GPDMA INIT*/
    //mmio_write32(0x45900004, 0x3c491);
    mmio_write32(0x45900004, 0x3E491);      // auto
    mmio_write32(0x45900064, (uint32_t)image_buf);
    mmio_write32(0x4590006c, 0x4500B0F4);
    mmio_write32(0x45900030, image_size_byte / sizeof(uint32_t));
    mmio_write32(0x45900278, image_size_byte / sizeof(uint32_t));      // auto
    mmio_write32(0x45900004, 0x3E493);  // GPDMA start

    gpdma_reg_dump(gp_dma_ch1);
    rgb_reg_dump();

    /* RGB INIT */
    mmio_write32(0x4500B010, 0x00200020);
    mmio_write32(0x4500B000, 0x00000111);
    //while(1);

    times = 0;
    while(1)
    {
        DELAY_MS(1);
        times++;

        if((times % 2000) == 0)
        {
            rgb_reg_dump();
            VIDEO_LOG("times=%d", __func__, __LINE__, times);
        }
    }

    while(0)
    {
        DELAY_MS(1);
        times++;
        VIDEO_LOG("[%s:%d] times=%d\r\n", __func__, __LINE__, times);
        mmio_write32(0x4500B0F4, 0x5555aaaa);
    }
}


static void test_rgb_lcd_vs_hs_color(uint32_t cnt)
{
    void *gGpioADev = GPIOA();
    void *gGpioBDev = GPIOB();

    if(cnt == 0)        // red
    {
        GPIO_PinWrite(gGpioBDev, (1UL <<  0), 1);   // GPIOB_00   R0
        GPIO_PinWrite(gGpioBDev, (1UL <<  1), 1);   // GPIOB_01   R1
        GPIO_PinWrite(gGpioADev, (1UL <<  8), 1);   // GPIOA_08   R2
        GPIO_PinWrite(gGpioADev, (1UL <<  9), 1);   // GPIOA_09   R3
        GPIO_PinWrite(gGpioADev, (1UL << 10), 1);   // GPIOA_10   R4
        GPIO_PinWrite(gGpioADev, (1UL << 11), 1);   // GPIOA_11   R5
        GPIO_PinWrite(gGpioADev, (1UL << 12), 1);   // GPIOA_12   R6
        GPIO_PinWrite(gGpioADev, (1UL << 13), 1);   // GPIOA_13   R7
        GPIO_PinWrite(gGpioBDev, (1UL <<  2), 0);   // GPIOB_02   G0
        GPIO_PinWrite(gGpioBDev, (1UL <<  3), 0);   // GPIOB_03   G1
        GPIO_PinWrite(gGpioADev, (1UL << 14), 0);   // GPIOA_14   G2
        GPIO_PinWrite(gGpioADev, (1UL << 15), 0);   // GPIOA_15   G3
        GPIO_PinWrite(gGpioADev, (1UL << 16), 0);   // GPIOA_16   G4
        GPIO_PinWrite(gGpioADev, (1UL << 17), 0);   // GPIOA_17   G5
        GPIO_PinWrite(gGpioADev, (1UL << 18), 0);   // GPIOA_18   G6
        GPIO_PinWrite(gGpioADev, (1UL << 19), 0);   // GPIOA_19   G7
        GPIO_PinWrite(gGpioBDev, (1UL <<  4), 0);   // GPIOB_04   B0
        GPIO_PinWrite(gGpioBDev, (1UL <<  5), 0);   // GPIOB_05   B1
        GPIO_PinWrite(gGpioADev, (1UL << 20), 0);   // GPIOA_20   B2
        GPIO_PinWrite(gGpioADev, (1UL << 21), 0);   // GPIOA_21   B3
        GPIO_PinWrite(gGpioADev, (1UL << 22), 0);   // GPIOA_22   B4
        GPIO_PinWrite(gGpioADev, (1UL << 23), 0);   // GPIOA_23   B5
        GPIO_PinWrite(gGpioADev, (1UL << 24), 0);   // GPIOA_24   B6
        GPIO_PinWrite(gGpioADev, (1UL << 25), 0);   // GPIOA_25   B7
    }
    else if(cnt == 100) // green
    {
        GPIO_PinWrite(gGpioBDev, (1UL <<  0), 0);   // GPIOB_00   R0
        GPIO_PinWrite(gGpioBDev, (1UL <<  1), 0);   // GPIOB_01   R1
        GPIO_PinWrite(gGpioADev, (1UL <<  8), 0);   // GPIOA_08   R2
        GPIO_PinWrite(gGpioADev, (1UL <<  9), 0);   // GPIOA_09   R3
        GPIO_PinWrite(gGpioADev, (1UL << 10), 0);   // GPIOA_10   R4
        GPIO_PinWrite(gGpioADev, (1UL << 11), 0);   // GPIOA_11   R5
        GPIO_PinWrite(gGpioADev, (1UL << 12), 0);   // GPIOA_12   R6
        GPIO_PinWrite(gGpioADev, (1UL << 13), 0);   // GPIOA_13   R7
        GPIO_PinWrite(gGpioBDev, (1UL <<  2), 1);   // GPIOB_02   G0
        GPIO_PinWrite(gGpioBDev, (1UL <<  3), 1);   // GPIOB_03   G1
        GPIO_PinWrite(gGpioADev, (1UL << 14), 1);   // GPIOA_14   G2
        GPIO_PinWrite(gGpioADev, (1UL << 15), 1);   // GPIOA_15   G3
        GPIO_PinWrite(gGpioADev, (1UL << 16), 1);   // GPIOA_16   G4
        GPIO_PinWrite(gGpioADev, (1UL << 17), 1);   // GPIOA_17   G5
        GPIO_PinWrite(gGpioADev, (1UL << 18), 1);   // GPIOA_18   G6
        GPIO_PinWrite(gGpioADev, (1UL << 19), 1);   // GPIOA_19   G7
        GPIO_PinWrite(gGpioBDev, (1UL <<  4), 0);   // GPIOB_04   B0
        GPIO_PinWrite(gGpioBDev, (1UL <<  5), 0);   // GPIOB_05   B1
        GPIO_PinWrite(gGpioADev, (1UL << 20), 0);   // GPIOA_20   B2
        GPIO_PinWrite(gGpioADev, (1UL << 21), 0);   // GPIOA_21   B3
        GPIO_PinWrite(gGpioADev, (1UL << 22), 0);   // GPIOA_22   B4
        GPIO_PinWrite(gGpioADev, (1UL << 23), 0);   // GPIOA_23   B5
        GPIO_PinWrite(gGpioADev, (1UL << 24), 0);   // GPIOA_24   B6
        GPIO_PinWrite(gGpioADev, (1UL << 25), 0);   // GPIOA_25   B7
    }
    else if(cnt == 200) // blue
    {
        GPIO_PinWrite(gGpioBDev, (1UL <<  0), 0);   // GPIOB_00   R0
        GPIO_PinWrite(gGpioBDev, (1UL <<  1), 0);   // GPIOB_01   R1
        GPIO_PinWrite(gGpioADev, (1UL <<  8), 0);   // GPIOA_08   R2
        GPIO_PinWrite(gGpioADev, (1UL <<  9), 0);   // GPIOA_09   R3
        GPIO_PinWrite(gGpioADev, (1UL << 10), 0);   // GPIOA_10   R4
        GPIO_PinWrite(gGpioADev, (1UL << 11), 0);   // GPIOA_11   R5
        GPIO_PinWrite(gGpioADev, (1UL << 12), 0);   // GPIOA_12   R6
        GPIO_PinWrite(gGpioADev, (1UL << 13), 0);   // GPIOA_13   R7
        GPIO_PinWrite(gGpioBDev, (1UL <<  2), 0);   // GPIOB_02   G0
        GPIO_PinWrite(gGpioBDev, (1UL <<  3), 0);   // GPIOB_03   G1
        GPIO_PinWrite(gGpioADev, (1UL << 14), 0);   // GPIOA_14   G2
        GPIO_PinWrite(gGpioADev, (1UL << 15), 0);   // GPIOA_15   G3
        GPIO_PinWrite(gGpioADev, (1UL << 16), 0);   // GPIOA_16   G4
        GPIO_PinWrite(gGpioADev, (1UL << 17), 0);   // GPIOA_17   G5
        GPIO_PinWrite(gGpioADev, (1UL << 18), 0);   // GPIOA_18   G6
        GPIO_PinWrite(gGpioADev, (1UL << 19), 0);   // GPIOA_19   G7
        GPIO_PinWrite(gGpioBDev, (1UL <<  4), 1);   // GPIOB_04   B0
        GPIO_PinWrite(gGpioBDev, (1UL <<  5), 1);   // GPIOB_05   B1
        GPIO_PinWrite(gGpioADev, (1UL << 20), 1);   // GPIOA_20   B2
        GPIO_PinWrite(gGpioADev, (1UL << 21), 1);   // GPIOA_21   B3
        GPIO_PinWrite(gGpioADev, (1UL << 22), 1);   // GPIOA_22   B4
        GPIO_PinWrite(gGpioADev, (1UL << 23), 1);   // GPIOA_23   B5
        GPIO_PinWrite(gGpioADev, (1UL << 24), 1);   // GPIOA_24   B6
        GPIO_PinWrite(gGpioADev, (1UL << 25), 1);   // GPIOA_25   B7
    }
    else if(cnt == 300) // red and green
    {
        GPIO_PinWrite(gGpioBDev, (1UL <<  0), 1);   // GPIOB_00   R0
        GPIO_PinWrite(gGpioBDev, (1UL <<  1), 1);   // GPIOB_01   R1
        GPIO_PinWrite(gGpioADev, (1UL <<  8), 1);   // GPIOA_08   R2
        GPIO_PinWrite(gGpioADev, (1UL <<  9), 1);   // GPIOA_09   R3
        GPIO_PinWrite(gGpioADev, (1UL << 10), 1);   // GPIOA_10   R4
        GPIO_PinWrite(gGpioADev, (1UL << 11), 1);   // GPIOA_11   R5
        GPIO_PinWrite(gGpioADev, (1UL << 12), 1);   // GPIOA_12   R6
        GPIO_PinWrite(gGpioADev, (1UL << 13), 1);   // GPIOA_13   R7
        GPIO_PinWrite(gGpioBDev, (1UL <<  2), 1);   // GPIOB_02   G0
        GPIO_PinWrite(gGpioBDev, (1UL <<  3), 1);   // GPIOB_03   G1
        GPIO_PinWrite(gGpioADev, (1UL << 14), 1);   // GPIOA_14   G2
        GPIO_PinWrite(gGpioADev, (1UL << 15), 1);   // GPIOA_15   G3
        GPIO_PinWrite(gGpioADev, (1UL << 16), 1);   // GPIOA_16   G4
        GPIO_PinWrite(gGpioADev, (1UL << 17), 1);   // GPIOA_17   G5
        GPIO_PinWrite(gGpioADev, (1UL << 18), 1);   // GPIOA_18   G6
        GPIO_PinWrite(gGpioADev, (1UL << 19), 1);   // GPIOA_19   G7
        GPIO_PinWrite(gGpioBDev, (1UL <<  4), 0);   // GPIOB_04   B0
        GPIO_PinWrite(gGpioBDev, (1UL <<  5), 0);   // GPIOB_05   B1
        GPIO_PinWrite(gGpioADev, (1UL << 20), 0);   // GPIOA_20   B2
        GPIO_PinWrite(gGpioADev, (1UL << 21), 0);   // GPIOA_21   B3
        GPIO_PinWrite(gGpioADev, (1UL << 22), 0);   // GPIOA_22   B4
        GPIO_PinWrite(gGpioADev, (1UL << 23), 0);   // GPIOA_23   B5
        GPIO_PinWrite(gGpioADev, (1UL << 24), 0);   // GPIOA_24   B6
        GPIO_PinWrite(gGpioADev, (1UL << 25), 0);   // GPIOA_25   B7
    }
    else
    {

    }
}

#define LCD_CLK_HZ          (6000000)
#define LCD_1000CLK_DELAY   (1000000000 / LCD_CLK_HZ * 133 / 236)
#define LCD_CLK_CFG         (LCD_CLK_HZ * 35 / 6)

#define SW_RGB_VS_HIGH()    GPIO_PinWrite(gGpioADev, (1UL << 5), 1)
#define SW_RGB_VS_LOW()     GPIO_PinWrite(gGpioADev, (1UL << 5), 0)
#define SW_RGB_HS_HIGH()    GPIO_PinWrite(gGpioADev, (1UL << 4), 1)
#define SW_RGB_HS_LOW()     GPIO_PinWrite(gGpioADev, (1UL << 4), 0)
#define SW_RGB_DE_HIGH()    GPIO_PinWrite(gGpioADev, (1UL << 7), 1)
#define SW_RGB_DE_LOW()     GPIO_PinWrite(gGpioADev, (1UL << 7), 0)
#define SW_RGB_CLK_HIGH()   GPIO_PinWrite(gGpioADev, (1UL << 6), 1)
#define SW_RGB_CLK_LOW()    GPIO_PinWrite(gGpioADev, (1UL << 6), 0)

static void test_rgb_lcd_gpio(void)
{
    uint32_t hs_cnt = 0;
    uint32_t vs_cnt = 0;
    uint32_t clk_cnt = 0;
    void *gGpioADev = NULL;
    void *gGpioBDev = NULL;

    /* GPIO pinmux */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  4, 0);  // GPIOA_04   HS
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  5, 0);  // GPIOA_05   VS
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  6, 0);  // GPIOA_06   CLK
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  7, 0);  // GPIOA_07   DE
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  0, 0);  // GPIOB_00   R0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  1, 0);  // GPIOB_01   R1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  8, 1);  // GPIOA_08   R2
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  9, 1);  // GPIOA_09   R3
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 10, 0);  // GPIOA_10   R4
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11, 0);  // GPIOA_11   R5
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 12, 0);  // GPIOA_12   R6
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 13, 0);  // GPIOA_13   R7
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  2, 0);  // GPIOB_02   G0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  3, 0);  // GPIOB_03   G1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 14, 0);  // GPIOA_14   G2
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 15, 0);  // GPIOA_15   G3
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 16, 0);  // GPIOA_16   G4
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 17, 0);  // GPIOA_17   G5
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 18, 0);  // GPIOA_18   G6
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 19, 0);  // GPIOA_19   G7
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  4, 0);  // GPIOB_04   B0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  5, 0);  // GPIOB_05   B1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, 0);  // GPIOA_20   B2
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, 0);  // GPIOA_21   B3
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22, 0);  // GPIOA_22   B4
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 23, 0);  // GPIOA_23   B5
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 24, 0);  // GPIOA_24   B6
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 25, 0);  // GPIOA_25   B7

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 27, 0);  // GPIOA_27   LCD_BL
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 30, 0);  // GPIOA_30   RESET

    /* GPIO config */
    gGpioADev = GPIOA();
    gGpioBDev = GPIOB();
    GPIO_Initialize(gGpioADev, NULL, NULL);
    GPIO_Initialize(gGpioBDev, NULL, NULL);
    GPIO_SetDir(gGpioADev, 0x4BFFFFF0, CSK_GPIO_DIR_OUTPUT);
    GPIO_SetDir(gGpioBDev, 0x0000003F, CSK_GPIO_DIR_OUTPUT);
    SW_RGB_CLK_LOW();
    SW_RGB_VS_LOW();
    SW_RGB_HS_LOW();
    SW_RGB_DE_LOW();

    GPIO_PinWrite(gGpioADev, (1UL << 27), 1);
    GPIO_PinWrite(gGpioADev, (1UL << 30), 0);   // RESET
    DELAY_MS(100);
    GPIO_PinWrite(gGpioADev, (1UL << 30), 1);
    DELAY_MS(100);

    VIDEO_LOG("[%s:%d] test GPIO for RGB-LCD VS HS", __func__, __LINE__);

    while(0)
    {
        GPIO_PinWrite(gGpioADev, (1UL << 6), 1);
        GPIO_PinWrite(gGpioADev, (1UL << 19), 1);
        GPIO_PinWrite(gGpioADev, (1UL << 25), 1);
        DELAY_MS(10);
        GPIO_PinWrite(gGpioADev, (1UL << 6), 0);
        GPIO_PinWrite(gGpioADev, (1UL << 19), 0);
        GPIO_PinWrite(gGpioADev, (1UL << 25), 0);
        DELAY_MS(10);
        clk_cnt++;
        VIDEO_LOG("[%s:%d] clk_cnt=%d", __func__, __LINE__, clk_cnt);
    }

    while(0)
    {
        SW_RGB_VS_HIGH();
        SW_RGB_HS_HIGH();
        SW_RGB_DE_HIGH();
        SW_RGB_CLK_HIGH();
        //GPIO_PinWrite(gGpioADev, (1UL << 30), 1);
        //GPIO_PinWrite(gGpioADev, (1UL << 27), 1);
        test_rgb_lcd_vs_hs_color(0);
        DELAY_US(1000);

        SW_RGB_VS_LOW();
        SW_RGB_HS_LOW();
        SW_RGB_DE_LOW();
        SW_RGB_CLK_LOW();
        //GPIO_PinWrite(gGpioADev, (1UL << 30), 0);   // RESET
        //GPIO_PinWrite(gGpioADev, (1UL << 27), 0);
        test_rgb_lcd_vs_hs_color(100);
        DELAY_US(1000);

        test_rgb_lcd_vs_hs_color(200);
        DELAY_US(1000);

        clk_cnt++;
        VIDEO_LOG("[%s:%d] clk_cnt=%d", __func__, __LINE__, clk_cnt);
    }


    dvp_reset();
    ap_cfg_gpdma_clk_enable();
    ap_cfg_video_clk_enable();
    ap_cfg_vic_clk_enable();
    ap_cfg_qspi0_clk_enable();
    ap_cfg_qspi0_clk_sel();
    ap_cfg_qspi0_clk_div_m(2);
    ap_cfg_qspi0_clk_div_n(1);
    ap_cfg_qspi0_clk_inv();
    ap_cfg_vic_reset();
    ap_cfg_qspi0_reset();

    /* VIC MCLK enable */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 26, CSK_IOMUX_FUNC_ALTER16);
    DVP_EnableClockout(DVP0(), 1000000);  // FPGA 10MHz->1.6MHz   70MHz->12MHz  35MHz->6MHz

    clk_cnt = 0;
    while(1)
    {

        SW_RGB_CLK_LOW();
        DELAY_US(100);
        SW_RGB_CLK_HIGH();
        DELAY_US(100);
        clk_cnt++;

        if(clk_cnt == 1056)
        {
            clk_cnt = 0;
            hs_cnt++;

            if(hs_cnt == 22)
            {
                SW_RGB_VS_HIGH();
                VIDEO_LOG("[%s:%d] frame %d", __func__, __LINE__, vs_cnt);
            }
        }

        if(hs_cnt == 525)
        {
            hs_cnt = 0;
            vs_cnt++;

            SW_RGB_VS_LOW();
            VIDEO_LOG("[%s:%d] frame %d", __func__, __LINE__, vs_cnt);
        }

        test_rgb_lcd_vs_hs_color(hs_cnt);
    }
}


