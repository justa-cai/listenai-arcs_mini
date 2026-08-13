#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "venusa_ap.h"
#include "log_print.h"
#include "Driver_DMA2D.h"
#include "nmsis_core.h"
#include "PSRAMManager.h"
#include "Driver_DVP.h"
#include "IOMuxManager.h"


#define DMA2D_TEST_DATA_LENGTH             1024
#define DMA2D_TEST_DATA_CNT                5
#define DMA2D_SUCCESSION_LOOP              50
#define CHECK_RET_EQ(Ret, express)\
    do{\
        if ((express) != (Ret))\
        {\
            CLOGD("ret %d not equal with %d failed at %s: LINE: %d", (Ret), (express), __FUNCTION__, __LINE__);\
        }\
    }while(0)

#define DMA2D_QSPI_OUT_IMAGE_START_X      20     // must be 4N
#define DMA2D_QSPI_OUT_IMAGE_START_Y      60
#define DMA2D_QSPI_OUT_IMAGE_SIZE_W       320     // must be 4N
#define DMA2D_QSPI_OUT_IMAGE_SIZE_H       240

typedef void (*function)(void);


static volatile uint32_t dma2d_image_event = 0;

extern uint8_t enc_yuv444_128x128[49152];
extern uint8_t enc_yuv422_128x128[32768];
extern uint8_t enc_rgb888_128x128[49152];
extern uint8_t enc_rgb565_128x128[32768];
extern uint8_t enc_gray_128x128[16384];
extern uint8_t enc_rgb888_160x120[57600];
extern uint8_t enc_yuv422_160x120[38400];
extern uint8_t enc_yuv422_320x240[153600];
extern uint8_t enc_rgb565_320x240[153600];

extern int32_t qspi_out_task_start(csk_dma2d_ch_t src_chn, csk_dma2d_ch_t dst_chn, uint16_t img_width, uint16_t img_height, csk_image_format_t img_format);
extern int32_t qspi_out_spd2010_image_flush_start(uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h, uint8_t *pbuf, uint32_t size_byte, csk_dma2d_ch_t dma_chn);
extern int32_t qspi_out_spd2010_image_flush_waitdone(void);
extern int32_t qspi_out_spd2010_image_flush_stop(csk_dma2d_ch_t dma_ch);
extern int32_t dvp_in_start(uint16_t width, uint16_t height);


uint32_t dma2d_pixel_bitw(csk_image_format_t format)
{
    uint32_t bitw = 0;

    switch(format)
    {
        case csk_image_format_yuv444_packed:
        case csk_image_format_rgb888:
        case csk_image_format_bgr888:
            bitw = 3;
            break;

        case csk_image_format_yuv422_yuyv_packed:
        case csk_image_format_yuv422_uyvy_packed:
        case csk_image_format_yuv422_yvyu_packed:
        case csk_image_format_yuv422_vyuy_packed:
        case csk_image_format_rgb565:
        case csk_image_format_bgr565:
            bitw = 2;
            break;

        case csk_image_format_y8:
            bitw = 1;
            break;

        default:
            bitw = 1;
            break;
    }

    return bitw;
}

static void dma2d_irq_callback(uint32_t event, void* workspace) 
{
    CLOGD("[%s:%d] event=%d", __FUNCTION__, __LINE__, event);
    dma2d_image_event = event;
    return;
}

static void DMA2D_NormalMode_Test(csk_tfr_mode_t tfr_mode, csk_dma2d_sample_unit_t basic_unit, csk_dma2d_burst_len_t burst_len, uint32_t blk_len)
{
    uint32_t i = 0;
    int32_t ret = CSK_DRIVER_OK;
    csk_dma2d_ch_t chn = dma_2d_ch0;
    uint8_t *dma2d_src_buffer = NULL;
    uint8_t *dma2d_dst_buffer = NULL;

    // Malloc buffer
    if (NULL == (dma2d_src_buffer = malloc(blk_len*2)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }
    dma2d_dst_buffer = dma2d_src_buffer+blk_len;

    for (chn = dma_2d_ch0; chn <= dma_2d_ch5; chn++)
    {
        for (i = 0; i < blk_len; i++)
        {
            *(dma2d_src_buffer + i) = (i*i*(chn+1))&0xFF;
        }
        memset(dma2d_dst_buffer, 0, blk_len);

        csk_dma2d_init_t dma2d_para;
        memset(&dma2d_para, 0, sizeof(dma2d_para));

        dma2d_para.dma_ch = chn;
        dma2d_para.src_basic_unit = basic_unit;
        dma2d_para.dst_basic_unit = dma2d_para.src_basic_unit;
        dma2d_para.src_burst_len = burst_len;
        dma2d_para.dst_burst_len = dma2d_para.src_burst_len;
        dma2d_para.prio_lvl = prio_mode_vhigh;
        dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
        dma2d_para.handshake = hs_none;
        dma2d_para.tfr_mode = tfr_mode;
        if (tfr_mode_m2m == dma2d_para.tfr_mode)
        {
            dma2d_para.src_inc_mode = inc_mode_increase;
            dma2d_para.dst_inc_mode = inc_mode_increase;
        }
        else if (tfr_mode_m2p == dma2d_para.tfr_mode)
        {
            dma2d_para.src_inc_mode = inc_mode_increase;
            dma2d_para.dst_inc_mode = inc_mode_fix;
        }
        else if (tfr_mode_p2m == dma2d_para.tfr_mode)
        {
            dma2d_para.src_inc_mode = inc_mode_fix;
            dma2d_para.dst_inc_mode = inc_mode_increase;
        }

        ret = DMA2D_Initialize();
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        ret = DMA2D_Start_Normal(chn, (void *)dma2d_src_buffer, (void *)dma2d_dst_buffer, blk_len);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        while (!dma2d_image_event);
        dma2d_image_event = 0;

        ret = DMA2D_Stop(chn);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        if (0 != memcmp(dma2d_dst_buffer, dma2d_src_buffer, blk_len))
            CLOGD("[%s][%d]chn=%d source:0x%x -> destination:0x%x copy compare error!!!", __FUNCTION__, __LINE__, chn, dma2d_src_buffer, dma2d_dst_buffer);
        else
            CLOGD("[%s][%d]chn=%d source:0x%x -> destination:0x%x copy compare success!!!", __FUNCTION__, __LINE__, chn, dma2d_src_buffer, dma2d_dst_buffer);

    }

    // Free
    free(dma2d_src_buffer);

    return;
}

static void DMA2D_NormalMode_M2m_Word_Burst1_1K_Test()
{
    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, DMA2D_TEST_DATA_LENGTH);

    return DMA2D_NormalMode_Test(tfr_mode_m2m, dma2d_sample_unit_word, dma2d_burst_len_1spl, DMA2D_TEST_DATA_LENGTH);
}

static void DMA2D_NormalMode_M2m_Word_Burst4_1K_Test()
{
    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, DMA2D_TEST_DATA_LENGTH);

    return DMA2D_NormalMode_Test(tfr_mode_m2m, dma2d_sample_unit_word, dma2d_burst_len_4spl, DMA2D_TEST_DATA_LENGTH);
}

static void DMA2D_NormalMode_M2m_Word_Burst8_1K_Test()
{
    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, DMA2D_TEST_DATA_LENGTH);

    return DMA2D_NormalMode_Test(tfr_mode_m2m, dma2d_sample_unit_word, dma2d_burst_len_8spl, DMA2D_TEST_DATA_LENGTH);
}

static void DMA2D_NormalMode_M2m_Word_Burst16_1K_Test()
{
    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, DMA2D_TEST_DATA_LENGTH);

    return DMA2D_NormalMode_Test(tfr_mode_m2m, dma2d_sample_unit_word, dma2d_burst_len_16spl, DMA2D_TEST_DATA_LENGTH);
}

static void DMA2D_NormalMode_M2m_HalfWord_Burst1_1K_Test()
{
    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, DMA2D_TEST_DATA_LENGTH);

    return DMA2D_NormalMode_Test(tfr_mode_m2m, dma2d_sample_unit_halfword, dma2d_burst_len_1spl, DMA2D_TEST_DATA_LENGTH);
}

static void DMA2D_NormalMode_M2m_HalfWord_Burst4_1K_Test()
{
    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, DMA2D_TEST_DATA_LENGTH);

    return DMA2D_NormalMode_Test(tfr_mode_m2m, dma2d_sample_unit_halfword, dma2d_burst_len_4spl, DMA2D_TEST_DATA_LENGTH);
}

static void DMA2D_NormalMode_M2m_HalfWord_Burst8_1K_Test()
{
    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, DMA2D_TEST_DATA_LENGTH);

    return DMA2D_NormalMode_Test(tfr_mode_m2m, dma2d_sample_unit_halfword, dma2d_burst_len_8spl, DMA2D_TEST_DATA_LENGTH);
}

static void DMA2D_NormalMode_M2m_HalfWord_Burst16_1K_Test()
{
    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, DMA2D_TEST_DATA_LENGTH);

    return DMA2D_NormalMode_Test(tfr_mode_m2m, dma2d_sample_unit_halfword, dma2d_burst_len_16spl, DMA2D_TEST_DATA_LENGTH);
}

static void DMA2D_NormalMode_M2m_Byte_Burst1_1K_Test()
{
    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, DMA2D_TEST_DATA_LENGTH);

    return DMA2D_NormalMode_Test(tfr_mode_m2m, dma2d_sample_unit_byte, dma2d_burst_len_1spl, DMA2D_TEST_DATA_LENGTH);
}

static void DMA2D_NormalMode_M2m_Byte_Burst4_1K_Test()
{
    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, DMA2D_TEST_DATA_LENGTH);

    return DMA2D_NormalMode_Test(tfr_mode_m2m, dma2d_sample_unit_byte, dma2d_burst_len_4spl, DMA2D_TEST_DATA_LENGTH);
}

static void DMA2D_NormalMode_M2m_Byte_Burst8_1K_Test()
{
    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, DMA2D_TEST_DATA_LENGTH);

    return DMA2D_NormalMode_Test(tfr_mode_m2m, dma2d_sample_unit_byte, dma2d_burst_len_8spl, DMA2D_TEST_DATA_LENGTH);
}

static void DMA2D_NormalMode_M2m_Byte_Burst16_1K_Test()
{
    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, DMA2D_TEST_DATA_LENGTH);

    return DMA2D_NormalMode_Test(tfr_mode_m2m, dma2d_sample_unit_byte, dma2d_burst_len_16spl, DMA2D_TEST_DATA_LENGTH);
}

static void DMA2D_NormalMode_M2m_Word_Burst4_16K_Test()
{
    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, DMA2D_TEST_DATA_LENGTH*16);

    return DMA2D_NormalMode_Test(tfr_mode_m2m, dma2d_sample_unit_word, dma2d_burst_len_4spl, DMA2D_TEST_DATA_LENGTH*16);
}

static void DMA2D_NormalMode_M2m_Word_Burst4_32K_Test()
{
    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, DMA2D_TEST_DATA_LENGTH*32);

    return DMA2D_NormalMode_Test(tfr_mode_m2m, dma2d_sample_unit_word, dma2d_burst_len_4spl, DMA2D_TEST_DATA_LENGTH*32);
}

static void DMA2D_NormalMode_M2M_Word_Burst4_1K_Loop_Test()
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t i = 0;
    uint32_t j = 0;
    csk_dma2d_ch_t chn = dma_2d_ch0;
    uint32_t blk_len = DMA2D_TEST_DATA_LENGTH;
    uint8_t *dma2d_src_buffer = NULL;
    uint8_t *dma2d_dst_buffer = NULL;

    // Malloc buffer
    if (NULL == (dma2d_src_buffer = malloc(blk_len*2)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }
    dma2d_dst_buffer = dma2d_src_buffer + blk_len;

    for (chn = dma_2d_ch0; chn <= dma_2d_ch5; chn++)
    {
        csk_dma2d_init_t dma2d_para;
        memset(&dma2d_para, 0, sizeof(dma2d_para));

        dma2d_para.dma_ch = chn;
        dma2d_para.tfr_mode = tfr_mode_m2m;
        dma2d_para.src_basic_unit = dma2d_sample_unit_word;
        dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
        dma2d_para.src_inc_mode = inc_mode_increase;
        dma2d_para.dst_inc_mode = inc_mode_increase;
        dma2d_para.src_burst_len = dma2d_burst_len_4spl;
        dma2d_para.dst_burst_len = dma2d_burst_len_4spl;
        dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
        dma2d_para.prio_lvl = prio_mode_vhigh;
        dma2d_para.handshake = hs_none;

        ret = DMA2D_Initialize();
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        uint8_t test_time = 0;
        for (i = 0; i < DMA2D_SUCCESSION_LOOP; i++)
        {
            for (j = 0; j < blk_len; j++)
            {
                *(dma2d_src_buffer + j) = (j*j*(chn+1))&0xFF;
            }
            memset(dma2d_dst_buffer, 0, blk_len);

            ret = DMA2D_Start_Normal(chn, (void *)dma2d_src_buffer, (void *)dma2d_dst_buffer, blk_len);
            CHECK_RET_EQ(ret, CSK_DRIVER_OK);

            while (!dma2d_image_event);
            dma2d_image_event = 0;

            ret = DMA2D_Stop(chn);
            CHECK_RET_EQ(ret, CSK_DRIVER_OK);

            if (0 != memcmp(dma2d_dst_buffer, dma2d_src_buffer, blk_len))
                CLOGD("[%s][%d]chn=%d source:0x%x -> destination:0x%x copy compare error!!!", __FUNCTION__, __LINE__, chn, dma2d_src_buffer, dma2d_dst_buffer);
            else
                CLOGD("[%s][%d]chn=%d source:0x%x -> destination:0x%x copy compare success,cnt=%d!!!", __FUNCTION__, __LINE__, chn, dma2d_src_buffer, dma2d_dst_buffer, test_time++);
        }
    }
    
    // Free
    free(dma2d_src_buffer);

    return;
}

static void DMA2D_NormalMode_Psram_Test(csk_dma2d_ch_t chn, csk_tfr_mode_t tfr_mode, csk_dma2d_sample_unit_t basic_unit, csk_dma2d_burst_len_t burst_len, uint32_t blk_len, uint8_t *src_buffer, uint8_t *dst_buffer, csk_dma2d_ahb_burst_len_t rd_max_len, csk_dma2d_ahb_burst_len_t wr_max_len)
{
    uint32_t i = 0;
    int32_t ret = CSK_DRIVER_OK;

    for (i = 0; i < blk_len; i++)
    {
        *(src_buffer + i) = (i*i*(chn+1))&0xFF;
    }
    memset(dst_buffer, 0, blk_len);

    csk_dma2d_init_t dma2d_para;
    memset(&dma2d_para, 0, sizeof(dma2d_para));

    dma2d_para.dma_ch = chn;
    dma2d_para.src_basic_unit = basic_unit;
    dma2d_para.dst_basic_unit = dma2d_para.src_basic_unit;
    dma2d_para.src_burst_len = burst_len;
    dma2d_para.dst_burst_len = dma2d_para.src_burst_len;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.handshake = hs_none;
    dma2d_para.tfr_mode = tfr_mode;
    dma2d_para.rd_max_len = rd_max_len;
    dma2d_para.wr_max_len = wr_max_len;
    if (tfr_mode_m2m == dma2d_para.tfr_mode)
    {
        dma2d_para.src_inc_mode = inc_mode_increase;
        dma2d_para.dst_inc_mode = inc_mode_increase;
    }
    else if (tfr_mode_m2p == dma2d_para.tfr_mode)
    {
        dma2d_para.src_inc_mode = inc_mode_increase;
        dma2d_para.dst_inc_mode = inc_mode_fix;
    }
    else if (tfr_mode_p2m == dma2d_para.tfr_mode)
    {
        dma2d_para.src_inc_mode = inc_mode_fix;
        dma2d_para.dst_inc_mode = inc_mode_increase;
    }

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Start_Normal(chn, (void *)src_buffer, (void *)dst_buffer, blk_len);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    while (!dma2d_image_event);
    dma2d_image_event = 0;

    ret = DMA2D_Stop(chn);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    if (0 != memcmp(dst_buffer, src_buffer, blk_len))
        CLOGD("[%s][%d]chn=%d source:0x%x -> destination:0x%x copy compare error!!!", __FUNCTION__, __LINE__, chn, src_buffer, dst_buffer);
    else
        CLOGD("[%s][%d]chn=%d source:0x%x -> destination:0x%x copy compare success!!!", __FUNCTION__, __LINE__, chn, src_buffer, dst_buffer);

    return;
}

