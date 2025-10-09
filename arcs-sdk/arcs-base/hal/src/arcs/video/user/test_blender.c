#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "chip.h"
#include "log_print.h"
#include "PSRAMManager.h"
#include "Driver_GPDMA.h"
#include "systick.h"
#include "ClockManager.h"
#include "Driver_Blender.h"
#include "cache.h"

#include "test_case.h"
#include "csk_driver.h"
#include "blender_case_config.h"

static int32_t test_blender_06_01(void);   // clk enable and reset
static int32_t test_blender_case(Blender_InitTypeDef *pblender_cfg, uint16_t id);
extern int img_blender_sw(Blender_InitTypeDef *pblender_cfg, void *dma_cfg, uint8_t *pout);
/* savebin D:\src\picture\blender_back_colorbar_bgr565_96x96.bin 0x200327d0 0x4800 */

void test_blender(void)
{
    int32_t ret = FAILURE;
    uint32_t i = 0;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    //CHECK_FUNC_EXIT(test_blender_06_01(), error);   // clk enable and reset     // pass

#if 1
    //CHECK_FUNC_EXIT(test_blender_case(blender_case_tab[0].pcfg, blender_case_tab[0].id), error);  // case0101: RGB565 back + color + mask + alpha
    //CHECK_FUNC_EXIT(test_blender_case(blender_case_tab[17].pcfg, blender_case_tab[17].id), error);  // case0403: RGB565 back + fore + mask + alpha
    CHECK_FUNC_EXIT(test_blender_case(blender_case_tab[3].pcfg, blender_case_tab[3].id), error);  // case0201: RGB565 back + color + alpha

    return;
#endif

#if 1
    for(i = 0; i < (sizeof(blender_case_tab) / sizeof(blender_CaseTypeDef)); i++)
    {
        blender_case_tab[i].ret = test_blender_case(blender_case_tab[i].pcfg, blender_case_tab[i].id);
        //CHECK_RET_EQ_EXIT(blender_case_tab[i].ret, SUCCESS, error);
    }

    for(i = 0; i < (sizeof(blender_case_tab) / sizeof(blender_CaseTypeDef)); i++)
    {
        if(SUCCESS == blender_case_tab[i].ret) {
            VIDEO_LOG("test blender case %04x SUCCESS", blender_case_tab[i].id);
        } else {
            VIDEO_LOG("test blender case %04x FAILED", blender_case_tab[i].id);
        }
    }
    return;
#else
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0101, 0x0101), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0102, 0x0102), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0103, 0x0103), error);   // pass

    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0201, 0x0201), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0202, 0x0202), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0203, 0x0203), error);   // pass

    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0301, 0x0301), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0302, 0x0302), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0303, 0x0303), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0304, 0x0304), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0305, 0x0305), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0306, 0x0306), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0307, 0x0307), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0308, 0x0308), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0309, 0x0309), error);   // pass

    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0401, 0x0401), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0402, 0x0402), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0403, 0x0403), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0404, 0x0404), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0405, 0x0405), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0406, 0x0406), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0407, 0x0407), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0408, 0x0408), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0409, 0x0409), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0410, 0x0410), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0411, 0x0411), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0412, 0x0412), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0413, 0x0413), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0414, 0x0414), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0415, 0x0415), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0416, 0x0416), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0417, 0x0417), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0418, 0x0418), error);   // pass

    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0501, 0x0501), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0502, 0x0502), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0503, 0x0503), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0504, 0x0504), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0505, 0x0505), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0506, 0x0506), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0507, 0x0507), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0508, 0x0508), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0509, 0x0509), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0510, 0x0510), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0511, 0x0511), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0512, 0x0512), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0513, 0x0513), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0514, 0x0514), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0515, 0x0515), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0516, 0x0516), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0517, 0x0517), error);   // pass
    CHECK_FUNC_EXIT(test_blender_case(&blender_cfg_case0518, 0x0518), error);   // pass

    ret = SUCCESS;
    VIDEO_LOG("[%s:%d]  all case test SUCCESS\r\n", __func__, __LINE__);
    return;
#endif

error:
    ret = FAILURE;
    VIDEO_LOG("[%s:%d]  case test FAILED\r\n", __func__, __LINE__);
    return;
}


static void blender_reset(void)
{
    __HAL_CRM_VIDEO_CLK_ENABLE();
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_BLENDER_CLK = 0x1; // blender clk enable
    IP_AP_CFG->REG_SW_RESET.bit.VIDEO_RESET = 0x1;
    IP_AP_CFG->REG_SW_RESET.bit.BLENDER_RESET = 0x1;
}

static void blender_cfg_dump(Blender_InitTypeDef *pCfg)
{
    VIDEO_LOG("blender_mode=%d", pCfg->blender_mode);
    VIDEO_LOG("alpha_mode=%d", pCfg->alpha_mode);
    VIDEO_LOG("back_format=%d", pCfg->back_format);
    VIDEO_LOG("fore_format=%d", pCfg->fore_format);
    VIDEO_LOG("img_width=%d", pCfg->img_width);
    VIDEO_LOG("img_height=%d", pCfg->img_height);
    VIDEO_LOG("color=0x%x", pCfg->color);
    VIDEO_LOG("alpha=0x%x", pCfg->alpha);
}

void blender_reg_dump(void)
{
    VIDEO_LOG("0x00 IMAGE_PROC_EN   *0x%08x=0x%08x", &IP_D2BLENDER->REG_BLENDER_EN.all, IP_D2BLENDER->REG_BLENDER_EN.all);
    VIDEO_LOG("0x04 BLENDER_CTRL    *0x%08x=0x%08x", &IP_D2BLENDER->REG_BLENDER_CTRL.all, IP_D2BLENDER->REG_BLENDER_CTRL.all);
    VIDEO_LOG("0x08 ALPHA           *0x%08x=0x%08x", &IP_D2BLENDER->REG_ALPHA.all, IP_D2BLENDER->REG_ALPHA.all);
    VIDEO_LOG("0x0C COLOR           *0x%08x=0x%08x", &IP_D2BLENDER->REG_COLOR.all, IP_D2BLENDER->REG_COLOR.all);
    VIDEO_LOG("0x10 FIFO_BURST_THD  *0x%08x=0x%08x", &IP_D2BLENDER->REG_FIFO_BURST_THD.all, IP_D2BLENDER->REG_FIFO_BURST_THD.all);
    VIDEO_LOG("0x14 FORE_SIZE       *0x%08x=0x%08x", &IP_D2BLENDER->REG_D2BLENDER_FORE_SIZE.all, IP_D2BLENDER->REG_D2BLENDER_FORE_SIZE.all);
    VIDEO_LOG("0x18 BACK_SIZE       *0x%08x=0x%08x", &IP_D2BLENDER->REG_D2BLENDER_BACK_SIZE.all, IP_D2BLENDER->REG_D2BLENDER_BACK_SIZE.all);
    VIDEO_LOG("0x1C MASK_SIZE       *0x%08x=0x%08x", &IP_D2BLENDER->REG_D2BLENDER_MASK_SIZE.all, IP_D2BLENDER->REG_D2BLENDER_MASK_SIZE.all);
}

