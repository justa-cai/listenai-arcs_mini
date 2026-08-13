#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <FreeRTOS.h>
#include <semphr.h>

#include "venusa_ap.h"
#include "cache.h"
#include "log_print.h"
#include "systick.h"
#include "ClockManager.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "Driver_DMA2D.h"
#include "Driver_RGB.h"

#define VIDEO_LOG   CLOGD
#define DELAY_MS(x) SysTick_Delay_Ms(x)
#define DELAY_US(x) SysTick_Delay_Us(x)

#define TEST_RGB_GPDMA_CH   dma_2d_ch0

#define CHECK_RET_EQ_EXIT(Ret, express, errExit)\
do{\
    if ((express) != (Ret))\
    {\
        VIDEO_LOG("ret %d not equal with %d failed at %s: LINE: %d", (Ret), (express), __FUNCTION__, __LINE__);\
        goto errExit;\
    }\
}while(0)

static SemaphoreHandle_t rgbSemaphore;
static void* auto_pbuf = NULL;
static uint32_t auto_size_byte = 0;

static void rgb_gpdma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);

    if (rgbSemaphore != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(rgbSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }

    if(auto_pbuf != NULL) {
        DMA2D_Start_Normal(TEST_RGB_GPDMA_CH, auto_pbuf, (void *)RGB0_Buf(), auto_size_byte);
    }
}

void test_rgb_buf_update(uint8_t *image_buf)
{
    auto_pbuf = image_buf;
}

static void rgb_callback(RGB_emIrqEvent event, uint32_t param)
{
    switch(event)
    {
        case RGB_IRQ_EVENT_SOF:
            //VIDEO_LOG("[%s:%d] RGB SOF event: %d", __func__, __LINE__, event);
            break;

        case RGB_IRQ_EVENT_EOF:
            //VIDEO_LOG("[%s:%d] RGB EOF event: %d", __func__, __LINE__, event);
            break;

        case RGB_IRQ_EVENT_FIFO_RD_EMPTY:
            VIDEO_LOG("[%s:%d] RGB_IRQ_EVENT_FIFO_RD_EMPTY", __func__, __LINE__);
            break;

        case RGB_IRQ_EVENT_FIFO_RD_FULL:
            VIDEO_LOG("[%s:%d] RGB_IRQ_EVENT_FIFO_RD_FULL", __func__, __LINE__);
            break;

        case RGB_IRQ_EVENT_FIFO_WR_EMPTY:
            VIDEO_LOG("[%s:%d] RGB_IRQ_EVENT_FIFO_WR_EMPTY", __func__, __LINE__);
            break;

        case RGB_IRQ_EVENT_FIFO_WR_FULL:
            VIDEO_LOG("[%s:%d] RGB_IRQ_EVENT_FIFO_WR_FULL", __func__, __LINE__);
            break;

        default:
            VIDEO_LOG("[%s:%d] RGB error event: %d", __func__, __LINE__, event);
            break;
    }

    return;
}