static void DMA2D_NormalMode_Sram_To_Psram_Test()
{
    uint8_t *src_buffer = NULL;
    uint8_t *dst_buffer = (uint8_t*)0x38000000;

    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, DMA2D_TEST_DATA_LENGTH);

    // Malloc buffer
    if (NULL == (src_buffer = malloc(DMA2D_TEST_DATA_LENGTH)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }

    uint32_t i = 0;
    csk_dma2d_ch_t chn = dma_2d_ch0;

    for (chn = dma_2d_ch0; chn <= dma_2d_ch5; chn++)
    {
        for (i = 0; i < DMA2D_SUCCESSION_LOOP; i++)
        {
            DMA2D_NormalMode_Psram_Test(chn, tfr_mode_m2m, dma2d_sample_unit_word, dma2d_burst_len_1spl, DMA2D_TEST_DATA_LENGTH, src_buffer, dst_buffer+i, dma2d_ahb_burst_len_default, dma2d_ahb_burst_len_default);
        }
    }

    CLOGD("[%s][%d]copy data end", __FUNCTION__, __LINE__);

    free(src_buffer);

    return;
}

static void DMA2D_NormalMode_Sram_To_Psram_WR_128Byte_Test()
{
    uint8_t *src_buffer = NULL;
    uint8_t *dst_buffer = (uint8_t*)0x38000000;
    uint32_t data_len = 32*DMA2D_TEST_DATA_LENGTH;

    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, data_len);

    // Malloc buffer
    if (NULL == (src_buffer = malloc(data_len)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }

    DMA2D_NormalMode_Psram_Test(dma_2d_ch5, tfr_mode_m2m, dma2d_sample_unit_word, dma2d_burst_len_16spl, data_len, src_buffer, dst_buffer, dma2d_ahb_burst_len_default, dma2d_ahb_burst_len_128byte);

    free(src_buffer);

    return;
}

static void DMA2D_NormalMode_Sram_To_Psram_WR_256Byte_Test()
{
    uint8_t *src_buffer = NULL;
    uint8_t *dst_buffer = (uint8_t*)0x38000000;
    uint32_t data_len = 32*DMA2D_TEST_DATA_LENGTH;

    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, data_len);

    // Malloc buffer
    if (NULL == (src_buffer = malloc(data_len)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }

    DMA2D_NormalMode_Psram_Test(dma_2d_ch5, tfr_mode_m2m, dma2d_sample_unit_word, dma2d_burst_len_16spl, data_len, src_buffer, dst_buffer, dma2d_ahb_burst_len_default, dma2d_ahb_burst_len_256byte);

    free(src_buffer);

    return;
}

static void DMA2D_NormalMode_Psram_To_Sram_Test()
{
    uint8_t *src_buffer = (uint8_t*)0x38000000;
    uint8_t *dst_buffer = NULL;

    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, DMA2D_TEST_DATA_LENGTH);

    // Malloc buffer
    if (NULL == (dst_buffer = malloc(DMA2D_TEST_DATA_LENGTH)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }

    uint32_t i = 0;
    csk_dma2d_ch_t chn = dma_2d_ch0;

    for (chn = dma_2d_ch0; chn <= dma_2d_ch5; chn++)
    {
        for (i = 0; i < DMA2D_SUCCESSION_LOOP; i++)
        {
            DMA2D_NormalMode_Psram_Test(chn, tfr_mode_m2m, dma2d_sample_unit_word, dma2d_burst_len_1spl, DMA2D_TEST_DATA_LENGTH, src_buffer+i, dst_buffer, dma2d_ahb_burst_len_default, dma2d_ahb_burst_len_default);
        }
    }

    CLOGD("[%s][%d]copy data end", __FUNCTION__, __LINE__);

    free(dst_buffer);

    return;
}

static void DMA2D_NormalMode_Psram_To_Sram_RD_128Byte_Test()
{
    uint8_t *src_buffer = (uint8_t*)0x38000000;
    uint8_t *dst_buffer = NULL;
    uint32_t data_len = 32*DMA2D_TEST_DATA_LENGTH;

    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, data_len);

    // Malloc buffer
    if (NULL == (dst_buffer = malloc(data_len)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }

    DMA2D_NormalMode_Psram_Test(dma_2d_ch0, tfr_mode_m2m, dma2d_sample_unit_word, dma2d_burst_len_16spl, data_len, src_buffer, dst_buffer, dma2d_ahb_burst_len_128byte, dma2d_ahb_burst_len_default);

    DMA2D_NormalMode_Psram_Test(dma_2d_ch5, tfr_mode_m2m, dma2d_sample_unit_word, dma2d_burst_len_16spl, data_len, src_buffer, dst_buffer, dma2d_ahb_burst_len_128byte, dma2d_ahb_burst_len_default);

    free(dst_buffer);

    return;
}

static void DMA2D_NormalMode_Psram_To_Sram_RD_256Byte_Test()
{
    uint8_t *src_buffer = (uint8_t*)0x38000000;
    uint8_t *dst_buffer = NULL;
    uint32_t data_len = 32*DMA2D_TEST_DATA_LENGTH;

    CLOGD("[%s][%d]copy data len=%d", __FUNCTION__, __LINE__, data_len);

    // Malloc buffer
    if (NULL == (dst_buffer = malloc(data_len)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }

    DMA2D_NormalMode_Psram_Test(dma_2d_ch0, tfr_mode_m2m, dma2d_sample_unit_word, dma2d_burst_len_16spl, data_len, src_buffer, dst_buffer, dma2d_ahb_burst_len_256byte, dma2d_ahb_burst_len_default);

    DMA2D_NormalMode_Psram_Test(dma_2d_ch5, tfr_mode_m2m, dma2d_sample_unit_word, dma2d_burst_len_16spl, data_len, src_buffer, dst_buffer, dma2d_ahb_burst_len_256byte, dma2d_ahb_burst_len_default);

    free(dst_buffer);

    return;
}

static void DMA2D_NormalMode_AddrDec_Test(csk_dma2d_ch_t chn, csk_dma2d_sample_unit_t basic_unit, csk_dma2d_burst_len_t burst_len, uint32_t blk_len, uint8_t *dma2d_src_buffer, uint8_t *dma2d_dst_buffer, csk_inc_mode_t src_inc_mode, csk_inc_mode_t dst_inc_mode)
{
    int32_t ret = CSK_DRIVER_OK;
    csk_dma2d_init_t dma2d_para;
    memset(&dma2d_para, 0, sizeof(dma2d_para));

    dma2d_para.dma_ch = chn;
    dma2d_para.src_basic_unit = basic_unit;
    dma2d_para.dst_basic_unit = dma2d_para.src_basic_unit;
    dma2d_para.src_burst_len = burst_len;
    dma2d_para.dst_burst_len = dma2d_para.src_burst_len;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.handshake = hs_none;
    dma2d_para.tfr_mode = tfr_mode_m2m;
    dma2d_para.src_inc_mode = src_inc_mode;
    dma2d_para.dst_inc_mode = dst_inc_mode;

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Start_Normal(chn, (void *)dma2d_src_buffer, (void *)dma2d_dst_buffer, blk_len);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    while (!dma2d_image_event);
    dma2d_image_event = 0;

    ret = DMA2D_Stop(chn);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return;
}

static void DMA2D_NormalMode_AddrDec_SrcInc_DstDec_Test()
{
    uint32_t i = 0;
    uint32_t blk_len = DMA2D_TEST_DATA_LENGTH;
    uint8_t data_buf[3][DMA2D_TEST_DATA_LENGTH] = {0};
    csk_dma2d_ch_t chn = dma_2d_ch0;
    csk_dma2d_burst_len_t burst_len = dma2d_burst_len_1spl;
    csk_dma2d_sample_unit_t basic_unit = dma2d_sample_unit_byte;
    uint32_t unit_len = 0;

    for (chn = dma_2d_ch0; chn <= dma_2d_ch5; chn++)
    {
        for (i = 0; i < blk_len; i++)
        {
            data_buf[0][i] = (i*i*(chn+1))&0xFF;
        }

        for (basic_unit = dma2d_sample_unit_byte; basic_unit <= dma2d_sample_unit_word; basic_unit++)
        {
            for (burst_len = dma2d_burst_len_1spl; burst_len <= dma2d_burst_len_16spl; burst_len++)
            {
                if (dma2d_sample_unit_word == basic_unit)
                    unit_len = 4;
                else if (dma2d_sample_unit_halfword == basic_unit)
                    unit_len = 2;
                else
                    unit_len = 1;

                DMA2D_NormalMode_AddrDec_Test(chn, basic_unit, burst_len, blk_len, data_buf[0], data_buf[2]-unit_len, inc_mode_increase, inc_mode_decrease);

                for (i = 0; i < blk_len; i += unit_len)
                {
                    if (0 != memcmp(&data_buf[0][i], &data_buf[1][blk_len-i-unit_len], unit_len))
                        break;
                }
                if (i < blk_len)
                    CLOGD("[%s][%d]chn=%d,unit=%d,burst_len=%d source:0x%x -> destination:0x%x copy compare error!!!", __FUNCTION__, __LINE__, chn, basic_unit, burst_len, data_buf[0], data_buf[1]);
                else
                    CLOGD("[%s][%d]chn=%d,unit=%d,burst_len=%d source:0x%x -> destination:0x%x copy compare success!!!", __FUNCTION__, __LINE__, chn, basic_unit, burst_len, data_buf[0], data_buf[1]);
            }
        }
    }

    return;
}

static void DMA2D_NormalMode_AddrDec_SrcDec_DstDec_Test()
{
    uint32_t i = 0;
    uint32_t blk_len = DMA2D_TEST_DATA_LENGTH;
    uint8_t data_buf[3][DMA2D_TEST_DATA_LENGTH] = {0};
    csk_dma2d_ch_t chn = dma_2d_ch0;
    csk_dma2d_burst_len_t burst_len = dma2d_burst_len_1spl;
    csk_dma2d_sample_unit_t basic_unit = dma2d_sample_unit_byte;
    uint32_t unit_len = 0;

    for (chn = dma_2d_ch0; chn <= dma_2d_ch5; chn++)
    {
        for (i = 0; i < blk_len; i++)
        {
            data_buf[0][i] = (i*i*(chn+1))&0xFF;
        }

        for (basic_unit = dma2d_sample_unit_byte; basic_unit <= dma2d_sample_unit_word; basic_unit++)
        {
            for (burst_len = dma2d_burst_len_1spl; burst_len <= dma2d_burst_len_16spl; burst_len++)
            {
                if (dma2d_sample_unit_word == basic_unit)
                    unit_len = 4;
                else if (dma2d_sample_unit_halfword == basic_unit)
                    unit_len = 2;
                else
                    unit_len = 1;

                DMA2D_NormalMode_AddrDec_Test(chn, basic_unit, burst_len, blk_len, data_buf[1]-unit_len, data_buf[2]-unit_len, inc_mode_decrease, inc_mode_decrease);

                if (0 != memcmp(data_buf[0], data_buf[1], blk_len))
                    CLOGD("[%s][%d]chn=%d,unit=%d,burst_len=%d source:0x%x -> destination:0x%x copy compare error!!!", __FUNCTION__, __LINE__, chn, basic_unit, burst_len, data_buf[0], data_buf[1]);
                else
                    CLOGD("[%s][%d]chn=%d,unit=%d,burst_len=%d source:0x%x -> destination:0x%x copy compare success!!!", __FUNCTION__, __LINE__, chn, basic_unit, burst_len, data_buf[0], data_buf[1]);
            }
        }
    }

    return;
}

static void DMA2D_NormalMode_AddrDec_SrcDec_DstInc_Test()
{
    uint32_t i = 0;
    uint32_t blk_len = DMA2D_TEST_DATA_LENGTH;
    uint8_t data_buf[3][DMA2D_TEST_DATA_LENGTH] = {0};
    csk_dma2d_ch_t chn = dma_2d_ch0;
    csk_dma2d_burst_len_t burst_len = dma2d_burst_len_1spl;
    csk_dma2d_sample_unit_t basic_unit = dma2d_sample_unit_byte;
    uint32_t unit_len = 0;

    for (chn = dma_2d_ch0; chn <= dma_2d_ch5; chn++)
    {
        for (i = 0; i < blk_len; i++)
        {
            data_buf[0][i] = (i*i*(chn+1))&0xFF;
        }

        for (basic_unit = dma2d_sample_unit_byte; basic_unit <= dma2d_sample_unit_word; basic_unit++)
        {
            for (burst_len = dma2d_burst_len_1spl; burst_len <= dma2d_burst_len_16spl; burst_len++)
            {
                if (dma2d_sample_unit_word == basic_unit)
                    unit_len = 4;
                else if (dma2d_sample_unit_halfword == basic_unit)
                    unit_len = 2;
                else
                    unit_len = 1;

                DMA2D_NormalMode_AddrDec_Test(chn, basic_unit, burst_len, blk_len, data_buf[1]-unit_len, data_buf[1], inc_mode_decrease, inc_mode_increase);

                for (i = 0; i < blk_len; i += unit_len)
                {
                    if (0 != memcmp(&data_buf[0][i], &data_buf[1][blk_len-i-unit_len], unit_len))
                        break;
                }
                if (i < blk_len)
                    CLOGD("[%s][%d]chn=%d,unit=%d,burst_len=%d source:0x%x -> destination:0x%x copy compare error!!!", __FUNCTION__, __LINE__, chn, basic_unit, burst_len, data_buf[0], data_buf[1]);
                else
                    CLOGD("[%s][%d]chn=%d,unit=%d,burst_len=%d source:0x%x -> destination:0x%x copy compare success!!!", __FUNCTION__, __LINE__, chn, basic_unit, burst_len, data_buf[0], data_buf[1]);
            }
        }
    }

    return;
}

static void DMA2D_Src_Gather_Test()
{
    uint32_t i = 0;
    uint32_t j = 0;
    int32_t ret = CSK_DRIVER_OK;
    csk_dma2d_ch_t chn = dma_2d_ch0;
    uint8_t *dma2d_src_buffer = (uint8_t*)0x38000000;
    uint8_t *dma2d_dst_buffer = NULL;

    for (chn = dma_2d_ch0; chn <= dma_2d_ch5; chn++)
    {
        // Malloc buffer
        if (NULL == (dma2d_dst_buffer = (uint8_t*)malloc(DMA2D_TEST_DATA_LENGTH*DMA2D_TEST_DATA_CNT)))
        {
            CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
            return;
        }
        memset(dma2d_dst_buffer, 0, DMA2D_TEST_DATA_LENGTH*DMA2D_TEST_DATA_CNT);

        for (i = 0; i < DMA2D_TEST_DATA_CNT; i++)
        {
            for (j = 0; j < DMA2D_TEST_DATA_LENGTH; j++)
            {
                *(dma2d_src_buffer+i*2*DMA2D_TEST_DATA_LENGTH+j) = (i*j*(chn+1))&0xFF;
            }
        }

        csk_dma2d_init_t dma2d_para;
        memset(&dma2d_para, 0, sizeof(dma2d_para));

        dma2d_para.dma_ch = chn;
        dma2d_para.tfr_mode = tfr_mode_m2m;
        dma2d_para.src_basic_unit = dma2d_sample_unit_byte;
        dma2d_para.dst_basic_unit = dma2d_sample_unit_byte;
        dma2d_para.src_inc_mode = inc_mode_increase;
        dma2d_para.dst_inc_mode = inc_mode_increase;
        dma2d_para.src_burst_len = dma2d_burst_len_1spl;
        dma2d_para.dst_burst_len = dma2d_burst_len_1spl;
        dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
        dma2d_para.prio_lvl = prio_mode_vhigh;
        dma2d_para.handshake = hs_none;
        dma2d_para.src_gather.enable = csk_func_enable;
        dma2d_para.src_gather.interval = DMA2D_TEST_DATA_LENGTH;
        dma2d_para.src_gather.counter = DMA2D_TEST_DATA_LENGTH;

        ret = DMA2D_Initialize();
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        ret = DMA2D_Start_Normal(chn, (void *)dma2d_src_buffer, (void *)dma2d_dst_buffer, DMA2D_TEST_DATA_LENGTH*DMA2D_TEST_DATA_CNT);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        while (!dma2d_image_event);
        dma2d_image_event = 0;

        ret = DMA2D_Stop(chn);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        for (i = 0; i < DMA2D_TEST_DATA_CNT; i++)
        {
            if (0 != memcmp(dma2d_dst_buffer+i*DMA2D_TEST_DATA_LENGTH, dma2d_src_buffer+i*2*DMA2D_TEST_DATA_LENGTH, DMA2D_TEST_DATA_LENGTH))
                CLOGD("[%s][%d]chn=%d index=%u source:0x%x -> destination:0x%x transfer error!!!", __FUNCTION__, __LINE__, chn, i, dma2d_src_buffer+i*2*DMA2D_TEST_DATA_LENGTH, dma2d_dst_buffer+i*DMA2D_TEST_DATA_LENGTH);
            else
                CLOGD("[%s][%d]chn=%d index=%u source:0x%x -> destination:0x%x transfer success!!!", __FUNCTION__, __LINE__, chn, i, dma2d_src_buffer+i*2*DMA2D_TEST_DATA_LENGTH, dma2d_dst_buffer+i*DMA2D_TEST_DATA_LENGTH);
        }

        // Free
        free(dma2d_dst_buffer);
    }
    return;
}

static void DMA2D_Dst_Scatter_Test()
{
    uint32_t i = 0;
    uint32_t j = 0;
    int32_t ret = CSK_DRIVER_OK;
    csk_dma2d_ch_t chn = dma_2d_ch0;
    uint8_t *dma2d_src_buffer = NULL;
    uint8_t *dma2d_dst_buffer = (uint8_t*)0x38000000;

    for (chn = dma_2d_ch0; chn <= dma_2d_ch5; chn++)
    {
        // Malloc buffer
        if (NULL == (dma2d_src_buffer = (uint8_t*)malloc(DMA2D_TEST_DATA_LENGTH*DMA2D_TEST_DATA_CNT)))
        {
            CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
            return;
        }
        for (i = 0; i < DMA2D_TEST_DATA_CNT; i++)
        {
            for (j = 0; j < DMA2D_TEST_DATA_LENGTH; j++)
            {
                *(dma2d_src_buffer+j) = (i*j*(chn+1))&0xFF;
            }
        }
        memset(dma2d_dst_buffer, 0, DMA2D_TEST_DATA_LENGTH*DMA2D_TEST_DATA_CNT*2);

        csk_dma2d_init_t dma2d_para;
        memset(&dma2d_para, 0, sizeof(dma2d_para));

        dma2d_para.dma_ch = chn;
        dma2d_para.tfr_mode = tfr_mode_m2m;
        dma2d_para.src_basic_unit = dma2d_sample_unit_word;
        dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
        dma2d_para.src_inc_mode = inc_mode_increase;
        dma2d_para.dst_inc_mode = inc_mode_increase;
        dma2d_para.src_burst_len = dma2d_burst_len_8spl;
        dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
        dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
        dma2d_para.prio_lvl = prio_mode_vhigh;
        dma2d_para.handshake = hs_none;
        dma2d_para.dst_scatter.enable = csk_func_enable;
        dma2d_para.dst_scatter.interval = DMA2D_TEST_DATA_LENGTH;
        dma2d_para.dst_scatter.counter = DMA2D_TEST_DATA_LENGTH;

        ret = DMA2D_Initialize();
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        ret = DMA2D_Start_Normal(chn, (void *)dma2d_src_buffer, (void *)dma2d_dst_buffer, DMA2D_TEST_DATA_LENGTH*DMA2D_TEST_DATA_CNT);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        while (!dma2d_image_event);
        dma2d_image_event = 0;

        ret = DMA2D_Stop(chn);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        for (i = 0; i < DMA2D_TEST_DATA_CNT; i++)
        {
            if (0 != memcmp(dma2d_src_buffer+i*DMA2D_TEST_DATA_LENGTH, dma2d_dst_buffer+i*2*DMA2D_TEST_DATA_LENGTH, DMA2D_TEST_DATA_LENGTH))
                CLOGD("[%s][%d]chn=%d index=%u source:0x%x -> destination:0x%x transfer error!!!", __FUNCTION__, __LINE__, chn, i, dma2d_src_buffer+i*DMA2D_TEST_DATA_LENGTH, dma2d_dst_buffer+i*2*DMA2D_TEST_DATA_LENGTH);
            else
                CLOGD("[%s][%d]chn=%d index=%u source:0x%x -> destination:0x%x transfer success!!!", __FUNCTION__, __LINE__, chn, i, dma2d_src_buffer+i*DMA2D_TEST_DATA_LENGTH, dma2d_dst_buffer+i*2*DMA2D_TEST_DATA_LENGTH);
        }

        // Free
        free(dma2d_src_buffer);
    }
    return;
}

static void DMA2D_Image_Crop_Test(csk_dma2d_ch_t chn, csk_image_info_t *img_info, uint8_t *img_buff, uint32_t img_buff_offset)
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t blk_len = 0;
    uint8_t *out_image_buf = NULL;
    csk_dma2d_init_t dma2d_para;
    memset(&dma2d_para, 0, sizeof(dma2d_para));

    dma2d_para.dma_ch = chn;
    dma2d_para.tfr_mode = tfr_mode_m2m;
    dma2d_para.src_basic_unit = dma2d_sample_unit_word;
    dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
    dma2d_para.src_inc_mode = inc_mode_increase;
    dma2d_para.dst_inc_mode = inc_mode_increase;
    dma2d_para.src_burst_len = dma2d_burst_len_8spl;
    dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.handshake = hs_none;

    csk_dma_2d_image_cfg_t dma2d_img_cfg;
    memset(&dma2d_img_cfg, 0, sizeof(dma2d_img_cfg));

    dma2d_img_cfg.img_input.img_width = img_info->img_width;
    dma2d_img_cfg.img_input.img_height = img_info->img_height;
    dma2d_img_cfg.img_input.img_format = img_info->img_format;
    dma2d_img_cfg.img_input.img_line_stride = img_info->img_line_stride;
    dma2d_img_cfg.img_output.img_width = img_info->img_width;
    dma2d_img_cfg.img_output.img_height = img_info->img_height;
    dma2d_img_cfg.img_output.img_format = img_info->img_format;
    dma2d_img_cfg.img_output.img_line_stride = dma2d_img_cfg.img_output.img_width * dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format);
    dma2d_img_cfg.img_crop_en = csk_func_enable;

    // Malloc Buffer
    blk_len = dma2d_img_cfg.img_input.img_width * dma2d_img_cfg.img_input.img_height * dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format);
    if (NULL == (out_image_buf = malloc(blk_len)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }
    memset(out_image_buf, 0, blk_len);

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Image_Config_Extend(chn, &dma2d_img_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Start_Normal(chn, img_buff+img_buff_offset, out_image_buf, blk_len);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    while(!dma2d_image_event);
    dma2d_image_event = 0;

    ret = DMA2D_Stop(chn);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    CLOGD("[%s][%d]chn=%d offset=%u source:0x%x -> destination:0x%x crop success!!!", __FUNCTION__, __LINE__, chn, img_buff_offset, img_buff, out_image_buf);

    free(out_image_buf);

    return;
}

static void DMA2D_Image_Crop_YUV444_128x128_Test()
{
    uint32_t img_buff_offset = 0;
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 64;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image size from [%d, %d] to [%d, %d]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info.img_width = img_width_out;
    img_info.img_height = img_height_out;
    img_info.img_format = csk_image_format_yuv444_packed;
    img_info.img_line_stride = img_width_in*dma2d_pixel_bitw(img_info.img_format);
    img_buff_offset = (((img_height_in-img_height_out)/2)*img_width_in+(img_width_in-img_width_out)/2)*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Crop_Test(dma_2d_ch0, &img_info, enc_yuv444_128x128, img_buff_offset);
}

static void DMA2D_Image_Crop_YUV422_128x128_Test()
{
    uint32_t img_buff_offset = 0;
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 64;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image size from [%d, %d] to [%d, %d]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info.img_width = img_width_out;
    img_info.img_height = img_height_out;
    img_info.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info.img_line_stride = img_width_in*dma2d_pixel_bitw(img_info.img_format);
    img_buff_offset = (((img_height_in-img_height_out)/2)*img_width_in+(img_width_in-img_width_out)/2)*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Crop_Test(dma_2d_ch0, &img_info, enc_yuv422_128x128, img_buff_offset);
}

static void DMA2D_Image_Crop_RGB888_128x128_Test()
{
    uint32_t img_buff_offset = 0;
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 64;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image size from [%d, %d] to [%d, %d]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info.img_width = img_width_out;
    img_info.img_height = img_height_out;
    img_info.img_format = csk_image_format_rgb888;
    img_info.img_line_stride = img_width_in*dma2d_pixel_bitw(img_info.img_format);
    img_buff_offset = (((img_height_in-img_height_out)/2)*img_width_in+(img_width_in-img_width_out)/2)*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Crop_Test(dma_2d_ch0, &img_info, enc_rgb888_128x128, img_buff_offset);
}

static void DMA2D_Image_Crop_RGB565_128x128_Test()
{
    uint32_t img_buff_offset = 0;
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 64;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image size from [%d, %d] to [%d, %d]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info.img_width = img_width_out;
    img_info.img_height = img_height_out;
    img_info.img_format = csk_image_format_rgb565;
    img_info.img_line_stride = img_width_in*dma2d_pixel_bitw(img_info.img_format);
    img_buff_offset = (((img_height_in-img_height_out)/2)*img_width_in+(img_width_in-img_width_out)/2)*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Crop_Test(dma_2d_ch0, &img_info, enc_rgb565_128x128, img_buff_offset);
}

static void DMA2D_Image_Crop_Y8_128x128_Test()
{
    uint32_t img_buff_offset = 0;
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 64;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image size from [%d, %d] to [%d, %d]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info.img_width = img_width_out;
    img_info.img_height = img_height_out;
    img_info.img_format = csk_image_format_y8;
    img_info.img_line_stride = img_width_in*dma2d_pixel_bitw(img_info.img_format);
    img_buff_offset = (((img_height_in-img_height_out)/2)*img_width_in+(img_width_in-img_width_out)/2)*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Crop_Test(dma_2d_ch0, &img_info, enc_gray_128x128, img_buff_offset);
}

static void DMA2D_Image_format_conv_Test(csk_dma2d_ch_t chn, csk_image_format_t format_in, csk_image_format_t format_out, uint8_t *img_buff)
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t blk_len = 0;
    uint8_t *out_image_buf = NULL;
    csk_dma2d_init_t dma2d_para;
    memset(&dma2d_para, 0, sizeof(dma2d_para));

    dma2d_para.dma_ch = chn;
    dma2d_para.tfr_mode = tfr_mode_m2m;
    dma2d_para.src_basic_unit = dma2d_sample_unit_word;
    dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
    dma2d_para.src_inc_mode = inc_mode_increase;
    dma2d_para.dst_inc_mode = inc_mode_increase;
    dma2d_para.src_burst_len = dma2d_burst_len_8spl;
    dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.handshake = hs_none;

    csk_dma_2d_image_cfg_t dma2d_img_cfg;
    memset(&dma2d_img_cfg, 0, sizeof(dma2d_img_cfg));

    dma2d_img_cfg.img_input.img_width = 128;
    dma2d_img_cfg.img_input.img_height = 128;
    dma2d_img_cfg.img_input.img_format = format_in;
    dma2d_img_cfg.img_input.img_line_stride = dma2d_img_cfg.img_input.img_width * dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format);
    dma2d_img_cfg.img_output.img_width = dma2d_img_cfg.img_input.img_width;
    dma2d_img_cfg.img_output.img_height = dma2d_img_cfg.img_input.img_height;
    dma2d_img_cfg.img_output.img_format = format_out;
    dma2d_img_cfg.img_output.img_line_stride = dma2d_img_cfg.img_output.img_width * dma2d_pixel_bitw(dma2d_img_cfg.img_output.img_format);

    // Malloc Buffer
    blk_len = dma2d_img_cfg.img_output.img_width * dma2d_img_cfg.img_output.img_height * dma2d_pixel_bitw(dma2d_img_cfg.img_output.img_format);
    if (NULL == (out_image_buf = malloc(blk_len)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }
    memset(out_image_buf, 0, blk_len);

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Image_Config_Extend(chn, &dma2d_img_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Start_Normal(chn, img_buff, out_image_buf, blk_len);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    while(!dma2d_image_event);
    dma2d_image_event = 0;

    ret = DMA2D_Stop(chn);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    CLOGD("[%s][%d]chn=%d source:0x%x -> destination:0x%x(%u) format conv success!!!", __FUNCTION__, __LINE__, chn, img_buff, out_image_buf, blk_len);

    free(out_image_buf);

    return;
}

static void DMA2D_Image_YUV444_To_YUV422_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_yuv444_packed, csk_image_format_yuv422_yuyv_packed);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_yuv444_packed, csk_image_format_yuv422_yuyv_packed, enc_yuv444_128x128);
}