static uint8_t blender_get_backformat_byte(Blender_emBackFormat format)
{
    switch(format)
    {
        case BLENDER_BACK_FORMAT_RGB565:
            return 2;

        case BLENDER_BACK_FORMAT_RGB888:
            return 3;

        case BLENDER_BACK_FORMAT_ARGB8888:
            return 4;

        default:
            return 0;
    }
}

static uint8_t blender_get_foreformat_byte(Blender_emForeFormat format)
{
    switch(format)
    {
        case BLENDER_FORE_FORMAT_L8:
            return 1;

        case BLENDER_FORE_FORMAT_RGB565:
        case BLENDER_FORE_FORMAT_ARGB1555:
        case BLENDER_FORE_FORMAT_ARGB4444:
            return 2;

        case BLENDER_FORE_FORMAT_RGB888:
            return 3;

        case BLENDER_FORE_FORMAT_ARGB8888:
            return 4;

        default:
            return 0;
    }
}

/****************************************** DMA ************************************************/
typedef struct
{
    void *back_addr;
    void *fore_addr;
    void *mask_addr;
    void *out_addr;
    uint32_t back_size;     // word
    uint32_t fore_size;     // word
    uint32_t mask_size;     // word
    uint32_t out_size;      // word
}DMA_CfgTypeDef;

static void d2blender_back_dma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] back event=%d", __func__, __LINE__, event);
}

static void d2blender_fore_dma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] fore event=%d", __func__, __LINE__, event);
}

static void d2blender_mask_dma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] mask event=%d", __func__, __LINE__, event);
}

static volatile uint32_t blender_dma_finish_flag = 0;

static void d2blender_out_dma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] out event=%d", __func__, __LINE__, event);
    blender_dma_finish_flag++;
}


#define D2BLENDER_BURST_LEN  gpdma_burst_len_4spl

static int32_t d2blender_dma_init(void)
{
    int32_t ret;

    csk_gpdma_init_t d2back_input = {
            .dma_ch = D2BLENDER_BACK_DMA_CH,
            .burst_len = D2BLENDER_BACK_GPDMA_BURST_LEN,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2p,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_fix,
            .prio_lvl = prio_mode_vhigh,
            .sample_unit = gpdma_sample_unit_word,
            .handshake = d2back_hs_num3,
    };

    csk_gpdma_init_t d2fore_input = {
            .dma_ch = D2BLENDER_FORE_DMA_CH,
            .burst_len = D2BLENDER_FORE_GPDMA_BURST_LEN,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2p,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_fix,
            .prio_lvl = prio_mode_vhigh,
            .sample_unit = gpdma_sample_unit_word,
            .handshake = d2fore_hs_num4,
    };

    csk_gpdma_init_t d2mask_input = {
            .dma_ch = D2BLENDER_MASK_DMA_CH,
            .burst_len = D2BLENDER_MASK_GPDMA_BURST_LEN,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2p,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_fix,
            .prio_lvl = prio_mode_vhigh,
            .sample_unit = gpdma_sample_unit_word,
            .handshake = d2mask_hs_num2,
    };

    csk_gpdma_init_t d2out_output = {
            .dma_ch = D2BLENDER_OUT_DMA_CH,
            .burst_len = D2BLENDER_OUT_GPDMA_BURST_LEN,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_p2m,
            .src_inc_mode = inc_mode_fix,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .sample_unit = gpdma_sample_unit_word,
            .handshake = d2out_hs_num1,
    };
    //VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    ret = GPDMA_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    ret = GPDMA_Config(&d2back_input, d2blender_back_dma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    ret = GPDMA_Config(&d2fore_input, d2blender_fore_dma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    ret = GPDMA_Config(&d2mask_input, d2blender_mask_dma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    ret = GPDMA_Config(&d2out_output, d2blender_out_dma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    //VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    return CSK_DRIVER_OK;
}