int32_t test_rgb_init(uint8_t *image_buf, SemaphoreHandle_t Semaphore)
{
    int32_t ret = 0;
    uint32_t times = 0;
    uint32_t image_size_byte = (800 * 480 * 2);  // RGB565
    RGB_InitTypeDef rgb_cfg = {
        .frms               = RGB_FRAME_CONTINUE,   // RGB_FRAME_CONTINUE  RGB_FRAME_ONCE
        .wires              = RGB_OUTPUT_WIRES_24,
        .sync               = RGB_SYNC_MODE_SYNC_DE,
        .de_continue        = false,
        .format_in          = RGB_INPUT_FORMAT_RGB565,
        .format_out         = RGB_OUTPUT_FORMAT_RGB565,
        .out_lsb            = false,
        .VSPolarity         = RGB_POLARITY_POSITIVE,
        .HSPolarity         = RGB_POLARITY_POSITIVE,
        .DEPolarity         = RGB_POLARITY_NEGATIVE,
        .CLKPolarity        = RGB_POLARITY_NEGATIVE,
        .clk_hz             = 50000000,
        .img_width          = 800,
        .img_height         = 480,
        .v_pulse_width      = 1,
        .h_pulse_width      = 1,
        .v_front_blanking   = 22,
        .h_front_blanking   = 210,
        .v_back_blanking    = 7,
        .h_back_blanking    = 7,
        .BurstThreshold     = 8,
    };
    csk_dma2d_init_t dma2d_para = {
        .dma_ch = TEST_RGB_GPDMA_CH,
        .tfr_mode = tfr_mode_m2p,
        .src_basic_unit = dma2d_sample_unit_word,
        .dst_basic_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_fix,
        .src_burst_len = dma2d_burst_len_8spl,
        .dst_burst_len = dma2d_burst_len_8spl,
        .flow_ctrl = dma2d_flow_ctrl_dma,
        .prio_lvl = prio_mode_vhigh,
        .handshake = rgb_hs_num0,
        .src_gather.enable = csk_func_disable,
        .dst_scatter.enable = csk_func_disable,
        .trigger.mode = csk_trigger_null,
        .trigger.triggered_en = csk_func_disable,
        .trigger.triggered_src_chn = dma_2d_ch0
    };

    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    rgbSemaphore = Semaphore;

    /* LCD RGB pinmux */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  2, CSK_IOMUX_FUNC_ALTER21);  // CLK
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_ALTER21);  // VS
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 19, CSK_IOMUX_FUNC_ALTER21);  // HS
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 30, CSK_IOMUX_FUNC_ALTER21);  // DE
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 23, CSK_IOMUX_FUNC_ALTER21);  // R0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22, CSK_IOMUX_FUNC_ALTER21);  // R1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_ALTER21);  // R2
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  7, CSK_IOMUX_FUNC_ALTER21);  // R3
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  6, CSK_IOMUX_FUNC_ALTER21);  // R4
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  5, CSK_IOMUX_FUNC_ALTER21);  // R5
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  4, CSK_IOMUX_FUNC_ALTER21);  // R6
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  3, CSK_IOMUX_FUNC_ALTER21);  // R7
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 25, CSK_IOMUX_FUNC_ALTER21);  // G0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 24, CSK_IOMUX_FUNC_ALTER21);  // G1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 13, CSK_IOMUX_FUNC_ALTER21);  // G2
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 12, CSK_IOMUX_FUNC_ALTER21);  // G3
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11, CSK_IOMUX_FUNC_ALTER21);  // G4
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 10, CSK_IOMUX_FUNC_ALTER21);  // G5
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  9, CSK_IOMUX_FUNC_ALTER21);  // G6
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  8, CSK_IOMUX_FUNC_ALTER21);  // G7
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 28, CSK_IOMUX_FUNC_ALTER21);  // B0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 27, CSK_IOMUX_FUNC_ALTER21);  // B1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 26, CSK_IOMUX_FUNC_ALTER21);  // B2
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 18, CSK_IOMUX_FUNC_ALTER21);  // B3
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 17, CSK_IOMUX_FUNC_ALTER21);  // B4
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 16, CSK_IOMUX_FUNC_ALTER21);  // B5
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 15, CSK_IOMUX_FUNC_ALTER21);  // B6
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 14, CSK_IOMUX_FUNC_ALTER21);  // B7

    /* LCD reset */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 29, CSK_IOMUX_FUNC_DEFAULT);
    GPIO_SetDir(GPIOA(), (1 << 29), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(GPIOA(), (1 << 29), 0);
    DELAY_MS(100);
    GPIO_PinWrite(GPIOA(), (1 << 29), 1);
    DELAY_MS(100);

    /* LCD BL */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 31, CSK_IOMUX_FUNC_DEFAULT);
    GPIO_SetDir(GPIOA(), (1 << 31), CSK_GPIO_DIR_OUTPUT);
    times = 10;
    do {
        GPIO_PinWrite(GPIOA(), (1 << 31), 0);
        DELAY_US(5);
        GPIO_PinWrite(GPIOA(), (1 << 31), 1);
        DELAY_US(5);
    } while(times--);

    ret = RGB_Initialize(RGB0(), rgb_callback, &rgb_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    ret = RGB_EnableClockout(rgb_cfg.clk_hz);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    ret = DMA2D_Config(&dma2d_para, rgb_gpdma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    /* DMA handshake */
    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_00 = 0;
    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_16 = 0;

    ret = DMA2D_Start_Normal(TEST_RGB_GPDMA_CH, image_buf, (void *)RGB0_Buf(), image_size_byte);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
    ret = DMA2D_Start_Normal(TEST_RGB_GPDMA_CH, image_buf, (void *)RGB0_Buf(), image_size_byte);  //shadow
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
    auto_pbuf = image_buf;
    auto_size_byte = image_size_byte;

    ret = RGB_Start(RGB0());
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    VIDEO_LOG("[%s:%d] SUCCESS", __func__, __LINE__);
    return ret;

error:
    ret = RGB_Stop(RGB0());
    ret = RGB_Uninitialize(RGB0());
    ret = RGB_DisableClockout();
    ret = DMA2D_Stop(TEST_RGB_GPDMA_CH);

    //VIDEO_LOG("[%s:%d] FAILED", __func__, __LINE__);
    return ret;
}



/********************************************************************************************************/
/********************************************************************************************************/
/********************************************************************************************************/

#define RGB565_RED         0xF800
#define RGB565_GREEN       0x07E0
#define RGB565_BLUE        0x001F
#define RGB565_YELLOW      0xFFE0
#define RGB565_BRED        0XF81F
#define RGB565_GBLUE       0X07FF
#define RGB565_WHITE       0xFFFF
#define RGB565_BLACK       0x0000

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