static void DMA2D_Image_YUV444_To_RGB888_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_yuv444_packed, csk_image_format_rgb888);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_yuv444_packed, csk_image_format_rgb888, enc_yuv444_128x128);
}

static void DMA2D_Image_YUV444_To_RGB565_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_yuv444_packed, csk_image_format_rgb565);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_yuv444_packed, csk_image_format_rgb565, enc_yuv444_128x128);
}

static void DMA2D_Image_YUV444_To_Y8_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_yuv444_packed, csk_image_format_y8);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_yuv444_packed, csk_image_format_y8, enc_yuv444_128x128);
}

static void DMA2D_Image_YUV422_To_YUV444_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_yuv422_yuyv_packed, csk_image_format_yuv444_packed);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_yuv422_yuyv_packed, csk_image_format_yuv444_packed, enc_yuv422_128x128);
}

static void DMA2D_Image_YUV422_To_RGB888_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_yuv422_yuyv_packed, csk_image_format_rgb888);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_yuv422_yuyv_packed, csk_image_format_rgb888, enc_yuv422_128x128);
}

static void DMA2D_Image_YUV422_To_RGB565_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_yuv422_yuyv_packed, csk_image_format_rgb565);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_yuv422_yuyv_packed, csk_image_format_rgb565, enc_yuv422_128x128);
}

static void DMA2D_Image_YUV422_To_Y8_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_yuv422_yuyv_packed, csk_image_format_y8);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_yuv422_yuyv_packed, csk_image_format_y8, enc_yuv422_128x128);
}

static void DMA2D_Image_RGB888_To_RGB565_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_rgb888, csk_image_format_rgb565);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_rgb888, csk_image_format_rgb565, enc_rgb888_128x128);
}

static void DMA2D_Image_RGB888_To_YUV444_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_rgb888, csk_image_format_yuv444_packed);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_rgb888, csk_image_format_yuv444_packed, enc_rgb888_128x128);
}

static void DMA2D_Image_RGB888_To_YUV422_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_rgb888, csk_image_format_yuv422_yuyv_packed);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_rgb888, csk_image_format_yuv422_yuyv_packed, enc_rgb888_128x128);
}

static void DMA2D_Image_RGB888_To_Y8_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_rgb888, csk_image_format_y8);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_rgb888, csk_image_format_y8, enc_rgb888_128x128);
}

static void DMA2D_Image_RGB565_To_RGB888_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_rgb565, csk_image_format_rgb888);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_rgb565, csk_image_format_rgb888, enc_rgb565_128x128);
}

static void DMA2D_Image_RGB565_To_YUV444_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_rgb565, csk_image_format_yuv444_packed);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_rgb565, csk_image_format_yuv444_packed, enc_rgb565_128x128);
}

static void DMA2D_Image_RGB565_To_YUV422_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_rgb565, csk_image_format_yuv422_yuyv_packed);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_rgb565, csk_image_format_yuv422_yuyv_packed, enc_rgb565_128x128);
}

static void DMA2D_Image_RGB565_To_Y8_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_rgb565, csk_image_format_y8);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_rgb565, csk_image_format_y8, enc_rgb565_128x128);
}

static void DMA2D_Image_Y8_To_YUV444_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_y8, csk_image_format_yuv444_packed);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_y8, csk_image_format_yuv444_packed, enc_gray_128x128);
}

static void DMA2D_Image_Y8_To_YUV422_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_y8, csk_image_format_yuv422_yuyv_packed);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_y8, csk_image_format_yuv422_yuyv_packed, enc_gray_128x128);
}

static void DMA2D_Image_Y8_To_RGB888_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_y8, csk_image_format_rgb888);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_y8, csk_image_format_rgb888, enc_gray_128x128);
}

static void DMA2D_Image_Y8_To_RGB565_128x128_Test()
{
    CLOGD("[%s][%d]image format from %d to %d", __FUNCTION__, __LINE__, csk_image_format_y8, csk_image_format_rgb565);

    return DMA2D_Image_format_conv_Test(dma_2d_ch0, csk_image_format_y8, csk_image_format_rgb565, enc_gray_128x128);
}