static int32_t d2blender_dma_start(DMA_CfgTypeDef *cfg)
{
    int32_t ret;

//    IP_GPDMA->REG_DMA_CH_CLR.bit.CFG_CH_CLR = 0x1 << D2BLENDER_BACK_DMA_CH;
//    IP_GPDMA->REG_DMA_CH_CLR.bit.CFG_CH_CLR = 0x1 << D2BLENDER_FORE_DMA_CH;
//    IP_GPDMA->REG_DMA_CH_CLR.bit.CFG_CH_CLR = 0x1 << D2BLENDER_MASK_DMA_CH;
//    IP_GPDMA->REG_DMA_CH_CLR.bit.CFG_CH_CLR = 0x1 << D2BLENDER_OUT_DMA_CH;

    //VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    if (NULL != cfg->back_addr) {
        ret = GPDMA_Start_Normal(D2BLENDER_BACK_DMA_CH, (uint32_t*)cfg->back_addr, (uint32_t*)D2BACK_BUF, cfg->back_size);
        //VIDEO_LOG("[%s:%d]", __func__, __LINE__);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    }
    if (NULL != cfg->fore_addr) {
        ret = GPDMA_Start_Normal(D2BLENDER_FORE_DMA_CH, (uint32_t*)cfg->fore_addr, (uint32_t*)D2FORE_BUF, cfg->fore_size);
        //VIDEO_LOG("[%s:%d]", __func__, __LINE__);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    }
    if (NULL != cfg->mask_addr) {
        ret = GPDMA_Start_Normal(D2BLENDER_MASK_DMA_CH, (uint32_t*)cfg->mask_addr, (uint32_t*)D2MASK_BUF, cfg->mask_size);
        //VIDEO_LOG("[%s:%d]", __func__, __LINE__);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    }
    if (NULL != cfg->out_addr) {
        ret = GPDMA_Start_Normal(D2BLENDER_OUT_DMA_CH, (uint32_t*)D2OUT_BUF, (uint32_t*)cfg->out_addr, cfg->out_size);
        //VIDEO_LOG("[%s:%d]", __func__, __LINE__);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    }
    //VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    return CSK_DRIVER_OK;
}


/****************************************** CASE ************************************************/
static int image_data_filling(Blender_InitTypeDef *pblender_cfg, DMA_CfgTypeDef *pdma_cfg, bool colorbar_en)
{
    uint8_t *pdata = NULL;
    uint16_t *prgb565_data = NULL;
    uint32_t pixel_num = 0;

    if(true == colorbar_en)
    {
        /* back image */
        if(NULL != pdma_cfg->back_addr)
        {
            pixel_num = (pblender_cfg->img_width * pblender_cfg->img_height);
            switch(pblender_cfg->back_format)
            {
                case BLENDER_BACK_FORMAT_RGB565:
                    rgb565_colorbar_create((uint16_t *)pdma_cfg->back_addr, pblender_cfg->img_width, pblender_cfg->img_height, 10);
                    break;

                case BLENDER_BACK_FORMAT_RGB888:
                    rgb565_colorbar_create((uint16_t *)pdma_cfg->out_addr, pblender_cfg->img_width, pblender_cfg->img_height, 10);
                    rgb565_to_rgb888((uint16_t *)pdma_cfg->out_addr, (uint8_t *)pdma_cfg->back_addr, pixel_num);
                    memset(pdma_cfg->out_addr, 0, pdma_cfg->out_size * sizeof(uint32_t));
                    break;

                case BLENDER_BACK_FORMAT_ARGB8888:
                    rgb565_colorbar_create((uint16_t *)pdma_cfg->out_addr, pblender_cfg->img_width, pblender_cfg->img_height, 10);
                    rgb565_to_argb8888((uint16_t *)pdma_cfg->out_addr, (uint8_t *)pdma_cfg->back_addr, pixel_num);
                    memset(pdma_cfg->out_addr, 0, pdma_cfg->out_size * sizeof(uint32_t));
                    break;

                default:
                    return FAILURE;
            }
        }

        /* fore image */
        if(NULL != pdma_cfg->fore_addr)
        {
            pixel_num = (pblender_cfg->img_width * pblender_cfg->img_height);
            switch(pblender_cfg->fore_format)
            {
                case BLENDER_FORE_FORMAT_RGB565:
                    prgb565_data = (uint16_t *)pdma_cfg->fore_addr;
                    while(pixel_num--)
                    {
                        *prgb565_data++ = RGB565_RED;
                    }
                    break;

                case BLENDER_FORE_FORMAT_RGB888:
                    pdata = (uint8_t *)pdma_cfg->fore_addr;
                    while(pixel_num--)
                    {
                        *pdata++ = 0x00;    // B
                        *pdata++ = 0x00;    // G
                        *pdata++ = 0xFF;    // R
                    }
                    break;

                case BLENDER_FORE_FORMAT_ARGB8888:
                    pdata = (uint8_t *)pdma_cfg->fore_addr;
                    while(pixel_num--)
                    {
                        *pdata++ = 0x00;    // B
                        *pdata++ = 0x00;    // G
                        *pdata++ = 0xFF;    // R
                        *pdata++ = 0xFF;    // A
                    }
                    break;

                case BLENDER_FORE_FORMAT_ARGB1555:
                    prgb565_data = (uint16_t *)pdma_cfg->fore_addr;
                    while(pixel_num--)
                    {
                        *prgb565_data++ = 0xFC00;
                    }
                    break;

                case BLENDER_FORE_FORMAT_ARGB4444:
                    prgb565_data = (uint16_t *)pdma_cfg->fore_addr;
                    while(pixel_num--)
                    {
                        *prgb565_data++ = 0xFF00;
                    }
                    break;

                case BLENDER_FORE_FORMAT_L8:
                    prgb565_data = (uint16_t *)pdma_cfg->fore_addr;
                    while(pixel_num--)
                    {
                        *prgb565_data++ = 0x80;
                    }
                    break;

                default:
                    return FAILURE;
            }
        }

        /* mask image */
        if(NULL != pdma_cfg->mask_addr)
        {
            pdata = (uint8_t *)pdma_cfg->mask_addr;
            pixel_num = pdma_cfg->mask_size * sizeof(uint32_t);
            while(pixel_num--)
            {
                *pdata++ = 0x80;
            }
        }
    }
    else
    {
        /* back image */
        if(NULL != pdma_cfg->back_addr)
        {
            pdata = (uint8_t *)pdma_cfg->back_addr;
            pixel_num = pdma_cfg->back_size * sizeof(uint32_t);
            while(pixel_num--)
            {
                *pdata++ = rand() & 0xFF;
                //*pdata++ = 0x0F;
            }
        }

        /* fore image */
        if(NULL != pdma_cfg->fore_addr)
        {
            pdata = (uint8_t *)pdma_cfg->fore_addr;
            pixel_num = pdma_cfg->fore_size * sizeof(uint32_t);
            while(pixel_num--)
            {
                *pdata++ = rand() & 0xFF;
            }
        }

        /* mask image */
        if(NULL != pdma_cfg->mask_addr)
        {
            pdata = (uint8_t *)pdma_cfg->mask_addr;
            pixel_num = pdma_cfg->mask_size * sizeof(uint32_t);
            while(pixel_num--)
            {
                *pdata++ = rand() & 0xFF;
                //*pdata++ = 0xC0;
            }
        }
    }

    return SUCCESS;
}

static int image_data_check(uint8_t *pout_buf, uint8_t *psw_buf, Blender_emBackFormat back_format, uint32_t pixel_num)
{
    uint16_t *pout_rgb565 = (uint16_t *)pout_buf;
    uint16_t *psw_rgb565 = (uint16_t *)psw_buf;
    uint8_t out_color = 0;
    uint8_t sw_color = 0;
    uint32_t i = 0;
    uint32_t diff = 0;

    switch(back_format)
    {
        case BLENDER_BACK_FORMAT_RGB565:
            for(i = 0; i < pixel_num; i++)
            {
                out_color = (*pout_rgb565 & 0xF800) >> 11;
                sw_color = (*psw_rgb565 & 0xF800) >> 11;
                diff = (out_color >= sw_color) ? (out_color - sw_color) : (sw_color - out_color);
                if(!(diff <= TEST_BLENDER_CHECK_DIFF))
                {
                    VIDEO_LOG("[%s:%d] check blender failed in pixel %d", __func__, __LINE__, i);
                    VIDEO_LOG("blender_out=0x%x sw=0x%x diff=%d", *pout_rgb565, *psw_rgb565, diff);
                    goto error;
                }

                out_color = (*pout_rgb565 & 0x07E0) >> 5;
                sw_color = (*psw_rgb565 & 0x07E0) >> 5;
                diff = (out_color >= sw_color) ? (out_color - sw_color) : (sw_color - out_color);
                if(!(diff <= TEST_BLENDER_CHECK_DIFF))
                {
                    VIDEO_LOG("[%s:%d] check blender failed in pixel %d", __func__, __LINE__, i);
                    VIDEO_LOG("blender_out=0x%x sw=0x%x diff=%d", *pout_rgb565, *psw_rgb565, diff);
                    goto error;
                }

                out_color = (*pout_rgb565 & 0x001F);
                sw_color = (*psw_rgb565 & 0x001F);
                diff = (out_color >= sw_color) ? (out_color - sw_color) : (sw_color - out_color);
                if(!(diff <= TEST_BLENDER_CHECK_DIFF))
                {
                    VIDEO_LOG("[%s:%d] check blender failed in pixel %d", __func__, __LINE__, i);
                    VIDEO_LOG("blender_out=0x%x sw=0x%x diff=%d", *pout_rgb565, *psw_rgb565, diff);
                    goto error;
                }

                pout_rgb565++;
                psw_rgb565++;
            }
            break;

        case BLENDER_BACK_FORMAT_RGB888:
            for(i = 0; i < (pixel_num * 3); i++)
            {
                diff = (*pout_buf >= *psw_buf) ? (*pout_buf - *psw_buf) : (*psw_buf - *pout_buf);

                if(!(diff <= TEST_BLENDER_CHECK_DIFF))
                {
                    VIDEO_LOG("[%s:%d] check blender failed in pixel byte %d", __func__, __LINE__, i);
                    VIDEO_LOG("blender_out=0x%x sw=0x%x diff=%d", *pout_buf, *psw_buf, diff);
                    //goto error;
                }
                pout_buf++;
                psw_buf++;
            }
            break;

        case BLENDER_BACK_FORMAT_ARGB8888:
            for(i = 0; i < (pixel_num * 4); i++)
            {
                diff = (*pout_buf >= *psw_buf) ? (*pout_buf - *psw_buf) : (*psw_buf - *pout_buf);

                if(!(diff <= TEST_BLENDER_CHECK_DIFF))
                {
                    VIDEO_LOG("[%s:%d] check blender failed in pixel byte %d", __func__, __LINE__, i);
                    VIDEO_LOG("blender_out=0x%x sw=0x%x diff=%d", *pout_buf, *psw_buf, diff);
                    //goto error;
                }
                pout_buf++;
                psw_buf++;
            }
            break;

        default:
            goto error;
    }
    return 0;

error:
    return i;
}


static int test_d2blender_unit(Blender_InitTypeDef *pblender_cfg, bool colorbar_en)
{
    int32_t ret = FAILURE;
    void *blender_dev = Blender0();
    DMA_CfgTypeDef dma_cfg = {0};
    uint8_t *pdata = NULL;
    uint8_t *pblender_swbuf = NULL;
    uint32_t img_size = (pblender_cfg->img_width * pblender_cfg->img_height * blender_get_backformat_byte(pblender_cfg->back_format));
    uint32_t i = 0;
    uint32_t look_num = 0;
    uint32_t colorbar = 0;
    uint8_t *pback_rgb888 = NULL;
    uint8_t *pfore_rgb888 = NULL;

#if BLENDER_CYCLE_ENABLE
    register uint32_t cycle_start = 0;
    register uint32_t cycle_end = 0;
#endif

    CHECK_POINT_NOT_NULL_EXIT(pblender_cfg, error0);
    //VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    dma_cfg.back_size = (pblender_cfg->img_width * pblender_cfg->img_height * blender_get_backformat_byte(pblender_cfg->back_format)) / sizeof(uint32_t);
#ifdef BLENDER_BASK_BUF
    dma_cfg.back_addr = (void *)BLENDER_BASK_BUF;
#else
    dma_cfg.back_addr = tiny_malloc(dma_cfg.back_size * sizeof(uint32_t));
#endif
    CHECK_POINT_NOT_NULL_EXIT(dma_cfg.back_addr, error1);

    if(pblender_cfg->blender_mode == BLENDER_MODE_MAP) {
        dma_cfg.fore_size = (pblender_cfg->img_width * pblender_cfg->img_height * blender_get_foreformat_byte(pblender_cfg->fore_format)) / sizeof(uint32_t);
#ifdef BLENDER_FORE_BUF
        dma_cfg.fore_addr = (void *)BLENDER_FORE_BUF;
#else
        dma_cfg.fore_addr = tiny_malloc(dma_cfg.fore_size * sizeof(uint32_t));
#endif
        CHECK_POINT_NOT_NULL_EXIT(dma_cfg.fore_addr, error1);
    } else {
        dma_cfg.fore_size = 0;
        dma_cfg.fore_addr = NULL;
    }

    if(pblender_cfg->alpha_mode == BLENDER_ALPHA_MODE_1) {
        dma_cfg.mask_size = (pblender_cfg->img_width * pblender_cfg->img_height) / sizeof(uint32_t);
#ifdef BLENDER_MASK_BUF
        dma_cfg.mask_addr = (void *)BLENDER_MASK_BUF;
#else
        dma_cfg.mask_addr = tiny_malloc(dma_cfg.mask_size * sizeof(uint32_t));
#endif
        CHECK_POINT_NOT_NULL_EXIT(dma_cfg.mask_addr, error1);
    } else {
        dma_cfg.mask_size = 0;
        dma_cfg.mask_addr = NULL;
    }

    dma_cfg.out_size = dma_cfg.back_size;
#ifdef BLENDER_OUT_BUF
    dma_cfg.out_addr = (void *)BLENDER_OUT_BUF;
#else
    dma_cfg.out_addr = tiny_malloc(dma_cfg.out_size * sizeof(uint32_t));
#endif
    CHECK_POINT_NOT_NULL_EXIT(dma_cfg.out_addr, error1);

#ifdef BLENDER_SWOUT_BUF
    pblender_swbuf = (void *)BLENDER_SWOUT_BUF;
#else
    pblender_swbuf = tiny_malloc(dma_cfg.out_size * sizeof(uint32_t));
#endif
    CHECK_POINT_NOT_NULL_EXIT(pblender_swbuf, error1);

    /* back/fore/mask image data init */
    image_data_filling(pblender_cfg, &dma_cfg, colorbar_en);
    memset(dma_cfg.out_addr, 0, dma_cfg.out_size * sizeof(uint32_t));

    /* blender softcode */
    img_blender_sw(pblender_cfg, &dma_cfg, pblender_swbuf);

    /* blender dma init */
    ret = d2blender_dma_init();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    /* blender init */
    blender_reset();
    DELAY_MS(1);
    ret = Blender_Initialize(blender_dev, pblender_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

//    VIDEO_LOG("d2back_buffer:  *0x%08x=0x%x", dma_cfg.back_addr, *(uint32_t *)dma_cfg.back_addr);
//    VIDEO_LOG("d2fore_buffer:  *0x%08x=0x%x", dma_cfg.fore_addr, *(uint32_t *)dma_cfg.fore_addr);
//    VIDEO_LOG("d2mask_buffer:  *0x%08x=0x%x", dma_cfg.mask_addr, *(uint32_t *)dma_cfg.mask_addr);
//    VIDEO_LOG("d2out_buffer:   *0x%08x=0x%x", dma_cfg.out_addr, *(uint32_t *)dma_cfg.out_addr);
//    VIDEO_LOG("swout_buffer:   *0x%08x=0x%x", pblender_swbuf, *(uint32_t *)pblender_swbuf);
//    VIDEO_LOG("out_size: 0x%x byte", dma_cfg.out_size * sizeof(uint32_t));

    look_num = 10;
    while(look_num--)
    {
        /* blender start */
        blender_dma_finish_flag = 0;
        ret = d2blender_dma_start(&dma_cfg);
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

#if BLENDER_CYCLE_ENABLE
        cycle_start = __RV_CSR_READ(CSR_MCYCLE);
#endif

        ret = Blender_Start(blender_dev);
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

        i = 1000000;  // wait blender done, timeout=1000ms
        while(!blender_dma_finish_flag)
        {
            DELAY_US(1);
            if(i-- == 0)
            {
                VIDEO_LOG("[%s:%d] wait timeout", __func__, __LINE__);
                ret = FAILURE;
                goto error2;
            }
        }

#if BLENDER_CYCLE_ENABLE
        cycle_end = __RV_CSR_READ(CSR_MCYCLE);
        VIDEO_LOG("[%s:%d] cycle=%d", __func__, __LINE__, cycle_end - cycle_start);   // SOC is 300MHz

#if 0
        cycle_start = __RV_CSR_READ(CSR_MCYCLE);
        DELAY_MS(10);
        cycle_end = __RV_CSR_READ(CSR_MCYCLE);
        VIDEO_LOG("[%s:%d] delay10ms cycle=%d", __func__, __LINE__, cycle_end - cycle_start);
#endif
#endif

        Blender_Stop(blender_dev);

        //HAL_InvalidateDCache();
        //HAL_InvalidateDCache_by_Addr(dma_cfg.out_addr, dma_cfg.out_size * sizeof(uint32_t));

        /* check data */
        ret = image_data_check(dma_cfg.out_addr, pblender_swbuf, pblender_cfg->back_format, pblender_cfg->img_width * pblender_cfg->img_height);
        CHECK_RET_EQ_EXIT(ret, SUCCESS, error2);
    }

    if(false == colorbar_en)
    {
        tiny_free(dma_cfg.out_addr);
        dma_cfg.out_addr = dma_cfg.back_addr;

        look_num = 10;
        while(look_num--)
        {
            /* blender softcode */
            img_blender_sw(pblender_cfg, &dma_cfg, pblender_swbuf);

            /* blender start */
            blender_dma_finish_flag = 0;
            ret = d2blender_dma_start(&dma_cfg);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            Blender_Start(blender_dev);

            i = 1000000;  // wait blender done, timeout=1000ms
            while(!blender_dma_finish_flag)
            {
                DELAY_US(1);
                if(i-- == 0)
                {
                    VIDEO_LOG("[%s:%d] wait timeout", __func__, __LINE__);
                    ret = FAILURE;
                    goto error2;
                }
            }
            Blender_Stop(blender_dev);

            /* check data */
            ret = image_data_check(dma_cfg.out_addr, pblender_swbuf, pblender_cfg->back_format, pblender_cfg->img_width * pblender_cfg->img_height);
            CHECK_RET_EQ_EXIT(ret, 0, error2);
            //VIDEO_LOG("d2out_buffer:   *0x%08x=0x%x", dma_cfg.out_addr, *(uint32_t *)dma_cfg.out_addr);
        }
    }

    if(true == colorbar_en)
        goto error2;
    else
        goto error1;

error2:
    blender_cfg_dump(pblender_cfg);
    blender_reg_dump();
    VIDEO_LOG("DMA_INT_STATUS=0x%x", mmio_read32(GPDMA_BASE + 0x158));
    VIDEO_LOG("d2back_buffer:  *0x%08x=0x%x", dma_cfg.back_addr, *(uint32_t *)dma_cfg.back_addr);
    VIDEO_LOG("d2fore_buffer:  *0x%08x=0x%x", dma_cfg.fore_addr, *(uint32_t *)dma_cfg.fore_addr);
    VIDEO_LOG("d2mask_buffer:  *0x%08x=0x%x", dma_cfg.mask_addr, *(uint32_t *)dma_cfg.mask_addr);
    VIDEO_LOG("d2out_buffer:   *0x%08x=0x%x", dma_cfg.out_addr, *(uint32_t *)dma_cfg.out_addr);
    VIDEO_LOG("swout_buffer:   *0x%08x=0x%x", pblender_swbuf, *(uint32_t *)pblender_swbuf);
    VIDEO_LOG("out_size: 0x%x byte", dma_cfg.out_size * sizeof(uint32_t));

    switch(pblender_cfg->back_format)
    {
        case BLENDER_BACK_FORMAT_RGB565:
            VIDEO_LOG("d2back_data:  RGB565=0x%x", *((uint16_t *)(dma_cfg.back_addr + ret * 2)));
            if(NULL != dma_cfg.mask_addr) {
                VIDEO_LOG("d2mask_data: 0x%x", *((uint8_t *)(dma_cfg.mask_addr + ret)));
            }
            break;

        case BLENDER_BACK_FORMAT_RGB888:
            VIDEO_LOG("d2back_data:  RGB888=0x%x", *((uint8_t *)(dma_cfg.back_addr + ret)));
            if(NULL != dma_cfg.mask_addr) {
                VIDEO_LOG("d2mask_data: 0x%x", *((uint8_t *)(dma_cfg.mask_addr + ret/3)));
            }
            break;

        case BLENDER_BACK_FORMAT_ARGB8888:
            VIDEO_LOG("d2back_data:  ARGB8888=0x%x", *((uint8_t *)(dma_cfg.back_addr + ret)));
            if(NULL != dma_cfg.mask_addr) {
                VIDEO_LOG("d2mask_data: 0x%x", *((uint8_t *)(dma_cfg.mask_addr + ret/4)));
            }
            break;

        default:
            break;
    }

error1:
#ifndef BLENDER_SWOUT_BUF
    tiny_free(pblender_swbuf);
#endif
#ifndef BLENDER_OUT_BUF
    tiny_free(dma_cfg.out_addr);
#endif
#ifndef BLENDER_MASK_BUF
    tiny_free(dma_cfg.mask_addr);
#endif
#ifndef BLENDER_FORE_BUF
    tiny_free(dma_cfg.fore_addr);
#endif
#ifndef BLENDER_BASK_BUF
    tiny_free(dma_cfg.back_addr);
#endif

error0:
    if(ret == SUCCESS) {
        //VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t test_blender_case(Blender_InitTypeDef *pblender_cfg, uint16_t id)
{
    int32_t ret = FAILURE;
    uint32_t error_num = 0;
    uint32_t alpha = 0;

    VIDEO_LOG("\r\n[%s%04x:%d] test start", __func__, id, __LINE__);

#if TEST_BLENDER_ALPHA_COLOR_ALL
    for(pblender_cfg->color = 0x000000; pblender_cfg->color <= 0xFFFFFF; pblender_cfg->color++)
    {
        for(alpha = 0x00; alpha <= 0xFF; alpha++)
        {
#else
    for(pblender_cfg->color = (rand() & 0x0F0F0F); pblender_cfg->color <= 0xFFFFFF; pblender_cfg->color += 0x101010)
    {
        for(alpha = (rand() & 0x0F); alpha <= 0xFF; alpha += 0x10)
        {
#endif
            pblender_cfg->alpha = alpha & 0xFF;
            ret = test_d2blender_unit(pblender_cfg, false);
            if(ret != SUCCESS)
            {
                error_num++;
                //return ret;
            }
            VIDEO_LOG("[%s%04x:%d] color=0x%06x alpha=0x%x", __func__, id, __LINE__, pblender_cfg->color, pblender_cfg->alpha);
        }
    }

    /* test colorbar */
    VIDEO_LOG("[%s%04x:%d] test colorbar", __func__, id, __LINE__);
    pblender_cfg->color = 0x000080;     // B8:G8:R8
    pblender_cfg->alpha = 0x80;
    ret = test_d2blender_unit(pblender_cfg, true);
    if(ret != SUCCESS)
    {
        error_num++;
    }
    VIDEO_LOG("[%s%04x:%d] color=0x%06x alpha=0x%x", __func__, id, __LINE__, pblender_cfg->color, pblender_cfg->alpha);

    if(error_num == 0) {
        VIDEO_LOG("[%s%04x:%d] test SUCCESS\r\n", __func__, id, __LINE__);
        ret = SUCCESS;
    } else {
        VIDEO_LOG("[%s%04x:%d] error_num=%d", __func__, id, __LINE__, error_num);
        VIDEO_LOG("[%s%04x:%d] test FAILED\r\n", __func__, id, __LINE__);
        ret = FAILURE;
    }
    return ret;
}



static int32_t test_blender_06_01(void)   // clk enable and reset
{
    int32_t ret = FAILURE;
    void *blender_dev = Blender0();
    Blender_InitTypeDef blender_cfg = {
            .blender_mode = BLENDER_MODE_FILL,
            .alpha_mode = BLENDER_ALPHA_MODE_2,
            .back_format = BLENDER_BACK_FORMAT_RGB565,
            .fore_format = BLENDER_FORE_FORMAT_RGB565,
            .img_width = 96,
            .img_height = 96,
            .color = 0x00654321,      // XRGB
            .alpha = 0xA5,
            .burst_thd = 8,
    };

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    __HAL_CRM_VIDEO_CLK_ENABLE();
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_BLENDER_CLK = 0x1; // blender clk enable
    IP_AP_CFG->REG_SW_RESET.bit.VIDEO_RESET = 0x1;
    DELAY_MS(1);
    mmio_write32(IMAGE_PROC_BASE + 0xC, 1);   // d2blender enable

    Blender_Initialize(blender_dev, &blender_cfg);
    Blender_Start(blender_dev);
    blender_reg_dump();
    VIDEO_LOG("[%s:%d]\r\n", __func__, __LINE__);

    CHECK_RET_EQ_EXIT(IP_D2BLENDER->REG_BLENDER_EN.all, 0x2, error);
    CHECK_RET_EQ_EXIT(IP_D2BLENDER->REG_BLENDER_CTRL.all, 0x22, error);
    CHECK_RET_EQ_EXIT(IP_D2BLENDER->REG_ALPHA.all, 0xA5, error);
    //CHECK_RET_EQ_EXIT(IP_D2BLENDER->REG_COLOR.all, 0x21, error);  // ARCS-C
    CHECK_RET_EQ_EXIT(IP_D2BLENDER->REG_COLOR.all, 0x00654321, error);
    CHECK_RET_EQ_EXIT(IP_D2BLENDER->REG_FIFO_BURST_THD.all, 0x08080808, error);
    CHECK_RET_EQ_EXIT(IP_D2BLENDER->REG_D2BLENDER_FORE_SIZE.all, 0x00001200, error);
    CHECK_RET_EQ_EXIT(IP_D2BLENDER->REG_D2BLENDER_BACK_SIZE.all, 0x00001200, error);
    CHECK_RET_EQ_EXIT(IP_D2BLENDER->REG_D2BLENDER_MASK_SIZE.all, 0x00000900, error);

    IP_AP_CFG->REG_SW_RESET.bit.VIDEO_RESET = 0x1;
    DELAY_MS(1);
    blender_reg_dump();
    VIDEO_LOG("[%s:%d]\r\n", __func__, __LINE__);

    CHECK_RET_EQ_EXIT(IP_D2BLENDER->REG_BLENDER_EN.all, 0, error);
    CHECK_RET_EQ_EXIT(IP_D2BLENDER->REG_BLENDER_CTRL.all, 0, error);
    CHECK_RET_EQ_EXIT(IP_D2BLENDER->REG_ALPHA.all, 0, error);
    CHECK_RET_EQ_EXIT(IP_D2BLENDER->REG_COLOR.all, 0, error);
    CHECK_RET_EQ_EXIT(IP_D2BLENDER->REG_FIFO_BURST_THD.all, 0, error);
    CHECK_RET_EQ_EXIT(IP_D2BLENDER->REG_D2BLENDER_FORE_SIZE.all, 0, error);
    CHECK_RET_EQ_EXIT(IP_D2BLENDER->REG_D2BLENDER_BACK_SIZE.all, 0, error);
    CHECK_RET_EQ_EXIT(IP_D2BLENDER->REG_D2BLENDER_MASK_SIZE.all, 0, error);

    ret = SUCCESS;

error:
    Blender_Start(blender_dev);
    Blender_Uninitialize(blender_dev);

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}

#if 0
#define REG_BASE_GP_DMAC     GPDMA_BASE
void test_d2blender_reg(void)               // test pass
{
    uint32_t i = 0;
    uint8_t d2back_buffer[96 * 96 * 2] = {0};
    uint8_t d2force_buffer[96 * 96 * 2] = {0};
    uint8_t d2mask_buffer[96 * 96] = {0};
    uint8_t d2out_buffer[96 * 96 * 2] = {0};

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    rgb565_colorbar_create((uint16_t *)d2back_buffer, 96, 96, 10);

    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_DMAC_GP_CLK = 0x1;          // 0x45000008 bit14
    IP_AP_CFG->REG_SW_RESET.bit.DMAC_GP_RESET = 0x0;
    DELAY_MS(10);
    IP_AP_CFG->REG_SW_RESET.bit.DMAC_GP_RESET = 0x1;
    DELAY_MS(10);

    __HAL_CRM_VIDEO_CLK_ENABLE();
    IP_AP_CFG->REG_SW_RESET.bit.VIDEO_RESET = 0x0;
    DELAY_MS(10);
    IP_AP_CFG->REG_SW_RESET.bit.VIDEO_RESET = 0x1;
    DELAY_MS(10);

    /* GP_DMAC 0x4510_0000 */
    /* d2back DMA CH1 */
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, d2back_hs_num3, 4, 28);       // DMA_CH1_CTRL  handsharking
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, gpdma_burst_len_8spl, 2, 14);       // src  0:1words  1:2words  2:4words  3:8words
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, gpdma_burst_len_8spl, 2, 16);       // dst  0:1words  1:2words  2:4words  3:8words
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, tfr_mode_m2p, 2, 4);        // 0:p2m  1:m2p  2:mem
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, inc_mode_increase, 1, 9);        // src address  0:add read  1:fix read
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, inc_mode_fix, 1, 10);       // dst address  0:add read  1:fix read
    mmio_write32(REG_BASE_GP_DMAC + 0x64, (uint32_t)d2back_buffer);     // DMA_SRC_ADDR0_CH1
    mmio_write32(REG_BASE_GP_DMAC + 0x6C, D2BACK_BUF);      // DMA_DST_ADDR0_CH1
    mmio_write32(REG_BASE_GP_DMAC + 0x30, sizeof(d2back_buffer) / sizeof(uint32_t));    // DMA_BLOCK_LEN_CH1: word

    /* d2out DMA CH2 */
    mmio_write32_field(REG_BASE_GP_DMAC + 0x08, d2out_hs_num1, 4, 28);       // DMA_CH1_CTRL  handsharking
    mmio_write32_field(REG_BASE_GP_DMAC + 0x08, gpdma_burst_len_8spl, 2, 14);       // src  0:1words  1:2words  2:4words  3:8words
    mmio_write32_field(REG_BASE_GP_DMAC + 0x08, gpdma_burst_len_8spl, 2, 16);       // dst  0:1words  1:2words  2:4words  3:8words
    mmio_write32_field(REG_BASE_GP_DMAC + 0x08, tfr_mode_m2p, 2, 4);        // 0:p2m  1:m2p  2:mem
    mmio_write32_field(REG_BASE_GP_DMAC + 0x08, inc_mode_fix, 1, 9);        // src address  0:add read  1:fix read
    mmio_write32_field(REG_BASE_GP_DMAC + 0x08, inc_mode_increase, 1, 10);       // dst address  0:add read  1:fix read
    mmio_write32(REG_BASE_GP_DMAC + 0x74, D2OUT_BUF);     // DMA_SRC_ADDR0_CH1
    mmio_write32(REG_BASE_GP_DMAC + 0x7C, (uint32_t)d2out_buffer);      // DMA_DST_ADDR0_CH1
    mmio_write32(REG_BASE_GP_DMAC + 0x34, sizeof(d2out_buffer) / sizeof(uint32_t));    // DMA_BLOCK_LEN_CH1: word

    /* test DMA CH0 */
    mmio_write32_field(REG_BASE_GP_DMAC + 0x00, d2fore_hs_num4, 4, 28);       // DMA_CH1_CTRL  handsharking
    mmio_write32_field(REG_BASE_GP_DMAC + 0x00, gpdma_burst_len_8spl, 2, 14);       // src  0:1words  1:2words  2:4words  3:8words
    mmio_write32_field(REG_BASE_GP_DMAC + 0x00, gpdma_burst_len_8spl, 2, 16);       // dst  0:1words  1:2words  2:4words  3:8words
    mmio_write32_field(REG_BASE_GP_DMAC + 0x00, tfr_mode_m2m, 2, 4);        // 0:p2m  1:m2p  2:mem
    mmio_write32_field(REG_BASE_GP_DMAC + 0x00, inc_mode_increase, 1, 9);        // src address  0:add read  1:fix read
    mmio_write32_field(REG_BASE_GP_DMAC + 0x00, inc_mode_increase, 1, 10);       // dst address  0:add read  1:fix read
    mmio_write32(REG_BASE_GP_DMAC + 0x54, (uint32_t)d2back_buffer);     // DMA_SRC_ADDR0_CH1
    mmio_write32(REG_BASE_GP_DMAC + 0x5C, (uint32_t)d2force_buffer);      // DMA_DST_ADDR0_CH1
    mmio_write32(REG_BASE_GP_DMAC + 0x2C, sizeof(d2back_buffer) / sizeof(uint32_t));    // DMA_BLOCK_LEN_CH1: word


    mmio_write32_field(REG_BASE_GP_DMAC + 0x08, 1, 1, 0);        // DMA channel  0:disable  1:enable
    mmio_write32_field(REG_BASE_GP_DMAC + 0x08, 1, 1, 1);        // DMA start

    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 1, 1, 0);        // DMA channel  0:disable  1:enable
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 1, 1, 1);        // DMA start

    mmio_write32_field(REG_BASE_GP_DMAC + 0x00, 1, 1, 0);        // DMA channel  0:disable  1:enable
    mmio_write32_field(REG_BASE_GP_DMAC + 0x00, 1, 1, 1);        // DMA start


    /* D2BLENDER 0x4480_7800 */
    //IP_D2BLENDER->REG_IMAGE_PROC_EN.bit.REG_CLK_FORCE_ON= 0x1;         // 0x00 bit0=1 reg_clk_force_on

    /* ALPHA_MODE=0 : alpha = (A * ALPHA_REG) >> 8 */
    /* ALPHA_MODE=1 : alpha = (mask_buf * ALPHA_REG) >> 8 */
    /* ALPHA_MODE=2 : alpha = ALPHA_REG */
    IP_D2BLENDER->REG_BLENDER_CTRL.bit.ALPHA_MODE = 0x2;               // 0x04

    /* 0£ºRGB565     1£ºRGB888     2£ºARGB8888 */
    IP_D2BLENDER->REG_BLENDER_CTRL.bit.BACK_COLOR_MODE = 0x0;          // 0x04

    /* 0£ºARGB8888     1£ºRGB888     2£ºRGB565     3£ºARGB1555     4£ºARGB4444     5£ºL8 */
    IP_D2BLENDER->REG_BLENDER_CTRL.bit.FORE_COLOR_MODE = 0x2;          // 0x04

    /* 0£ºfill mode(alpha_mode !=0 && fore_color_mode  invalid)  the force is come from color_reg    1£ºmap mode */
    IP_D2BLENDER->REG_BLENDER_CTRL.bit.BLENDER_MODE = 0x0;             // 0x04

    IP_D2BLENDER->REG_ALPHA.bit.ALPHA = 0x80;             // 0x08

    IP_D2BLENDER->REG_COLOR.bit.COLOR = 0x80;             // 0x0C

    IP_D2BLENDER->REG_FIFO_BURST_THD.bit.D2FORE_BURST_THD = 0x8;       // 0x10 bit[5:0]:
    IP_D2BLENDER->REG_FIFO_BURST_THD.bit.D2BACK_BURST_THD = 0x8;       // 0x10 bit[13:8]:
    IP_D2BLENDER->REG_FIFO_BURST_THD.bit.D2OUT_BURST_THD = 0x8;        // 0x10 bit[21:16]:
    IP_D2BLENDER->REG_FIFO_BURST_THD.bit.D2MASK_BURST_THD = 0x8;       // 0x10 bit[27:24]:

    mmio_write32(D2BLENDER_BASE + 0x14, sizeof(d2force_buffer) / sizeof(uint32_t));
    mmio_write32(D2BLENDER_BASE + 0x18, sizeof(d2back_buffer) / sizeof(uint32_t));
    mmio_write32(D2BLENDER_BASE + 0x1C, sizeof(d2mask_buffer) / sizeof(uint32_t));

    //IP_D2BLENDER->REG_IMAGE_PROC_EN.bit.REG_CLK_FORCE_ON= 0x0;         // 0x00 bit0=1 reg_clk_force_on
    mmio_write32_field(D2BLENDER_BASE + 0x00, 1, 1, 1);        // blender_en

    //mmio_write32(IMAGE_PROC_BASE + 0x0, 1);   // image proc enable
    mmio_write32(IMAGE_PROC_BASE + 0xC, 1);   // d2bledner enable



    DELAY_MS(1000);

    //gpdma_reg_dump();
    for(i = 0; i < 32; i++)
    {
        VIDEO_LOG("d2out[%d]=0x%x", i, d2out_buffer[i]);
    }

    VIDEO_LOG("0x00 IMAGE_PROC_EN   *0x%08x=0x%08x", &IP_D2BLENDER->REG_IMAGE_PROC_EN.all, IP_D2BLENDER->REG_IMAGE_PROC_EN.all);
    VIDEO_LOG("0x04 BLENDER_CTRL    *0x%08x=0x%08x", &IP_D2BLENDER->REG_BLENDER_CTRL.all, IP_D2BLENDER->REG_BLENDER_CTRL.all);
    VIDEO_LOG("0x08 ALPHA           *0x%08x=0x%08x", &IP_D2BLENDER->REG_ALPHA.all, IP_D2BLENDER->REG_ALPHA.all);
    VIDEO_LOG("0x0C COLOR           *0x%08x=0x%08x", &IP_D2BLENDER->REG_COLOR.all, IP_D2BLENDER->REG_COLOR.all);
    VIDEO_LOG("0x10 FIFO_BURST_THD  *0x%08x=0x%08x", &IP_D2BLENDER->REG_FIFO_BURST_THD.all, IP_D2BLENDER->REG_FIFO_BURST_THD.all);
    VIDEO_LOG("0x14 d2force_size=0x%08x", mmio_read32(D2BLENDER_BASE + 0x14));
    VIDEO_LOG("0x18 d2back_size=0x%08x", mmio_read32(D2BLENDER_BASE + 0x18));
    VIDEO_LOG("0x1C d2mask_size=0x%08x", mmio_read32(D2BLENDER_BASE + 0x1C));

    VIDEO_LOG("d2back_buffer:  *0x%08x=0x%x", d2back_buffer, d2back_buffer[0]);
    VIDEO_LOG("d2force_buffer: *0x%08x=0x%x", d2force_buffer, d2force_buffer[0]);
    VIDEO_LOG("d2mask_buffer:  *0x%08x=0x%x", d2mask_buffer, d2mask_buffer[0]);
    VIDEO_LOG("d2out_buffer:   *0x%08x=0x%x", d2out_buffer, d2out_buffer[0]);
    VIDEO_LOG("size: 0x%x byte", sizeof(d2out_buffer));

    for(i = 0; i < 10; i++)
    {
        if(mmio_read32_field(REG_BASE_GP_DMAC + 0x158, 1, i))
        {
            VIDEO_LOG("GPDMA CH%d finish", i);
        }
    }


    while(1);
}
#endif