static void DMA2D_Image_Copy_Test(csk_dma2d_ch_t chn, csk_image_info_t *img_info, uint8_t *img_buff, uint16_t img_w_offset, uint16_t img_h_offset, csk_func_switch_t enable_2d)
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t blk_len = 0;
    uint32_t blk_offset = 0;
    uint8_t *out_image_buf = NULL;
    csk_dma2d_init_t dma2d_para;
    memset(&dma2d_para, 0, sizeof(dma2d_para));

    dma2d_para.dma_ch = chn;
    dma2d_para.tfr_mode = tfr_mode_m2m;
    dma2d_para.src_basic_unit = dma2d_sample_unit_word;
    dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
    dma2d_para.src_inc_mode = inc_mode_increase;
    dma2d_para.dst_inc_mode = inc_mode_increase;
    dma2d_para.src_burst_len = dma2d_burst_len_8spl;
    dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.handshake = hs_none;

    csk_dma_2d_image_cfg_t dma2d_img_cfg;
    memset(&dma2d_img_cfg, 0, sizeof(dma2d_img_cfg));

    dma2d_img_cfg.img_input = *img_info;
    dma2d_img_cfg.img_output = dma2d_img_cfg.img_input;
    dma2d_img_cfg.img_copy_en = enable_2d;

    // Malloc Buffer
    blk_len = dma2d_img_cfg.img_input.img_width * dma2d_img_cfg.img_input.img_height * dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format) * 2;
    blk_offset = (dma2d_img_cfg.img_input.img_width * img_h_offset + img_w_offset) * dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format);
    if (NULL == (out_image_buf = (uint8_t*)malloc(blk_len)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }
    memset(out_image_buf, 0, blk_len);
    memcpy(out_image_buf, img_buff, blk_len/2);

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Image_Config_Extend(chn, &dma2d_img_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Start_Normal(chn, out_image_buf, out_image_buf+blk_offset, blk_len/2);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    while(!dma2d_image_event);
    dma2d_image_event = 0;

    ret = DMA2D_Stop(chn);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    CLOGD("[%s][%d]chn=%d offset=%u source:0x%x -> destination:0x%x copy success(0x%x)!!!", __FUNCTION__, __LINE__, chn, blk_offset, out_image_buf, out_image_buf+blk_offset, img_buff);

    free(out_image_buf);

    return;
}

static void DMA2D_Image_Copy_YUV444_128x128_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_w_offset = 0;
    uint16_t img_h_offset = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image copy [%u, %u], offset [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_w_offset, img_h_offset);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_yuv444_packed;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Copy_Test(dma_2d_ch0, &img_info, enc_yuv444_128x128, img_w_offset, img_h_offset, csk_func_enable);
}

static void DMA2D_Image_Copy_YUV444_128x128_Overlap_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_w_offset = 64;
    uint16_t img_h_offset = 64;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image copy [%u, %u], offset [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_w_offset, img_h_offset);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_yuv444_packed;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Copy_Test(dma_2d_ch0, &img_info, enc_yuv444_128x128, img_w_offset, img_h_offset, csk_func_enable);
}

static void DMA2D_Image_Copy_YUV444_128x128_1d_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_w_offset = 0;
    uint16_t img_h_offset = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image copy [%u, %u], offset [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_w_offset, img_h_offset);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_yuv444_packed;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Copy_Test(dma_2d_ch0, &img_info, enc_yuv444_128x128, img_w_offset, img_h_offset, csk_func_disable);
}

static void DMA2D_Image_Copy_YUV444_128x128_Overlap_1d_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_w_offset = 64;
    uint16_t img_h_offset = 64;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image copy [%u, %u], offset [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_w_offset, img_h_offset);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_yuv444_packed;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Copy_Test(dma_2d_ch0, &img_info, enc_yuv444_128x128, img_w_offset, img_h_offset, csk_func_disable);
}

static void DMA2D_Image_Copy_YUV422_128x128_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_w_offset = 0;
    uint16_t img_h_offset = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image copy [%u, %u], offset [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_w_offset, img_h_offset);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Copy_Test(dma_2d_ch0, &img_info, enc_yuv422_128x128, img_w_offset, img_h_offset, csk_func_enable);
}

static void DMA2D_Image_Copy_YUV422_128x128_Overlap_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_w_offset = 64;
    uint16_t img_h_offset = 64;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image copy [%u, %u], offset [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_w_offset, img_h_offset);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Copy_Test(dma_2d_ch0, &img_info, enc_yuv422_128x128, img_w_offset, img_h_offset, csk_func_enable);
}

static void DMA2D_Image_Copy_YUV422_128x128_1d_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_w_offset = 0;
    uint16_t img_h_offset = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image copy [%u, %u], offset [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_w_offset, img_h_offset);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Copy_Test(dma_2d_ch0, &img_info, enc_yuv422_128x128, img_w_offset, img_h_offset, csk_func_disable);
}

static void DMA2D_Image_Copy_RGB888_128x128_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_w_offset = 0;
    uint16_t img_h_offset = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image copy [%u, %u], offset [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_w_offset, img_h_offset);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_rgb888;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Copy_Test(dma_2d_ch0, &img_info, enc_rgb888_128x128, img_w_offset, img_h_offset, csk_func_enable);
}

static void DMA2D_Image_Copy_RGB888_128x128_Overlap_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_w_offset = 64;
    uint16_t img_h_offset = 64;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image copy [%u, %u], offset [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_w_offset, img_h_offset);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_rgb888;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Copy_Test(dma_2d_ch0, &img_info, enc_rgb888_128x128, img_w_offset, img_h_offset, csk_func_enable);
}

static void DMA2D_Image_Copy_RGB888_128x128_1d_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_w_offset = 0;
    uint16_t img_h_offset = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image copy [%u, %u], offset [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_w_offset, img_h_offset);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_rgb888;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Copy_Test(dma_2d_ch0, &img_info, enc_rgb888_128x128, img_w_offset, img_h_offset, csk_func_disable);
}

static void DMA2D_Image_Copy_RGB565_128x128_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_w_offset = 0;
    uint16_t img_h_offset = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image copy [%u, %u], offset [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_w_offset, img_h_offset);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_rgb565;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Copy_Test(dma_2d_ch0, &img_info, enc_rgb565_128x128, img_w_offset, img_h_offset, csk_func_enable);
}

static void DMA2D_Image_Copy_RGB565_128x128_Overlap_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_w_offset = 64;
    uint16_t img_h_offset = 64;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image copy [%u, %u], offset [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_w_offset, img_h_offset);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_rgb565;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Copy_Test(dma_2d_ch0, &img_info, enc_rgb565_128x128, img_w_offset, img_h_offset, csk_func_enable);
}

static void DMA2D_Image_Copy_RGB565_128x128_1d_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_w_offset = 0;
    uint16_t img_h_offset = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image copy [%u, %u], offset [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_w_offset, img_h_offset);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_rgb565;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Copy_Test(dma_2d_ch0, &img_info, enc_rgb565_128x128, img_w_offset, img_h_offset, csk_func_disable);
}

static void DMA2D_Image_Copy_Y8_128x128_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_w_offset = 0;
    uint16_t img_h_offset = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image copy [%u, %u], offset [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_w_offset, img_h_offset);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_y8;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Copy_Test(dma_2d_ch0, &img_info, enc_gray_128x128, img_w_offset, img_h_offset, csk_func_enable);
}

static void DMA2D_Image_Copy_Y8_128x128_Overlap_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_w_offset = 64;
    uint16_t img_h_offset = 64;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image copy [%u, %u], offset [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_w_offset, img_h_offset);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_y8;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Copy_Test(dma_2d_ch0, &img_info, enc_gray_128x128, img_w_offset, img_h_offset, csk_func_enable);
}

static void DMA2D_Image_Copy_Y8_128x128_1d_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_w_offset = 0;
    uint16_t img_h_offset = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image copy [%u, %u], offset [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_w_offset, img_h_offset);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_y8;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Copy_Test(dma_2d_ch0, &img_info, enc_gray_128x128, img_w_offset, img_h_offset, csk_func_disable);
}

static void DMA2D_Image_Rotation_Test(csk_dma2d_ch_t chn, csk_image_info_t *img_info_in, csk_image_info_t *img_info_out, uint8_t *img_buff, csk_rotation_mode_t rot_mode, csk_func_switch_t enable_tile)
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t blk_len = 0;
    uint8_t *out_image_buf = NULL;
    csk_dma2d_init_t dma2d_para;
    memset(&dma2d_para, 0, sizeof(dma2d_para));

    dma2d_para.dma_ch = chn;
    dma2d_para.tfr_mode = tfr_mode_m2m;
    dma2d_para.src_basic_unit = dma2d_sample_unit_word;
    dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
    dma2d_para.src_inc_mode = inc_mode_increase;
    dma2d_para.dst_inc_mode = inc_mode_increase;
    dma2d_para.src_burst_len = dma2d_burst_len_8spl;
    dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.handshake = hs_none;

    csk_dma_2d_image_cfg_t dma2d_img_cfg;
    memset(&dma2d_img_cfg, 0, sizeof(dma2d_img_cfg));

    dma2d_img_cfg.img_input = *img_info_in;
    dma2d_img_cfg.img_output = *img_info_out;
    dma2d_img_cfg.img_rota_en = csk_func_enable;
    dma2d_img_cfg.img_rota_tile_en = enable_tile;
    dma2d_img_cfg.img_rota_mode = rot_mode;

    // Malloc Buffer
    blk_len = dma2d_img_cfg.img_output.img_width * dma2d_img_cfg.img_output.img_height * dma2d_pixel_bitw(dma2d_img_cfg.img_output.img_format);
    if (NULL == (out_image_buf = (uint8_t*)malloc(blk_len)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }
    memset(out_image_buf, 0, blk_len);

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Image_Config_Extend(chn, &dma2d_img_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Start_Normal(chn, img_buff, out_image_buf, blk_len);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    while(!dma2d_image_event);
    dma2d_image_event = 0;

    ret = DMA2D_Stop(chn);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    CLOGD("[%s][%d]chn=%d source:0x%x -> destination:0x%x rotation success!!!", __FUNCTION__, __LINE__, chn, img_buff, out_image_buf);

    free(out_image_buf);

    return;
}

static void DMA2D_Image_Rotation_YUV444_128x128_CW90_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_yuv444_128x128, csk_rota_cw90, csk_func_disable);
}

static void DMA2D_Image_Rotation_YUV444_128x128_CW90_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_yuv444_128x128, csk_rota_cw90, csk_func_enable);
}

static void DMA2D_Image_Rotation_YUV444_128x128_CCW90_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_yuv444_128x128, csk_rota_ccw90, csk_func_disable);
}

static void DMA2D_Image_Rotation_YUV444_128x128_CCW90_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_yuv444_128x128, csk_rota_ccw90, csk_func_enable);
}

static void DMA2D_Image_Rotation_YUV444_128x128_CW180_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_width;
    img_info_out.img_height = img_info_in.img_height;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_yuv444_128x128, csk_rota_cw180, csk_func_disable);
}

static void DMA2D_Image_Rotation_YUV444_128x128_CW180_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_width;
    img_info_out.img_height = img_info_in.img_height;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_yuv444_128x128, csk_rota_cw180, csk_func_enable);
}

static void DMA2D_Image_Rotation_YUV444_128x128_Transpose_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_yuv444_128x128, csk_rota_transpose, csk_func_disable);
}

static void DMA2D_Image_Rotation_YUV444_128x128_Transpose_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_yuv444_128x128, csk_rota_transpose, csk_func_enable);
}

static void DMA2D_Image_Rotation_YUV422_128x128_CW90_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_yuv422_128x128, csk_rota_cw90, csk_func_disable);
}

static void DMA2D_Image_Rotation_YUV422_128x128_CW90_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_yuv422_128x128, csk_rota_cw90, csk_func_enable);
}

static void DMA2D_Image_Rotation_YUV422_128x128_CCW90_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_yuv422_128x128, csk_rota_ccw90, csk_func_disable);
}

static void DMA2D_Image_Rotation_YUV422_128x128_CCW90_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_yuv422_128x128, csk_rota_ccw90, csk_func_enable);
}

static void DMA2D_Image_Rotation_YUV422_128x128_CW180_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_width;
    img_info_out.img_height = img_info_in.img_height;
    img_info_out.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_yuv422_128x128, csk_rota_cw180, csk_func_disable);
}

static void DMA2D_Image_Rotation_YUV422_128x128_CW180_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_width;
    img_info_out.img_height = img_info_in.img_height;
    img_info_out.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_yuv422_128x128, csk_rota_cw180, csk_func_enable);
}

static void DMA2D_Image_Rotation_YUV422_128x128_Transpose_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_yuv422_128x128, csk_rota_transpose, csk_func_disable);
}

static void DMA2D_Image_Rotation_YUV422_128x128_Transpose_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_yuv422_128x128, csk_rota_transpose, csk_func_enable);
}

static void DMA2D_Image_Rotation_RGB888_128x128_CW90_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb888_128x128, csk_rota_cw90, csk_func_disable);
}

static void DMA2D_Image_Rotation_RGB888_128x128_CW90_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb888_128x128, csk_rota_cw90, csk_func_enable);
}

static void DMA2D_Image_Rotation_RGB888_128x128_CCW90_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb888_128x128, csk_rota_ccw90, csk_func_disable);
}

static void DMA2D_Image_Rotation_RGB888_128x128_CCW90_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb888_128x128, csk_rota_ccw90, csk_func_enable);
}

static void DMA2D_Image_Rotation_RGB888_128x128_CW180_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_width;
    img_info_out.img_height = img_info_in.img_height;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb888_128x128, csk_rota_cw180, csk_func_disable);
}

static void DMA2D_Image_Rotation_RGB888_128x128_CW180_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_width;
    img_info_out.img_height = img_info_in.img_height;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb888_128x128, csk_rota_cw180, csk_func_enable);
}

static void DMA2D_Image_Rotation_RGB888_128x128_Transpose_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb888_128x128, csk_rota_transpose, csk_func_disable);
}

static void DMA2D_Image_Rotation_RGB888_128x128_Transpose_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb888_128x128, csk_rota_transpose, csk_func_enable);
}

static void DMA2D_Image_Rotation_RGB565_128x128_CW90_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb565;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_rgb565;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb565_128x128, csk_rota_cw90, csk_func_disable);
}

static void DMA2D_Image_Rotation_RGB565_128x128_CW90_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb565;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_rgb565;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb565_128x128, csk_rota_cw90, csk_func_enable);
}

static void DMA2D_Image_Rotation_RGB565_128x128_CCW90_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb565;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_rgb565;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb565_128x128, csk_rota_ccw90, csk_func_disable);
}

static void DMA2D_Image_Rotation_RGB565_128x128_CCW90_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb565;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_rgb565;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb565_128x128, csk_rota_ccw90, csk_func_enable);
}

static void DMA2D_Image_Rotation_RGB565_128x128_CW180_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb565;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_width;
    img_info_out.img_height = img_info_in.img_height;
    img_info_out.img_format = csk_image_format_rgb565;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb565_128x128, csk_rota_cw180, csk_func_disable);
}

static void DMA2D_Image_Rotation_RGB565_128x128_CW180_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb565;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_width;
    img_info_out.img_height = img_info_in.img_height;
    img_info_out.img_format = csk_image_format_rgb565;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb565_128x128, csk_rota_cw180, csk_func_enable);
}

static void DMA2D_Image_Rotation_RGB565_128x128_Transpose_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb565;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_rgb565;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb565_128x128, csk_rota_transpose, csk_func_disable);
}

static void DMA2D_Image_Rotation_RGB565_128x128_Transpose_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb565;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_rgb565;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb565_128x128, csk_rota_transpose, csk_func_enable);
}

static void DMA2D_Image_Rotation_Y8_128x128_CW90_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_y8;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_y8;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_gray_128x128, csk_rota_cw90, csk_func_disable);
}

static void DMA2D_Image_Rotation_Y8_128x128_CW90_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_y8;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_y8;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_gray_128x128, csk_rota_cw90, csk_func_enable);
}

static void DMA2D_Image_Rotation_Y8_128x128_CCW90_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_y8;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_y8;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_gray_128x128, csk_rota_ccw90, csk_func_disable);
}

static void DMA2D_Image_Rotation_Y8_128x128_CCW90_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_y8;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_y8;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_gray_128x128, csk_rota_ccw90, csk_func_enable);
}

static void DMA2D_Image_Rotation_Y8_128x128_CW180_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_y8;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_width;
    img_info_out.img_height = img_info_in.img_height;
    img_info_out.img_format = csk_image_format_y8;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_gray_128x128, csk_rota_cw180, csk_func_disable);
}

static void DMA2D_Image_Rotation_Y8_128x128_CW180_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_y8;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_width;
    img_info_out.img_height = img_info_in.img_height;
    img_info_out.img_format = csk_image_format_y8;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_gray_128x128, csk_rota_cw180, csk_func_enable);
}

static void DMA2D_Image_Rotation_Y8_128x128_Transpose_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_y8;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_y8;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_gray_128x128, csk_rota_transpose, csk_func_disable);
}

static void DMA2D_Image_Rotation_Y8_128x128_Transpose_Tile_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_y8;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_y8;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_gray_128x128, csk_rota_transpose, csk_func_enable);
}

static void DMA2D_Image_Rotation_RGB888_160x120_CW90_Test()
{
    uint16_t img_width_in = 160;
    uint16_t img_height_in = 120;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb888_160x120, csk_rota_cw90, csk_func_disable);
}

static void DMA2D_Image_Rotation_RGB888_160x120_CW90_Tile_Test()
{
    uint16_t img_width_in = 160;
    uint16_t img_height_in = 120;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb888_160x120, csk_rota_cw90, csk_func_enable);
}

static void DMA2D_Image_Rotation_RGB888_160x120_CCW90_Test()
{
    uint16_t img_width_in = 160;
    uint16_t img_height_in = 120;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb888_160x120, csk_rota_ccw90, csk_func_disable);
}

static void DMA2D_Image_Rotation_RGB888_160x120_CW180_Test()
{
    uint16_t img_width_in = 160;
    uint16_t img_height_in = 120;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_width;
    img_info_out.img_height = img_info_in.img_height;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb888_160x120, csk_rota_cw180, csk_func_disable);
}

static void DMA2D_Image_Rotation_RGB888_160x120_Transpose_Test()
{
    uint16_t img_width_in = 160;
    uint16_t img_height_in = 120;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image rotation from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_height_in, img_width_in);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_info_in.img_height;
    img_info_out.img_height = img_info_in.img_width;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Rotation_Test(dma_2d_ch1, &img_info_in, &img_info_out, enc_rgb888_160x120, csk_rota_transpose, csk_func_disable);
}

static void DMA2D_Image_Mirror_Test(csk_dma2d_ch_t chn, csk_image_info_t *img_info, uint8_t *img_buff, csk_mirror_mode_t mir_mode)
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t blk_len = 0;
    uint8_t *out_image_buf = NULL;
    csk_dma2d_init_t dma2d_para;
    memset(&dma2d_para, 0, sizeof(dma2d_para));

    dma2d_para.dma_ch = chn;
    dma2d_para.tfr_mode = tfr_mode_m2m;
    dma2d_para.src_basic_unit = dma2d_sample_unit_word;
    dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
    dma2d_para.src_inc_mode = inc_mode_increase;
    dma2d_para.dst_inc_mode = inc_mode_increase;
    dma2d_para.src_burst_len = dma2d_burst_len_8spl;
    dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.handshake = hs_none;

    csk_dma_2d_image_cfg_t dma2d_img_cfg;
    memset(&dma2d_img_cfg, 0, sizeof(dma2d_img_cfg));

    dma2d_img_cfg.img_input = *img_info;
    dma2d_img_cfg.img_output = dma2d_img_cfg.img_input;
    dma2d_img_cfg.img_mirror_en = csk_func_enable;
    dma2d_img_cfg.img_mirror_mode = mir_mode;

    // Malloc Buffer
    blk_len = dma2d_img_cfg.img_output.img_width * dma2d_img_cfg.img_output.img_height * dma2d_pixel_bitw(dma2d_img_cfg.img_output.img_format);
    if (NULL == (out_image_buf = (uint8_t*)malloc(blk_len)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }
    memset(out_image_buf, 0, blk_len);

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Image_Config_Extend(chn, &dma2d_img_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Start_Normal(chn, img_buff, out_image_buf, blk_len);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    while(!dma2d_image_event);
    dma2d_image_event = 0;

    ret = DMA2D_Stop(chn);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    CLOGD("[%s][%d]chn=%d source:0x%x -> destination:0x%x mirror success!!!", __FUNCTION__, __LINE__, chn, img_buff, out_image_buf);

    free(out_image_buf);

    return;
}

static void DMA2D_Image_Mirror_YUV444_128x128_Hor_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image mirror [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_yuv444_packed;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Mirror_Test(dma_2d_ch1, &img_info, enc_yuv444_128x128, csk_mirror_hor);
}

static void DMA2D_Image_Mirror_YUV444_128x128_Vert_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image mirror [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_yuv444_packed;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Mirror_Test(dma_2d_ch1, &img_info, enc_yuv444_128x128, csk_mirror_vert);
}

static void DMA2D_Image_Mirror_YUV422_128x128_Hor_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image mirror [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Mirror_Test(dma_2d_ch1, &img_info, enc_yuv422_128x128, csk_mirror_hor);
}

static void DMA2D_Image_Mirror_YUV422_128x128_Vert_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image mirror [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Mirror_Test(dma_2d_ch1, &img_info, enc_yuv422_128x128, csk_mirror_vert);
}

static void DMA2D_Image_Mirror_RGB888_128x128_Hor_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image mirror [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_rgb888;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Mirror_Test(dma_2d_ch1, &img_info, enc_rgb888_128x128, csk_mirror_hor);
}

static void DMA2D_Image_Mirror_RGB888_128x128_Vert_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image mirror [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_rgb888;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Mirror_Test(dma_2d_ch1, &img_info, enc_rgb888_128x128, csk_mirror_vert);
}

static void DMA2D_Image_Mirror_RGB565_128x128_Hor_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image mirror [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_rgb565;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Mirror_Test(dma_2d_ch1, &img_info, enc_rgb565_128x128, csk_mirror_hor);
}

static void DMA2D_Image_Mirror_RGB565_128x128_Vert_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image mirror [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_rgb565;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Mirror_Test(dma_2d_ch1, &img_info, enc_rgb565_128x128, csk_mirror_vert);
}

static void DMA2D_Image_Mirror_Y8_128x128_Hor_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image mirror [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_y8;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Mirror_Test(dma_2d_ch1, &img_info, enc_gray_128x128, csk_mirror_hor);
}

static void DMA2D_Image_Mirror_Y8_128x128_Vert_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    csk_image_info_t img_info;
    memset(&img_info, 0, sizeof(img_info));

    CLOGD("[%s][%d]image mirror [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in);

    img_info.img_width = img_width_in;
    img_info.img_height = img_height_in;
    img_info.img_format = csk_image_format_y8;
    img_info.img_line_stride = img_info.img_width*dma2d_pixel_bitw(img_info.img_format);
    return DMA2D_Image_Mirror_Test(dma_2d_ch1, &img_info, enc_gray_128x128, csk_mirror_vert);
}

static void DMA2D_Image_Scaler_Test(csk_dma2d_ch_t chn, csk_image_info_t *img_info_in, csk_image_info_t *img_info_out, uint8_t *img_buff)
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t blk_len = 0;
    uint8_t *out_image_buf = NULL;
    csk_dma2d_init_t dma2d_para;
    memset(&dma2d_para, 0, sizeof(dma2d_para));

    dma2d_para.dma_ch = chn;
    dma2d_para.tfr_mode = tfr_mode_m2m;
    dma2d_para.src_basic_unit = dma2d_sample_unit_word;
    dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
    dma2d_para.src_inc_mode = inc_mode_increase;
    dma2d_para.dst_inc_mode = inc_mode_increase;
    dma2d_para.src_burst_len = dma2d_burst_len_8spl;
    dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.handshake = hs_none;

    csk_dma_2d_image_cfg_t dma2d_img_cfg;
    memset(&dma2d_img_cfg, 0, sizeof(dma2d_img_cfg));

    dma2d_img_cfg.img_input = *img_info_in;
    dma2d_img_cfg.img_output = *img_info_out;
    dma2d_img_cfg.img_scaler_en = csk_func_enable;

    // Malloc Buffer
    blk_len = dma2d_img_cfg.img_output.img_width * dma2d_img_cfg.img_output.img_height * dma2d_pixel_bitw(dma2d_img_cfg.img_output.img_format);
    if (NULL == (out_image_buf = (uint8_t*)malloc(blk_len)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }
    memset(out_image_buf, 0, blk_len);

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Image_Config_Extend(chn, &dma2d_img_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Start_Normal(chn, img_buff, out_image_buf, blk_len);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    while(!dma2d_image_event);
    dma2d_image_event = 0;

    ret = DMA2D_Stop(chn);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    CLOGD("[%s][%d]chn=%d source:0x%x -> destination:0x%x scaler success!!!", __FUNCTION__, __LINE__, chn, img_buff, out_image_buf);

    free(out_image_buf);

    return;
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_128x128_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 128;
    uint16_t img_height_out = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_128x256_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 128;
    uint16_t img_height_out = 256;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_128x200_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 128;
    uint16_t img_height_out = 200;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_128x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 128;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_128x100_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 128;
    uint16_t img_height_out = 100;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_128x50_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 128;
    uint16_t img_height_out = 50;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_256x128_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_200x128_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 200;
    uint16_t img_height_out = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_256x256_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 256;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_200x256_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 200;
    uint16_t img_height_out = 256;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_256x200_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 200;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_200x200_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 200;
    uint16_t img_height_out = 200;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_200x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 200;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_256x100_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 100;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_256x50_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 50;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_200x100_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 200;
    uint16_t img_height_out = 100;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_200x50_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 200;
    uint16_t img_height_out = 50;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_64x128_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 64;
    uint16_t img_height_out = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_100x128_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 100;
    uint16_t img_height_out = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_50x128_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 50;
    uint16_t img_height_out = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_64x256_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 64;
    uint16_t img_height_out = 256;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_100x256_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 100;
    uint16_t img_height_out = 256;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_50x256_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 50;
    uint16_t img_height_out = 256;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_64x200_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 64;
    uint16_t img_height_out = 200;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_100x200_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 100;
    uint16_t img_height_out = 200;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_50x200_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 50;
    uint16_t img_height_out = 200;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_64x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 64;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_100x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 100;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_50x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 50;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_64x100_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 64;
    uint16_t img_height_out = 100;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_64x50_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 64;
    uint16_t img_height_out = 50;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_100x100_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 100;
    uint16_t img_height_out = 100;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_50x50_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 50;
    uint16_t img_height_out = 50;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV422_128x128_To_YUV422_128x128_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 128;
    uint16_t img_height_out = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv422_128x128);
}

static void DMA2D_Image_Scaler_RGB888_128x128_To_RGB888_128x128_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 128;
    uint16_t img_height_out = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_rgb888_128x128);
}

static void DMA2D_Image_Scaler_RGB565_128x128_To_RGB565_128x128_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 128;
    uint16_t img_height_out = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb565;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_rgb565;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_rgb565_128x128);
}

static void DMA2D_Image_Scaler_Y8_128x128_To_Y8_128x128_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 128;
    uint16_t img_height_out = 128;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_y8;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_y8;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_gray_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV422_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_YUV422_256x50_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 50;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_RGB888_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV444_128x128_To_RGB565_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv444_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_rgb565;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv444_128x128);
}

static void DMA2D_Image_Scaler_YUV422_128x128_To_YUV444_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv422_128x128);
}

static void DMA2D_Image_Scaler_YUV422_128x128_To_RGB888_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv422_128x128);
}

static void DMA2D_Image_Scaler_YUV422_128x128_To_RGB565_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_rgb565;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv422_128x128);
}

static void DMA2D_Image_Scaler_YUV422_128x128_To_Y8_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_y8;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv422_128x128);
}

static void DMA2D_Image_Scaler_RGB888_128x128_To_RGB565_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_rgb565;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_rgb888_128x128);
}

static void DMA2D_Image_Scaler_RGB888_128x128_To_YUV444_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_rgb888_128x128);
}

static void DMA2D_Image_Scaler_RGB888_128x128_To_YUV422_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_rgb888_128x128);
}

static void DMA2D_Image_Scaler_RGB888_128x128_To_Y8_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb888;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_y8;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_rgb888_128x128);
}

static void DMA2D_Image_Scaler_RGB565_128x128_To_RGB888_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb565;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_rgb565_128x128);
}

static void DMA2D_Image_Scaler_RGB565_128x128_To_YUV444_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb565;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_rgb565_128x128);
}

static void DMA2D_Image_Scaler_RGB565_128x128_To_YUV422_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_rgb565;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_rgb565_128x128);
}

static void DMA2D_Image_Scaler_Y8_128x128_To_RGB888_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_y8;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_gray_128x128);
}

static void DMA2D_Image_Scaler_Y8_128x128_To_RGB565_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_y8;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_rgb565;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_gray_128x128);
}

static void DMA2D_Image_Scaler_Y8_128x128_To_YUV444_256x64_Test()
{
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image scaler from [%u, %u] to [%u, %u]", __FUNCTION__, __LINE__, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_y8;
    img_info_in.img_line_stride = img_info_in.img_width*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_yuv444_packed;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    return DMA2D_Image_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_gray_128x128);
}

static void DMA2D_Image_Crop_Scaler_Test(csk_dma2d_ch_t chn, csk_image_info_t *img_info_in, csk_image_info_t *img_info_out, uint8_t *img_buff, uint32_t img_buff_offset)
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t blk_len = 0;
    uint8_t *out_image_buf = NULL;
    csk_dma2d_init_t dma2d_para;
    memset(&dma2d_para, 0, sizeof(dma2d_para));

    dma2d_para.dma_ch = chn;
    dma2d_para.tfr_mode = tfr_mode_m2m;
    dma2d_para.src_basic_unit = dma2d_sample_unit_word;
    dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
    dma2d_para.src_inc_mode = inc_mode_increase;
    dma2d_para.dst_inc_mode = inc_mode_increase;
    dma2d_para.src_burst_len = dma2d_burst_len_8spl;
    dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.handshake = hs_none;

    csk_dma_2d_image_cfg_t dma2d_img_cfg;
    memset(&dma2d_img_cfg, 0, sizeof(dma2d_img_cfg));

    dma2d_img_cfg.img_input = *img_info_in;
    dma2d_img_cfg.img_output = *img_info_out;
    dma2d_img_cfg.img_crop_en = csk_func_enable;
    dma2d_img_cfg.img_scaler_en = csk_func_enable;

    // Malloc Buffer
    blk_len = dma2d_img_cfg.img_output.img_width * dma2d_img_cfg.img_output.img_height * dma2d_pixel_bitw(dma2d_img_cfg.img_output.img_format);
    if (NULL == (out_image_buf = (uint8_t*)malloc(blk_len)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }
    memset(out_image_buf, 0, blk_len);

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Image_Config_Extend(chn, &dma2d_img_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Start_Normal(chn, img_buff+img_buff_offset, out_image_buf, blk_len);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    while(!dma2d_image_event);
    dma2d_image_event = 0;

    ret = DMA2D_Stop(chn);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    CLOGD("[%s][%d]chn=%d source:0x%x -> destination:0x%x crop-scaler success!!!", __FUNCTION__, __LINE__, chn, img_buff, out_image_buf);

    free(out_image_buf);

    return;
}

static void DMA2D_Image_Scaler_YUV422_160x120_Crop_128x96_To_RGB888_256x64_Test()
{
    uint32_t img_buff_offset = 0;
    uint16_t img_width_in_ori = 160;
    uint16_t img_height_in_ori = 120;
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 96;
    uint16_t img_width_out = 256;
    uint16_t img_height_out = 64;
    csk_image_info_t img_info_in;
    csk_image_info_t img_info_out;
    memset(&img_info_in, 0, sizeof(img_info_in));
    memset(&img_info_out, 0, sizeof(img_info_out));

    CLOGD("[%s][%d]image crop from [%u, %u] to [%u, %u] scaler to [%u, %u]", __FUNCTION__, __LINE__, img_width_in_ori, img_height_in_ori, img_width_in, img_height_in, img_width_out, img_height_out);

    img_info_in.img_width = img_width_in;
    img_info_in.img_height = img_height_in;
    img_info_in.img_format = csk_image_format_yuv422_yuyv_packed;
    img_info_in.img_line_stride = img_width_in_ori*dma2d_pixel_bitw(img_info_in.img_format);
    img_info_out.img_width = img_width_out;
    img_info_out.img_height = img_height_out;
    img_info_out.img_format = csk_image_format_rgb888;
    img_info_out.img_line_stride = img_info_out.img_width*dma2d_pixel_bitw(img_info_out.img_format);
    img_buff_offset = (((img_height_in_ori-img_height_in)/2)*img_width_in_ori+(img_width_in_ori-img_width_in)/2)*dma2d_pixel_bitw(img_info_in.img_format);
    return DMA2D_Image_Crop_Scaler_Test(dma_2d_ch0, &img_info_in, &img_info_out, enc_yuv422_160x120, img_buff_offset);
}

static void DMA2D_Work_Register_Dump(csk_dma2d_ch_t chn)
{
    IP_GPDMA2D->REG_DMA_DIAG_SEL.bit.CFG_DIAG_CH_SEL = chn;

    CLOGD("DMA_CH_WK_REG0_RPT%d                  *0x%08x=0x%08x", chn, (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG0_RPT_00.all + chn, *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG0_RPT_00.all + chn));
    CLOGD("DMA_CH_WK_REG1_RPT%d                  *0x%08x=0x%08x", chn, (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG1_RPT_00.all + chn, *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG1_RPT_00.all + chn));
    CLOGD("DMA_CH_WK_REG2_RPT%d                  *0x%08x=0x%08x", chn, (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG2_RPT_00.all + chn, *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG2_RPT_00.all + chn));
    CLOGD("DMA_CH_WK_REG3_RPT%d                  *0x%08x=0x%08x", chn, (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG3_RPT_00.all + chn, *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG3_RPT_00.all + chn));
    CLOGD("DMA_CH_WK_REG4_RPT%d                  *0x%08x=0x%08x", chn, (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG4_RPT_00.all + chn, *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG4_RPT_00.all + chn));
    CLOGD("DMA_CH_WK_REG5_RPT%d                  *0x%08x=0x%08x", chn, (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG5_RPT_00.all + chn, *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG5_RPT_00.all + chn));
    CLOGD("DMA_CH_WK_REG6_RPT%d                  *0x%08x=0x%08x", chn, (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG6_RPT_00.all + chn, *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG6_RPT_00.all + chn));
    CLOGD("DMA_CH_WK_REG7_RPT%d                  *0x%08x=0x%08x", chn, (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG7_RPT_00.all + chn, *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG7_RPT_00.all + chn));
    CLOGD("DMA_CH_WK_REG8_RPT%d                  *0x%08x=0x%08x", chn, (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG8_RPT_00.all + chn, *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG8_RPT_00.all + chn));
    CLOGD("DMA_CH_WK_REG9_RPT%d                  *0x%08x=0x%08x", chn, (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG9_RPT_00.all + chn, *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG9_RPT_00.all + chn));
    CLOGD("DMA_SCALER_WK_RPT                     *0x%08x=0x%08x", &IP_GPDMA2D->REG_DMA_SCALER_WK_RPT.all, IP_GPDMA2D->REG_DMA_SCALER_WK_RPT.all);
    CLOGD("DMA_DIAG_RPT                          *0x%08x=0x%08x", &IP_GPDMA2D->REG_DMA_DIAG_RPT.all, IP_GPDMA2D->REG_DMA_DIAG_RPT.all);

    return;
}

static void DMA2D_Work_Register_Check_SGather_Test()
{
    uint32_t i = 0;
    uint32_t j = 0;
    int32_t ret = CSK_DRIVER_OK;
    csk_dma2d_ch_t chn = dma_2d_ch0;
    uint8_t *dma2d_src_buffer = (uint8_t*)0x38000000;
    uint8_t *dma2d_dst_buffer = NULL;

    for (chn = dma_2d_ch0; chn <= dma_2d_ch5; chn++)
    {
        // Malloc buffer
        if (NULL == (dma2d_dst_buffer = (uint8_t*)malloc(DMA2D_TEST_DATA_LENGTH*DMA2D_TEST_DATA_CNT)))
        {
            CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
            return;
        }
        memset(dma2d_dst_buffer, 0, DMA2D_TEST_DATA_LENGTH*DMA2D_TEST_DATA_CNT);

        for (i = 0; i < DMA2D_TEST_DATA_CNT; i++)
        {
            for (j = 0; j < DMA2D_TEST_DATA_LENGTH; j++)
            {
                *(dma2d_src_buffer+i*2*DMA2D_TEST_DATA_LENGTH+j) = (i*j*(chn+1))&0xFF;
            }
        }

        csk_dma2d_init_t dma2d_para;
        memset(&dma2d_para, 0, sizeof(dma2d_para));

        dma2d_para.dma_ch = chn;
        dma2d_para.tfr_mode = tfr_mode_m2m;
        dma2d_para.src_basic_unit = dma2d_sample_unit_byte;
        dma2d_para.dst_basic_unit = dma2d_sample_unit_byte;
        dma2d_para.src_inc_mode = inc_mode_increase;
        dma2d_para.dst_inc_mode = inc_mode_increase;
        dma2d_para.src_burst_len = dma2d_burst_len_1spl;
        dma2d_para.dst_burst_len = dma2d_burst_len_1spl;
        dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
        dma2d_para.prio_lvl = prio_mode_vhigh;
        dma2d_para.handshake = hs_none;
        dma2d_para.src_gather.enable = csk_func_enable;
        dma2d_para.src_gather.interval = DMA2D_TEST_DATA_LENGTH;
        dma2d_para.src_gather.counter = DMA2D_TEST_DATA_LENGTH;

        ret = DMA2D_Initialize();
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        CLOGD("--------------------init------------------------");
        DMA2D_Work_Register_Dump(chn);

        ret = DMA2D_Config(&dma2d_para, NULL, NULL);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        ret = DMA2D_Start_Normal(chn, (void *)dma2d_src_buffer, (void *)dma2d_dst_buffer, DMA2D_TEST_DATA_LENGTH*DMA2D_TEST_DATA_CNT);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        CLOGD("--------------------config------------------------");
        DMA2D_Work_Register_Dump(chn);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG0_RPT_00.all + chn), (uint32_t)dma2d_src_buffer);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG1_RPT_00.all + chn), (uint32_t)dma2d_dst_buffer);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG2_RPT_00.all + chn), dma2d_para.src_gather.interval&0xFFFFFF);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG3_RPT_00.all + chn), ((dma2d_para.src_gather.enable&0x1)<<31)|(dma2d_para.src_gather.counter&0xFFFFFF));
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG4_RPT_00.all + chn), dma2d_para.dst_scatter.interval&0xFFFFFF);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG5_RPT_00.all + chn), ((dma2d_para.dst_scatter.enable&0x1)<<31)|(dma2d_para.dst_scatter.counter&0xFFFFFF));
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG6_RPT_00.all + chn), (0x1<<31)|((DMA2D_TEST_DATA_LENGTH*DMA2D_TEST_DATA_CNT)&0xFFFFFF));
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG7_RPT_00.all + chn), ((dma2d_para.src_inc_mode&0x3)<<24)|((dma2d_para.src_burst_len&0x3)<<22));
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG8_RPT_00.all + chn), ((dma2d_para.dst_inc_mode&0x3)<<24)|((dma2d_para.dst_burst_len&0x3)<<22));
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG9_RPT_00.all + chn), 0x00000000);

        ret = DMA2D_Stop(chn);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        IP_SYSCTRL->REG_SW_RESET_CFG2.bit.GPDMA2D_RESET = 1;

        CLOGD("--------------------reset------------------------");
        DMA2D_Work_Register_Dump(chn);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG0_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG1_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG2_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG3_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG4_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG5_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG6_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG7_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG8_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG9_RPT_00.all + chn), 0x00000000);

        // Free
        free(dma2d_dst_buffer);
    }
    return;
}