#if 0
typedef enum {
    VIDEO_FORMAT_RAW8,
    VIDEO_FORMAT_ARGB1555,
    VIDEO_FORMAT_ARGB4444,
    VIDEO_FORMAT_RGB565,
    VIDEO_FORMAT_RBGR565,
    VIDEO_FORMAT_RGB888,
    VIDEO_FORMAT_BGR888,
    VIDEO_FORMAT_ARGB888,
    VIDEO_FORMAT_XRGB888,
    VIDEO_FORMAT_YUV422_UYVY,
    VIDEO_FORMAT_YUV422_VYUY,
    VIDEO_FORMAT_YUV422_YUYV,
    VIDEO_FORMAT_YUV422_YVYU,
} video_pixformat_t;

static uint8_t video_get_pixel_bitwidth(video_pixformat_t format)
{
    switch(format)
    {
        case VIDEO_FORMAT_RAW8:
            return 8;

        case VIDEO_FORMAT_ARGB1555:
        case VIDEO_FORMAT_ARGB4444:
        case VIDEO_FORMAT_RGB565:
        case VIDEO_FORMAT_RBGR565:
        case VIDEO_FORMAT_YUV422_UYVY:
        case VIDEO_FORMAT_YUV422_VYUY:
        case VIDEO_FORMAT_YUV422_YUYV:
        case VIDEO_FORMAT_YUV422_YVYU:
            return 16;

        case VIDEO_FORMAT_RGB888:
        case VIDEO_FORMAT_BGR888:
            return 24;

        case VIDEO_FORMAT_ARGB888:
        case VIDEO_FORMAT_XRGB888:
            return 32;

        default:
            return 0;
    }
}
#endif