static void DMA2D_Work_Register_Check_DSatter_Test()
{
    uint32_t i = 0;
    uint32_t j = 0;
    int32_t ret = CSK_DRIVER_OK;
    csk_dma2d_ch_t chn = dma_2d_ch0;
    uint8_t *dma2d_src_buffer = NULL;
    uint8_t *dma2d_dst_buffer = (uint8_t*)0x38000000;

    for (chn = dma_2d_ch0; chn <= dma_2d_ch5; chn++)
    {
        // Malloc buffer
        if (NULL == (dma2d_src_buffer = (uint8_t*)malloc(DMA2D_TEST_DATA_LENGTH*DMA2D_TEST_DATA_CNT)))
        {
            CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
            return;
        }
        for (i = 0; i < DMA2D_TEST_DATA_CNT; i++)
        {
            for (j = 0; j < DMA2D_TEST_DATA_LENGTH; j++)
            {
                *(dma2d_src_buffer+j) = (i*j*(chn+1))&0xFF;
            }
        }
        memset(dma2d_dst_buffer, 0, DMA2D_TEST_DATA_LENGTH*DMA2D_TEST_DATA_CNT*2);

        csk_dma2d_init_t dma2d_para;
        memset(&dma2d_para, 0, sizeof(dma2d_para));

        dma2d_para.dma_ch = chn;
        dma2d_para.tfr_mode = tfr_mode_m2m;
        dma2d_para.src_basic_unit = dma2d_sample_unit_word;
        dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
        dma2d_para.src_inc_mode = inc_mode_increase;
        dma2d_para.dst_inc_mode = inc_mode_increase;
        dma2d_para.src_burst_len = dma2d_burst_len_8spl;
        dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
        dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
        dma2d_para.prio_lvl = prio_mode_vhigh;
        dma2d_para.handshake = hs_none;
        dma2d_para.dst_scatter.enable = csk_func_enable;
        dma2d_para.dst_scatter.interval = DMA2D_TEST_DATA_LENGTH;
        dma2d_para.dst_scatter.counter = DMA2D_TEST_DATA_LENGTH;

        ret = DMA2D_Initialize();
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        CLOGD("--------------------init------------------------");
        DMA2D_Work_Register_Dump(chn);

        ret = DMA2D_Config(&dma2d_para, NULL, NULL);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        ret = DMA2D_Start_Normal(chn, (void *)dma2d_src_buffer, (void *)dma2d_dst_buffer, DMA2D_TEST_DATA_LENGTH*DMA2D_TEST_DATA_CNT);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        CLOGD("--------------------config------------------------");
        DMA2D_Work_Register_Dump(chn);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG0_RPT_00.all + chn), (uint32_t)dma2d_src_buffer);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG1_RPT_00.all + chn), (uint32_t)dma2d_dst_buffer);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG2_RPT_00.all + chn), dma2d_para.src_gather.interval&0xFFFFFF);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG3_RPT_00.all + chn), ((dma2d_para.src_gather.enable&0x1)<<31)|(dma2d_para.src_gather.counter&0xFFFFFF));
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG4_RPT_00.all + chn), dma2d_para.dst_scatter.interval&0xFFFFFF);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG5_RPT_00.all + chn), ((dma2d_para.dst_scatter.enable&0x1)<<31)|(dma2d_para.dst_scatter.counter&0xFFFFFF));
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG6_RPT_00.all + chn), (0x1<<31)|((DMA2D_TEST_DATA_LENGTH*DMA2D_TEST_DATA_CNT)&0xFFFFFF));
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG7_RPT_00.all + chn), ((dma2d_para.src_inc_mode&0x3)<<24)|((dma2d_para.src_burst_len&0x3)<<22));
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG8_RPT_00.all + chn), ((dma2d_para.dst_inc_mode&0x3)<<24)|((dma2d_para.dst_burst_len&0x3)<<22));
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG9_RPT_00.all + chn), 0x00000000);

        ret = DMA2D_Stop(chn);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        IP_SYSCTRL->REG_SW_RESET_CFG2.bit.GPDMA2D_RESET = 1;

        CLOGD("--------------------reset------------------------");
        DMA2D_Work_Register_Dump(chn);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG0_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG1_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG2_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG3_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG4_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG5_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG6_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG7_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG8_RPT_00.all + chn), 0x00000000);
        CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG9_RPT_00.all + chn), 0x00000000);

        // Free
        free(dma2d_src_buffer);
    }
    return;
}

static void DMA2D_Work_Register_Check_Image_Test()
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t blk_len = 0;
    csk_dma2d_ch_t chn = dma_2d_ch0;
    uint8_t *out_image_buf = NULL;
    csk_dma2d_init_t dma2d_para;
    memset(&dma2d_para, 0, sizeof(dma2d_para));

    dma2d_para.dma_ch = chn;
    dma2d_para.tfr_mode = tfr_mode_m2m;
    dma2d_para.src_basic_unit = dma2d_sample_unit_word;
    dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
    dma2d_para.src_inc_mode = inc_mode_increase;
    dma2d_para.dst_inc_mode = inc_mode_increase;
    dma2d_para.src_burst_len = dma2d_burst_len_8spl;
    dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.handshake = hs_none;

    csk_dma_2d_image_cfg_t dma2d_img_cfg;
    memset(&dma2d_img_cfg, 0, sizeof(dma2d_img_cfg));

    dma2d_img_cfg.img_input.img_width = 128;
    dma2d_img_cfg.img_input.img_height = 128;
    dma2d_img_cfg.img_input.img_format = csk_image_format_yuv444_packed;
    dma2d_img_cfg.img_input.img_line_stride = dma2d_img_cfg.img_input.img_width * dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format);
    dma2d_img_cfg.img_output.img_width = dma2d_img_cfg.img_input.img_width;
    dma2d_img_cfg.img_output.img_height = dma2d_img_cfg.img_input.img_height;
    dma2d_img_cfg.img_output.img_format = csk_image_format_yuv422_yuyv_packed;
    dma2d_img_cfg.img_output.img_line_stride = dma2d_img_cfg.img_output.img_width * dma2d_pixel_bitw(dma2d_img_cfg.img_output.img_format);

    // Malloc Buffer
    blk_len = dma2d_img_cfg.img_output.img_width * dma2d_img_cfg.img_output.img_height * dma2d_pixel_bitw(dma2d_img_cfg.img_output.img_format);
    if (NULL == (out_image_buf = malloc(blk_len)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }
    memset(out_image_buf, 0, blk_len);

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    CLOGD("--------------------init------------------------");
    DMA2D_Work_Register_Dump(chn);

    ret = DMA2D_Config(&dma2d_para, NULL, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Image_Config_Extend(chn, &dma2d_img_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Start_Normal(chn, enc_yuv444_128x128, out_image_buf, blk_len);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    CLOGD("--------------------config------------------------");
    DMA2D_Work_Register_Dump(chn);
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG0_RPT_00.all + chn), (uint32_t)enc_yuv444_128x128);
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG1_RPT_00.all + chn), (uint32_t)out_image_buf);
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG2_RPT_00.all + chn), dma2d_para.src_gather.interval&0xFFFFFF);
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG3_RPT_00.all + chn), ((dma2d_para.src_gather.enable&0x1)<<31)|(dma2d_para.src_gather.counter&0xFFFFFF));
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG4_RPT_00.all + chn), dma2d_para.dst_scatter.interval&0xFFFFFF);
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG5_RPT_00.all + chn), ((dma2d_para.dst_scatter.enable&0x1)<<31)|(dma2d_para.dst_scatter.counter&0xFFFFFF));
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG6_RPT_00.all + chn), (0x1<<31)|(blk_len&0xFFFFFF));
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG7_RPT_00.all + chn), ((dma2d_para.src_inc_mode&0x3)<<24)|((dma2d_para.src_burst_len&0x3)<<22)|((dma2d_img_cfg.img_input.img_height&0x7FF)<<11)|(dma2d_img_cfg.img_input.img_width&0x7FF));
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG8_RPT_00.all + chn), ((dma2d_para.dst_inc_mode&0x3)<<24)|((dma2d_para.dst_burst_len&0x3)<<22)|((dma2d_img_cfg.img_output.img_height&0x7FF)<<11)|(dma2d_img_cfg.img_output.img_width&0x7FF));
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG9_RPT_00.all + chn), (0x1<<31)|((dma2d_img_cfg.img_output.img_line_stride&0x3FFF)<<14)|(dma2d_img_cfg.img_input.img_line_stride&0x3FFF));

    ret = DMA2D_Stop(chn);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.GPDMA2D_RESET = 1;

    CLOGD("--------------------reset------------------------");
    DMA2D_Work_Register_Dump(chn);
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG0_RPT_00.all + chn), 0x00000000);
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG1_RPT_00.all + chn), 0x00000000);
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG2_RPT_00.all + chn), 0x00000000);
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG3_RPT_00.all + chn), 0x00000000);
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG4_RPT_00.all + chn), 0x00000000);
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG5_RPT_00.all + chn), 0x00000000);
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG6_RPT_00.all + chn), 0x00000000);
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG7_RPT_00.all + chn), 0x00000000);
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG8_RPT_00.all + chn), 0x00000000);
    CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG9_RPT_00.all + chn), 0x00000000);

    free(out_image_buf);

    return;
}

static void DMA2D_UserDef_Line_IRQ_Test()
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t blk_len = 0;
    csk_dma2d_ch_t chn = dma_2d_ch0;
    uint16_t img_width_in = 128;
    uint16_t img_height_in = 128;
    uint16_t img_width_out = 64;
    uint16_t img_height_out = 64;
    uint8_t *out_image_buf = NULL;
    uint32_t img_buff_offset = 0;
    csk_dma_2d_image_cfg_t dma2d_img_cfg;
    memset(&dma2d_img_cfg, 0, sizeof(dma2d_img_cfg));

    dma2d_img_cfg.img_input.img_width = img_width_out;
    dma2d_img_cfg.img_input.img_height = img_height_out;
    dma2d_img_cfg.img_input.img_format = csk_image_format_yuv444_packed;
    dma2d_img_cfg.img_input.img_line_stride = img_width_in*dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format);
    dma2d_img_cfg.img_output.img_width = dma2d_img_cfg.img_input.img_width;
    dma2d_img_cfg.img_output.img_height = dma2d_img_cfg.img_input.img_height;
    dma2d_img_cfg.img_output.img_format = dma2d_img_cfg.img_input.img_format;
    dma2d_img_cfg.img_output.img_line_stride = dma2d_img_cfg.img_output.img_width * dma2d_pixel_bitw(dma2d_img_cfg.img_output.img_format);
    dma2d_img_cfg.img_crop_en = csk_func_enable;
    img_buff_offset = (((img_height_in-img_height_out)/2)*img_width_in+(img_width_in-img_width_out)/2)*dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format);

    // Malloc Buffer
    blk_len = dma2d_img_cfg.img_input.img_width * dma2d_img_cfg.img_input.img_height * dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format);
    if (NULL == (out_image_buf = malloc(blk_len)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }

    for (chn = dma_2d_ch0; chn <= dma_2d_ch5; chn++)
    {
        csk_dma2d_init_t dma2d_para;
        memset(&dma2d_para, 0, sizeof(dma2d_para));

        dma2d_para.dma_ch = chn;
        dma2d_para.tfr_mode = tfr_mode_m2m;
        dma2d_para.src_basic_unit = dma2d_sample_unit_word;
        dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
        dma2d_para.src_inc_mode = inc_mode_increase;
        dma2d_para.dst_inc_mode = inc_mode_increase;
        dma2d_para.src_burst_len = dma2d_burst_len_8spl;
        dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
        dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
        dma2d_para.prio_lvl = prio_mode_vhigh;
        dma2d_para.handshake = hs_none;

        memset(out_image_buf, 0, blk_len);

        ret = DMA2D_Initialize();
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        *((uint32_t*)&IP_GPDMA2D->REG_DMA_BLOCK_IRQ_LINE_NUM_00.all + chn) = img_height_out;
        *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_IRQ_MASK_00.all + chn) &= 0x1D;

        ret = DMA2D_Image_Config_Extend(chn, &dma2d_img_cfg);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        ret = DMA2D_Start_Normal(chn, enc_yuv444_128x128+img_buff_offset, out_image_buf, blk_len);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        while(!dma2d_image_event);
        dma2d_image_event = 0;

        CLOGD("[%s][%d]chn=%d raw_status=%#x status=%#x mask=%#x !!!", __FUNCTION__, __LINE__, chn, *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_IRQ_RAW_STATUS_00.all + chn), *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_IRQ_STATUS_00.all + chn), *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_IRQ_MASK_00.all + chn));
        //CHECK_RET_EQ(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_IRQ_RAW_STATUS_00.all + chn), 0x16);

        ret = DMA2D_Stop(chn);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    }

    free(out_image_buf);

    return;
}

static void DMA2D_Trigger_Display_Test(csk_dma2d_ch_t src_chn, csk_dma2d_ch_t dst_chn, uint8_t *img_buff)
{
    int32_t ret = CSK_DRIVER_OK;
    uint8_t *ring_buffer = NULL;
    uint32_t ring_buffer_len = 0;
    uint32_t blk_len_in = 0;
    uint32_t blk_len_out = 0;
    csk_dma2d_init_t dma2d_para;
    memset(&dma2d_para, 0, sizeof(dma2d_para));

    dma2d_para.dma_ch = src_chn;
    dma2d_para.tfr_mode = tfr_mode_m2m;
    dma2d_para.src_basic_unit = dma2d_sample_unit_word;
    dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
    dma2d_para.src_inc_mode = inc_mode_increase;
    dma2d_para.dst_inc_mode = inc_mode_increase;
    dma2d_para.src_burst_len = dma2d_burst_len_8spl;
    dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.handshake = hs_none;
    dma2d_para.trigger.mode = csk_trigger_src;
    dma2d_para.trigger.triggered_en = csk_func_disable;
    dma2d_para.trigger.triggered_src_chn = dst_chn;

    csk_dma_2d_image_cfg_t dma2d_img_cfg;
    memset(&dma2d_img_cfg, 0, sizeof(dma2d_img_cfg));

    dma2d_img_cfg.img_input.img_width = DMA2D_QSPI_OUT_IMAGE_SIZE_W;
    dma2d_img_cfg.img_input.img_height = DMA2D_QSPI_OUT_IMAGE_SIZE_H;
    dma2d_img_cfg.img_input.img_format = csk_image_format_rgb565;
    dma2d_img_cfg.img_input.img_line_stride = dma2d_img_cfg.img_input.img_width * dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format);
    dma2d_img_cfg.img_output.img_width = dma2d_img_cfg.img_input.img_width;
    dma2d_img_cfg.img_output.img_height = dma2d_img_cfg.img_input.img_height;
    dma2d_img_cfg.img_output.img_format = csk_image_format_rgb565;
    dma2d_img_cfg.img_output.img_line_stride = dma2d_img_cfg.img_output.img_width * dma2d_pixel_bitw(dma2d_img_cfg.img_output.img_format);

    // lcd init(include dma2d config)
    qspi_out_task_start(src_chn, dst_chn, dma2d_img_cfg.img_output.img_width, dma2d_img_cfg.img_output.img_height, dma2d_img_cfg.img_output.img_format);

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Image_Config_Extend(src_chn, &dma2d_img_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    // Malloc Buffer
    blk_len_in = dma2d_img_cfg.img_input.img_height * dma2d_img_cfg.img_input.img_line_stride;
    blk_len_out = dma2d_img_cfg.img_output.img_height * dma2d_img_cfg.img_output.img_line_stride;
    ring_buffer_len = 2 * dma2d_img_cfg.img_output.img_line_stride;
    if (NULL == (ring_buffer = malloc(ring_buffer_len)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }
    memset(ring_buffer, 0, ring_buffer_len);

    while (1)
    {
        ret = qspi_out_spd2010_image_flush_start(DMA2D_QSPI_OUT_IMAGE_START_X, DMA2D_QSPI_OUT_IMAGE_START_Y, dma2d_img_cfg.img_output.img_width, dma2d_img_cfg.img_output.img_height, ring_buffer, blk_len_out, dst_chn);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        ret = DMA2D_Start_Normal(src_chn, img_buff, ring_buffer, blk_len_in);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        while(!dma2d_image_event);
        dma2d_image_event = 0;

        ret = qspi_out_spd2010_image_flush_waitdone();
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        qspi_out_spd2010_image_flush_stop(dst_chn);

        ret = DMA2D_Stop(src_chn);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    }

    CLOGD("[%s][%d]src_chn=%d source:0x%x -> dst_chn=%d trigger success!!!", __FUNCTION__, __LINE__, src_chn, img_buff, dst_chn);

    free(ring_buffer);

    return;
}

static void DMA2D_Trigger_Display_Normal_Test()
{
    return DMA2D_Trigger_Display_Test(dma_2d_ch1, dma_2d_ch3, enc_rgb565_320x240);
}

static void DMA2D_Trigger_Display_Mirror_Test(csk_dma2d_ch_t src_chn, csk_dma2d_ch_t dst_chn, uint8_t *img_buff, csk_mirror_mode_t mirror_mode)
{
    int32_t ret = CSK_DRIVER_OK;
    uint8_t *ring_buffer = NULL;
    uint32_t ring_buffer_len = 0;
    uint32_t blk_len_in = 0;
    uint32_t blk_len_out = 0;
    csk_dma2d_init_t dma2d_para;
    memset(&dma2d_para, 0, sizeof(dma2d_para));

    dma2d_para.dma_ch = src_chn;
    dma2d_para.tfr_mode = tfr_mode_m2m;
    dma2d_para.src_basic_unit = dma2d_sample_unit_word;
    dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
    dma2d_para.src_inc_mode = inc_mode_increase;
    dma2d_para.dst_inc_mode = inc_mode_increase;
    dma2d_para.src_burst_len = dma2d_burst_len_8spl;
    dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.handshake = hs_none;
    dma2d_para.trigger.mode = csk_trigger_src;
    dma2d_para.trigger.triggered_en = csk_func_disable;
    dma2d_para.trigger.triggered_src_chn = dst_chn;

    csk_dma_2d_image_cfg_t dma2d_img_cfg;
    memset(&dma2d_img_cfg, 0, sizeof(dma2d_img_cfg));

    dma2d_img_cfg.img_input.img_width = DMA2D_QSPI_OUT_IMAGE_SIZE_W;
    dma2d_img_cfg.img_input.img_height = DMA2D_QSPI_OUT_IMAGE_SIZE_H;
    dma2d_img_cfg.img_input.img_format = csk_image_format_rgb565;
    dma2d_img_cfg.img_input.img_line_stride = dma2d_img_cfg.img_input.img_width * dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format);
    dma2d_img_cfg.img_output.img_width = dma2d_img_cfg.img_input.img_width;
    dma2d_img_cfg.img_output.img_height = dma2d_img_cfg.img_input.img_height;
    dma2d_img_cfg.img_output.img_format = csk_image_format_rgb565;
    dma2d_img_cfg.img_output.img_line_stride = dma2d_img_cfg.img_output.img_width * dma2d_pixel_bitw(dma2d_img_cfg.img_output.img_format);
    dma2d_img_cfg.img_mirror_en = csk_func_enable;
    dma2d_img_cfg.img_mirror_mode = mirror_mode;

    // lcd init(include dma2d config)
    qspi_out_task_start(src_chn, dst_chn, dma2d_img_cfg.img_output.img_width, dma2d_img_cfg.img_output.img_height, dma2d_img_cfg.img_output.img_format);

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Image_Config_Extend(src_chn, &dma2d_img_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    // Malloc Buffer
    blk_len_in = dma2d_img_cfg.img_input.img_height * dma2d_img_cfg.img_input.img_line_stride;
    blk_len_out = dma2d_img_cfg.img_output.img_height * dma2d_img_cfg.img_output.img_line_stride;
    ring_buffer_len = 2 * dma2d_img_cfg.img_output.img_line_stride;
    if (NULL == (ring_buffer = malloc(ring_buffer_len)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }
    memset(ring_buffer, 0, ring_buffer_len);

    while (1)
    {
        ret = qspi_out_spd2010_image_flush_start(DMA2D_QSPI_OUT_IMAGE_START_X, DMA2D_QSPI_OUT_IMAGE_START_Y, dma2d_img_cfg.img_output.img_width, dma2d_img_cfg.img_output.img_height, ring_buffer, blk_len_out, dst_chn);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        ret = DMA2D_Start_Normal(src_chn, img_buff, ring_buffer, blk_len_in);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        while(!dma2d_image_event);
        dma2d_image_event = 0;

        ret = qspi_out_spd2010_image_flush_waitdone();
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        qspi_out_spd2010_image_flush_stop(dst_chn);

        ret = DMA2D_Stop(src_chn);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    }

    CLOGD("[%s][%d]src_chn=%d source:0x%x -> dst_chn=%d trigger success!!!", __FUNCTION__, __LINE__, src_chn, img_buff, dst_chn);

    free(ring_buffer);

    return;
}

static void DMA2D_Trigger_Display_Mirror_Hor_Test()
{
    return DMA2D_Trigger_Display_Mirror_Test(dma_2d_ch1, dma_2d_ch3, enc_rgb565_320x240, csk_mirror_hor);
}

static void DMA2D_Trigger_Display_Mirror_Vert_Test()
{
    return DMA2D_Trigger_Display_Mirror_Test(dma_2d_ch1, dma_2d_ch3, enc_rgb565_320x240, csk_mirror_vert);
}

static void DMA2D_Trigger_Display_Rotation_Test(csk_dma2d_ch_t src_chn, csk_dma2d_ch_t dst_chn, uint8_t *img_buff, csk_rotation_mode_t rota_mode)
{
    int32_t ret = CSK_DRIVER_OK;
    uint8_t *ring_buffer = NULL;
    uint32_t ring_buffer_len = 0;
    uint32_t blk_len_in = 0;
    uint32_t blk_len_out = 0;
    csk_dma2d_init_t dma2d_para;
    memset(&dma2d_para, 0, sizeof(dma2d_para));

    dma2d_para.dma_ch = src_chn;
    dma2d_para.tfr_mode = tfr_mode_m2m;
    dma2d_para.src_basic_unit = dma2d_sample_unit_word;
    dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
    dma2d_para.src_inc_mode = inc_mode_increase;
    dma2d_para.dst_inc_mode = inc_mode_increase;
    dma2d_para.src_burst_len = dma2d_burst_len_8spl;
    dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.handshake = hs_none;
    dma2d_para.trigger.mode = csk_trigger_src;
    dma2d_para.trigger.triggered_en = csk_func_enable;
    dma2d_para.trigger.triggered_src_chn = dst_chn;

    csk_dma_2d_image_cfg_t dma2d_img_cfg;
    memset(&dma2d_img_cfg, 0, sizeof(dma2d_img_cfg));

    dma2d_img_cfg.img_input.img_width = DMA2D_QSPI_OUT_IMAGE_SIZE_W;
    dma2d_img_cfg.img_input.img_height = DMA2D_QSPI_OUT_IMAGE_SIZE_H;
    dma2d_img_cfg.img_input.img_format = csk_image_format_rgb565;
    dma2d_img_cfg.img_input.img_line_stride = dma2d_img_cfg.img_input.img_width * dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format);
    if (csk_rota_cw180 == rota_mode)
    {
        dma2d_img_cfg.img_output.img_width = dma2d_img_cfg.img_input.img_width;
        dma2d_img_cfg.img_output.img_height = dma2d_img_cfg.img_input.img_height;
    }
    else
    {
        dma2d_img_cfg.img_output.img_width = dma2d_img_cfg.img_input.img_height;
        dma2d_img_cfg.img_output.img_height = dma2d_img_cfg.img_input.img_width;
    }
    dma2d_img_cfg.img_output.img_format = csk_image_format_rgb565;
    dma2d_img_cfg.img_output.img_line_stride = dma2d_img_cfg.img_output.img_width * dma2d_pixel_bitw(dma2d_img_cfg.img_output.img_format);
    dma2d_img_cfg.img_rota_en = csk_func_enable;
    dma2d_img_cfg.img_rota_tile_en = csk_func_enable;
    dma2d_img_cfg.img_rota_mode = rota_mode;

    // lcd init(include dma2d config)
    qspi_out_task_start(src_chn, dst_chn, dma2d_img_cfg.img_output.img_width, dma2d_img_cfg.img_output.img_height, dma2d_img_cfg.img_output.img_format);

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Image_Config_Extend(src_chn, &dma2d_img_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    // Malloc Buffer
    blk_len_in = dma2d_img_cfg.img_input.img_height * dma2d_img_cfg.img_input.img_line_stride;
    blk_len_out = dma2d_img_cfg.img_output.img_height * dma2d_img_cfg.img_output.img_line_stride;
    ring_buffer_len = 16 * dma2d_img_cfg.img_output.img_line_stride;
    if (NULL == (ring_buffer = malloc(ring_buffer_len)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }
    memset(ring_buffer, 0, ring_buffer_len);

    while (1)
    {
        ret = qspi_out_spd2010_image_flush_start(DMA2D_QSPI_OUT_IMAGE_START_X, DMA2D_QSPI_OUT_IMAGE_START_Y, dma2d_img_cfg.img_output.img_width, dma2d_img_cfg.img_output.img_height, ring_buffer, blk_len_out, dst_chn);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        ret = DMA2D_Start_Normal(src_chn, img_buff, ring_buffer, blk_len_in);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        while(!dma2d_image_event);
        dma2d_image_event = 0;

        ret = qspi_out_spd2010_image_flush_waitdone();
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        qspi_out_spd2010_image_flush_stop(dst_chn);

        ret = DMA2D_Stop(src_chn);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    }

    CLOGD("[%s][%d]src_chn=%d source:0x%x -> dst_chn=%d trigger success!!!", __FUNCTION__, __LINE__, src_chn, img_buff, dst_chn);

    free(ring_buffer);

    return;
}

static void DMA2D_Trigger_Display_Rotation_CW90_Test()
{
    return DMA2D_Trigger_Display_Rotation_Test(dma_2d_ch1, dma_2d_ch3, enc_rgb565_320x240, csk_rota_cw90);
}

static void DMA2D_Trigger_Display_Rotation_CCW90_Test()
{
    return DMA2D_Trigger_Display_Rotation_Test(dma_2d_ch1, dma_2d_ch3, enc_rgb565_320x240, csk_rota_ccw90);
}

static void DMA2D_Trigger_Display_Rotation_CW180_Test()
{
    return DMA2D_Trigger_Display_Rotation_Test(dma_2d_ch1, dma_2d_ch3, enc_rgb565_320x240, csk_rota_cw180);
}

static void DMA2D_Trigger_Display_Rotation_Transpose_Test()
{
    return DMA2D_Trigger_Display_Rotation_Test(dma_2d_ch1, dma_2d_ch3, enc_rgb565_320x240, csk_rota_transpose);
}

static void DMA2D_Trigger_Display_Camera_Test(csk_dma2d_ch_t src_chn, csk_dma2d_ch_t dst_chn)
{
    int32_t ret = CSK_DRIVER_OK;
    uint8_t *ring_buffer = NULL;
    uint32_t ring_buffer_len = 0;
    uint32_t blk_len_in = 0;
    uint32_t blk_len_out = 0;
    csk_dma2d_init_t dma2d_para;
    memset(&dma2d_para, 0, sizeof(dma2d_para));

    dma2d_para.dma_ch = src_chn;
    dma2d_para.tfr_mode = tfr_mode_p2m;
    dma2d_para.src_basic_unit = dma2d_sample_unit_word;
    dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
    dma2d_para.src_inc_mode = inc_mode_fix;
    dma2d_para.dst_inc_mode = inc_mode_increase;
    dma2d_para.src_burst_len = dma2d_burst_len_8spl;
    dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.handshake = dvp_hs_num3;
    dma2d_para.trigger.mode = csk_trigger_src;
    dma2d_para.trigger.triggered_en = csk_func_disable;
    dma2d_para.trigger.triggered_src_chn = dst_chn;

    csk_dma_2d_image_cfg_t dma2d_img_cfg;
    memset(&dma2d_img_cfg, 0, sizeof(dma2d_img_cfg));

    dma2d_img_cfg.img_input.img_width = DMA2D_QSPI_OUT_IMAGE_SIZE_W;
    dma2d_img_cfg.img_input.img_height = DMA2D_QSPI_OUT_IMAGE_SIZE_H;
    dma2d_img_cfg.img_input.img_format = csk_image_format_yuv422_yuyv_packed;
    dma2d_img_cfg.img_input.img_line_stride = dma2d_img_cfg.img_input.img_width * dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format);
    dma2d_img_cfg.img_output.img_width = dma2d_img_cfg.img_input.img_width;
    dma2d_img_cfg.img_output.img_height = dma2d_img_cfg.img_input.img_height;
    dma2d_img_cfg.img_output.img_format = csk_image_format_yuv422_yuyv_packed;
    dma2d_img_cfg.img_output.img_line_stride = dma2d_img_cfg.img_output.img_width * dma2d_pixel_bitw(dma2d_img_cfg.img_output.img_format);

    // dvp camera init
    dvp_in_start(DMA2D_QSPI_OUT_IMAGE_SIZE_W, DMA2D_QSPI_OUT_IMAGE_SIZE_H);

    // lcd init(include dma2d config)
    qspi_out_task_start(src_chn, dst_chn, dma2d_img_cfg.img_output.img_width, dma2d_img_cfg.img_output.img_height, dma2d_img_cfg.img_output.img_format);

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Image_Config_Extend(src_chn, &dma2d_img_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    // Malloc Buffer
    blk_len_in = dma2d_img_cfg.img_input.img_height * dma2d_img_cfg.img_input.img_line_stride;
    blk_len_out = dma2d_img_cfg.img_output.img_height * dma2d_img_cfg.img_output.img_line_stride;
    ring_buffer_len = 2 * dma2d_img_cfg.img_output.img_line_stride;
    if (NULL == (ring_buffer = malloc(ring_buffer_len)))
    {
        CLOGE("[%s][%d]Failed to allocate memory", __FUNCTION__, __LINE__);
        return;
    }
    memset(ring_buffer, 0, ring_buffer_len);

    while (1)
    {
        ret = qspi_out_spd2010_image_flush_start(DMA2D_QSPI_OUT_IMAGE_START_X, DMA2D_QSPI_OUT_IMAGE_START_Y, dma2d_img_cfg.img_output.img_width, dma2d_img_cfg.img_output.img_height, ring_buffer, blk_len_out, dst_chn);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        ret = DMA2D_Start_Normal(src_chn, (void*)DVP0_Buf(), ring_buffer, blk_len_in);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        while(!dma2d_image_event);
        dma2d_image_event = 0;

        ret = qspi_out_spd2010_image_flush_waitdone();
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);

        qspi_out_spd2010_image_flush_stop(dst_chn);

        ret = DMA2D_Stop(src_chn);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    }

    CLOGD("[%s][%d]src_chn=%d source:0x%x -> dst_chn=%d trigger success!!!", __FUNCTION__, __LINE__, src_chn, (void*)DVP0_Buf(), dst_chn);

    free(ring_buffer);

    return;
}

static void DMA2D_Trigger_Display_Camera_Online_Test()
{
    return DMA2D_Trigger_Display_Camera_Test(dma_2d_ch1, dma_2d_ch3);
}

static function test_function_array[] = {
/* one dimensional */
//    DMA2D_NormalMode_M2m_Word_Burst1_1K_Test,
//    DMA2D_NormalMode_M2m_Word_Burst4_1K_Test,
//    DMA2D_NormalMode_M2m_Word_Burst8_1K_Test,
//    DMA2D_NormalMode_M2m_Word_Burst16_1K_Test,
//    DMA2D_NormalMode_M2m_HalfWord_Burst1_1K_Test,
//    DMA2D_NormalMode_M2m_HalfWord_Burst4_1K_Test,
//    DMA2D_NormalMode_M2m_HalfWord_Burst8_1K_Test,
//    DMA2D_NormalMode_M2m_HalfWord_Burst16_1K_Test,
//    DMA2D_NormalMode_M2m_Byte_Burst1_1K_Test,
//    DMA2D_NormalMode_M2m_Byte_Burst4_1K_Test,
//    DMA2D_NormalMode_M2m_Byte_Burst8_1K_Test,
//    DMA2D_NormalMode_M2m_Byte_Burst16_1K_Test,
//    DMA2D_NormalMode_M2m_Word_Burst4_16K_Test,
//    DMA2D_NormalMode_M2m_Word_Burst4_32K_Test,
//    DMA2D_NormalMode_M2M_Word_Burst4_1K_Loop_Test,
//    DMA2D_NormalMode_Sram_To_Psram_Test,
//    DMA2D_NormalMode_Sram_To_Psram_WR_128Byte_Test,
//    DMA2D_NormalMode_Sram_To_Psram_WR_256Byte_Test,
//    DMA2D_NormalMode_Psram_To_Sram_Test,
//    DMA2D_NormalMode_Psram_To_Sram_RD_128Byte_Test,
//    DMA2D_NormalMode_Psram_To_Sram_RD_256Byte_Test,
//    DMA2D_NormalMode_AddrDec_SrcInc_DstDec_Test,
//    DMA2D_NormalMode_AddrDec_SrcDec_DstDec_Test,
//    DMA2D_NormalMode_AddrDec_SrcDec_DstInc_Test,

/* src gather & dst scatter */
//    DMA2D_Src_Gather_Test,
//    DMA2D_Dst_Scatter_Test,

/* image crop */
//    DMA2D_Image_Crop_YUV444_128x128_Test,
//    DMA2D_Image_Crop_YUV422_128x128_Test,
//    DMA2D_Image_Crop_RGB888_128x128_Test,
//    DMA2D_Image_Crop_RGB565_128x128_Test,
//    DMA2D_Image_Crop_Y8_128x128_Test,

/* image format convertion */
//    DMA2D_Image_YUV444_To_YUV422_128x128_Test,
//    DMA2D_Image_YUV444_To_RGB888_128x128_Test,
//    DMA2D_Image_YUV444_To_RGB565_128x128_Test,
//    DMA2D_Image_YUV444_To_Y8_128x128_Test,
//    DMA2D_Image_YUV422_To_YUV444_128x128_Test,
//    DMA2D_Image_YUV422_To_RGB888_128x128_Test,
//    DMA2D_Image_YUV422_To_RGB565_128x128_Test,
//    DMA2D_Image_YUV422_To_Y8_128x128_Test,
//    DMA2D_Image_RGB888_To_RGB565_128x128_Test,
//    DMA2D_Image_RGB888_To_YUV444_128x128_Test,
//    DMA2D_Image_RGB888_To_YUV422_128x128_Test,
//    DMA2D_Image_RGB888_To_Y8_128x128_Test,
//    DMA2D_Image_RGB565_To_RGB888_128x128_Test,
//    DMA2D_Image_RGB565_To_YUV444_128x128_Test,
//    DMA2D_Image_RGB565_To_YUV422_128x128_Test,
//    DMA2D_Image_RGB565_To_Y8_128x128_Test,
//    DMA2D_Image_Y8_To_RGB888_128x128_Test,
//    DMA2D_Image_Y8_To_RGB565_128x128_Test,
//    DMA2D_Image_Y8_To_YUV444_128x128_Test,
//    DMA2D_Image_Y8_To_YUV422_128x128_Test,

/* image copy */
//    DMA2D_Image_Copy_YUV444_128x128_Test,
//    DMA2D_Image_Copy_YUV444_128x128_Overlap_Test,
//    DMA2D_Image_Copy_YUV444_128x128_1d_Test,
//    DMA2D_Image_Copy_YUV444_128x128_Overlap_1d_Test,
//    DMA2D_Image_Copy_YUV422_128x128_Test,
//    DMA2D_Image_Copy_YUV422_128x128_Overlap_Test,
//    DMA2D_Image_Copy_YUV422_128x128_1d_Test,
//    DMA2D_Image_Copy_RGB888_128x128_Test,
//    DMA2D_Image_Copy_RGB888_128x128_Overlap_Test,
//    DMA2D_Image_Copy_RGB888_128x128_1d_Test,
//    DMA2D_Image_Copy_RGB565_128x128_Test,
//    DMA2D_Image_Copy_RGB565_128x128_Overlap_Test,
//    DMA2D_Image_Copy_RGB565_128x128_1d_Test,
//    DMA2D_Image_Copy_Y8_128x128_Test,
//    DMA2D_Image_Copy_Y8_128x128_Overlap_Test,
//    DMA2D_Image_Copy_Y8_128x128_1d_Test,

/* image rotation */
//    DMA2D_Image_Rotation_YUV444_128x128_CW90_Test,
//    DMA2D_Image_Rotation_YUV444_128x128_CW90_Tile_Test,
//    DMA2D_Image_Rotation_YUV444_128x128_CCW90_Test,
//    DMA2D_Image_Rotation_YUV444_128x128_CCW90_Tile_Test,
//    DMA2D_Image_Rotation_YUV444_128x128_CW180_Test,
//    DMA2D_Image_Rotation_YUV444_128x128_CW180_Tile_Test,
//    DMA2D_Image_Rotation_YUV444_128x128_Transpose_Test,
//    DMA2D_Image_Rotation_YUV444_128x128_Transpose_Tile_Test,
//    DMA2D_Image_Rotation_YUV422_128x128_CW90_Test,
//    DMA2D_Image_Rotation_YUV422_128x128_CW90_Tile_Test,
//    DMA2D_Image_Rotation_YUV422_128x128_CCW90_Test,
//    DMA2D_Image_Rotation_YUV422_128x128_CCW90_Tile_Test,
//    DMA2D_Image_Rotation_YUV422_128x128_CW180_Test,
//    DMA2D_Image_Rotation_YUV422_128x128_CW180_Tile_Test,
//    DMA2D_Image_Rotation_YUV422_128x128_Transpose_Test,
//    DMA2D_Image_Rotation_YUV422_128x128_Transpose_Tile_Test,
//    DMA2D_Image_Rotation_RGB888_128x128_CW90_Test,
//    DMA2D_Image_Rotation_RGB888_128x128_CW90_Tile_Test,
//    DMA2D_Image_Rotation_RGB888_128x128_CCW90_Test,
//    DMA2D_Image_Rotation_RGB888_128x128_CCW90_Tile_Test,
//    DMA2D_Image_Rotation_RGB888_128x128_CW180_Test,
//    DMA2D_Image_Rotation_RGB888_128x128_CW180_Tile_Test,
//    DMA2D_Image_Rotation_RGB888_128x128_Transpose_Test,
//    DMA2D_Image_Rotation_RGB888_128x128_Transpose_Tile_Test,
//    DMA2D_Image_Rotation_RGB565_128x128_CW90_Test,
//    DMA2D_Image_Rotation_RGB565_128x128_CW90_Tile_Test,
//    DMA2D_Image_Rotation_RGB565_128x128_CCW90_Test,
//    DMA2D_Image_Rotation_RGB565_128x128_CCW90_Tile_Test,
//    DMA2D_Image_Rotation_RGB565_128x128_CW180_Test,
//    DMA2D_Image_Rotation_RGB565_128x128_CW180_Tile_Test,
//    DMA2D_Image_Rotation_RGB565_128x128_Transpose_Test,
//    DMA2D_Image_Rotation_RGB565_128x128_Transpose_Tile_Test,
//    DMA2D_Image_Rotation_Y8_128x128_CW90_Test,
//    DMA2D_Image_Rotation_Y8_128x128_CW90_Tile_Test,
//    DMA2D_Image_Rotation_Y8_128x128_CCW90_Test,
//    DMA2D_Image_Rotation_Y8_128x128_CCW90_Tile_Test,
//    DMA2D_Image_Rotation_Y8_128x128_CW180_Test,
//    DMA2D_Image_Rotation_Y8_128x128_CW180_Tile_Test,
//    DMA2D_Image_Rotation_Y8_128x128_Transpose_Test,
//    DMA2D_Image_Rotation_Y8_128x128_Transpose_Tile_Test,
//    DMA2D_Image_Rotation_RGB888_160x120_CW90_Test,
//    DMA2D_Image_Rotation_RGB888_160x120_CW90_Tile_Test,
//    DMA2D_Image_Rotation_RGB888_160x120_CCW90_Test,
//    DMA2D_Image_Rotation_RGB888_160x120_CW180_Test,
//    DMA2D_Image_Rotation_RGB888_160x120_Transpose_Test,

/* image mirror */
//    DMA2D_Image_Mirror_YUV444_128x128_Hor_Test,
//    DMA2D_Image_Mirror_YUV444_128x128_Vert_Test,
//    DMA2D_Image_Mirror_YUV422_128x128_Hor_Test,
//    DMA2D_Image_Mirror_YUV422_128x128_Vert_Test,
//    DMA2D_Image_Mirror_RGB888_128x128_Hor_Test,
//    DMA2D_Image_Mirror_RGB888_128x128_Vert_Test,
//    DMA2D_Image_Mirror_RGB565_128x128_Hor_Test,
//    DMA2D_Image_Mirror_RGB565_128x128_Vert_Test,
//    DMA2D_Image_Mirror_Y8_128x128_Hor_Test,
//    DMA2D_Image_Mirror_Y8_128x128_Vert_Test,

/* image scaler */
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_128x128_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_128x256_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_128x200_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_128x64_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_128x100_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_128x50_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_256x128_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_200x128_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_256x256_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_200x256_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_256x200_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_200x200_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_256x64_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_200x64_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_256x100_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_256x50_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_200x100_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_200x50_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_64x128_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_100x128_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_50x128_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_64x256_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_100x256_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_50x256_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_64x200_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_100x200_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_50x200_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_64x64_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_100x64_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_50x64_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_64x100_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_64x50_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_100x100_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV444_50x50_Test,
//    DMA2D_Image_Scaler_YUV422_128x128_To_YUV422_128x128_Test,
//    DMA2D_Image_Scaler_RGB888_128x128_To_RGB888_128x128_Test,
//    DMA2D_Image_Scaler_RGB565_128x128_To_RGB565_128x128_Test,
//    DMA2D_Image_Scaler_Y8_128x128_To_Y8_128x128_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV422_256x64_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_YUV422_256x50_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_RGB888_256x64_Test,
//    DMA2D_Image_Scaler_YUV444_128x128_To_RGB565_256x64_Test,
//    DMA2D_Image_Scaler_YUV422_128x128_To_YUV444_256x64_Test,
//    DMA2D_Image_Scaler_YUV422_128x128_To_RGB888_256x64_Test,
//    DMA2D_Image_Scaler_YUV422_128x128_To_RGB565_256x64_Test,
//    DMA2D_Image_Scaler_YUV422_128x128_To_Y8_256x64_Test,
//    DMA2D_Image_Scaler_RGB888_128x128_To_RGB565_256x64_Test,
//    DMA2D_Image_Scaler_RGB888_128x128_To_YUV444_256x64_Test,
//    DMA2D_Image_Scaler_RGB888_128x128_To_YUV422_256x64_Test,
//    DMA2D_Image_Scaler_RGB888_128x128_To_Y8_256x64_Test,
//    DMA2D_Image_Scaler_RGB565_128x128_To_RGB888_256x64_Test,
//    DMA2D_Image_Scaler_RGB565_128x128_To_YUV444_256x64_Test,
//    DMA2D_Image_Scaler_RGB565_128x128_To_YUV422_256x64_Test,
//    DMA2D_Image_Scaler_Y8_128x128_To_RGB888_256x64_Test,
//    DMA2D_Image_Scaler_Y8_128x128_To_RGB565_256x64_Test,
//    DMA2D_Image_Scaler_Y8_128x128_To_YUV444_256x64_Test,
//    DMA2D_Image_Scaler_YUV422_160x120_Crop_128x96_To_RGB888_256x64_Test,

/* work register report */
//    DMA2D_Work_Register_Check_SGather_Test,
//    DMA2D_Work_Register_Check_DSatter_Test,
//    DMA2D_Work_Register_Check_Image_Test,

/* irq */
//    DMA2D_UserDef_Line_IRQ_Test,
//    DMA2D_UserDef_Byte_IRQ_Test,

/* chn trigger */
//    DMA2D_Trigger_Display_Normal_Test,
//    DMA2D_Trigger_Display_Mirror_Hor_Test,
//    DMA2D_Trigger_Display_Mirror_Vert_Test,
//    DMA2D_Trigger_Display_Rotation_CW90_Test,
//    DMA2D_Trigger_Display_Rotation_CCW90_Test,
//    DMA2D_Trigger_Display_Rotation_CW180_Test,
//    DMA2D_Trigger_Display_Rotation_Transpose_Test,
//    DMA2D_Trigger_Display_Camera_Online_Test,
};

int main(void)
{
    uint32_t times = 0;

    // Enable global interrupt
    enable_GINT();

    // Disable D-Cache
    DisableDCache();
    __RWMB();
    __FENCE_I();

    logInit(0, 115200);

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 8, CSK_IOMUX_FUNC_ALTER2);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 9, CSK_IOMUX_FUNC_ALTER2);

//    PSRAM_Initialize(NULL, NULL, 1);

    CLOGD("DMA2D VALIDATION\r\n");

    for (times = 0; times < sizeof(test_function_array) / sizeof(test_function_array[0]); times++)
    {
        test_function_array[times]();
    }

    while (1)
        ;
}


