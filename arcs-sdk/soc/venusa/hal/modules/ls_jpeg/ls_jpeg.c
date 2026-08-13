#include "ls_jpeg.h"
#ifdef CFG_RTOS
#include <FreeRTOS.h>
#include <semphr.h>
#endif

#undef CLOGD
#define CLOGD 

#define CSK_JPEG_DMA2D_ADDR_ALIGNMENT                   32       /* dma2d addr alignment requirements about psram */
#define CSK_JPEG_DATA_ALIGN(a,b)     ((((a)+(b)-1)/(b))*(b))
#define CSK_JPEG_MCU_PIXEL_NUM                          64       /* jpeg min codec unit pixel num */
#define CSK_JPEG_QQFACTOR_MIN                           1        /* jpeg encoder min quantitative quality factor */
#define CSK_JPEG_QQFACTOR_MAX                           99       /* jpeg encoder max quantitative quality factor */
#define CSK_JPEG_DQT_QV_INDEX                           5        /* jpeg header DQT segment qtable value index */
#define CSK_JPEG_GRAY_HEADER_SIZE                       640      /* jpeg gray pic header size(not include ecs+eoi) need dcache line(32Byte) align 613+27 */
#define CSK_JPEG_YUV_HEADER_SIZE                        640      /* jpeg yuv pic header size(not include ecs+eoi) need dcache line(32Byte) align 623+17 */
#define CSK_JPEG_HEADER_DQT0_INDEX                      20
#define CSK_JPEG_HEADER_DQT1_INDEX                      89
#define CSK_JPEG_HEADER_SOF0_INDEX                      158


/* encode */
extern const uint32_t jpeg_enc_htable_golden[384];
extern uint32_t jpeg_enc_qtable_golden[128];

/* decode */
extern uint32_t jpeg_dec_base_table_golden[64];
extern uint32_t jpeg_dec_min_table_golden[16];
extern uint32_t jpeg_dec_symbol_table_golden[336];
extern uint32_t jpeg_dec_qtable_golden[256];

/* Static ECS output buffer in PSRAM to avoid malloc issues */
//#define JPEG_ECS_BUF_SIZE  (20 * 1024)  /* 20KB, enough for 320x240 */
//static uint8_t __attribute__((section(".psram_bss"), aligned(32))) jpeg_ecs_static_buf[JPEG_ECS_BUF_SIZE];

static uint8_t qt_luma[CSK_JPEG_DQT_QV_INDEX+CSK_JPEG_MCU_PIXEL_NUM] = {0xff,0xdb,0x00,0x43,0x00,0x08,0x06,0x06,0x07,0x06,0x05,0x08,0x07,0x07,0x07,0x09,0x09,0x08,0x0a,0x0c,0x14,0x0d,0x0c,0x0b,0x0b,0x0c,0x19,0x12,0x13,0x0f,0x14,0x1d,0x1a,0x1f,0x1e,0x1d,0x1a,
               0x1c,0x1c,0x20,0x24,0x2e,0x27,0x20,0x22,0x2c,0x23,0x1c,0x1c,0x28,0x37,0x29,0x2c,0x30,0x31,0x34,0x34,0x34,0x1f,0x27,0x39,0x3d,0x38,0x32,0x3c,0x2e,0x33,0x34,0x32};
static uint8_t qt_chroma[CSK_JPEG_DQT_QV_INDEX+CSK_JPEG_MCU_PIXEL_NUM] = {0xff,0xdb,0x00,0x43,0x01,0x09,0x09,0x09,0x0c,0x0b,0x0c,0x18,0x0d,0x0d,0x18,0x32,0x21,0x1c,0x21,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,
               0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32};

__attribute__((weak)) void dma2d_reg_dump(csk_dma2d_ch_t chn)
{
    (void)chn;
}


#define CHECK_RET_EQ(Ret, express)\
    do{\
        if ((express) != (Ret))\
        {\
            CLOGD("ret %d not equal with %d failed at %s: LINE: %d", (Ret), (express), __FUNCTION__, __LINE__);\
            return (Ret);\
        }\
    }while(0)

#define CHECK_POINT_NOT_NULL(point)\
    do{\
        if (NULL == (point))\
        {\
            CLOGD("point %s is NULL at %s: LINE: %d", #point, __FUNCTION__, __LINE__);\
            return 1;\
        }\
    }while(0)

#define CHECK_EQ_TIMEOUT_EXIT(value0, value1, timeout, errExit)\
    do{\
        if ((value0) != (value1))\
        {\
            break;\
        }\
        SysTick_Delay_Us(1); \
        if(timeout-- == 0) \
        { \
            CLOGD("[%s:%d] wait timeout", __func__, __LINE__); \
            ret = 1; \
            goto errExit; \
        } \
    }while(1)

static void jpeg_callback(Jpeg_emIrqEvent event, uint32_t param)
{
    //CLOGD("[%s:%d] event=%d", __func__, __LINE__, event);

    switch(event)
    {
        case JPEG_IRQ_EVENT_CODEC_DONE:
            CLOGD("[%s:%d] CODEC_DONE", __func__, __LINE__);
            break;

        case JPEG_IRQ_EVENT_PDMA_DONE:
            CLOGD("[%s:%d] PDMA_DONE", __func__, __LINE__);
            break;

        case JPEG_IRQ_EVENT_EDMA_DONE:
            CLOGD("[%s:%d] EDMA_DONE", __func__, __LINE__);
            break;

        default:
            CLOGD("[%s:%d] JPEG error event: %d", __func__, __LINE__, event);
            break;
    }
}


int32_t jpeg_init(Jpeg_InitTypeDef *pcfg)
{
    int32_t ret = 0;
    void *jpeg_dev = Jpeg0();

    CLOGD("[%s:%d]", __func__, __LINE__);

    ret = Jpeg_Initialize(jpeg_dev, jpeg_callback, pcfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


int32_t jpeg_deinit(void)
{
    int32_t ret = 0;
    void *jpeg_dev = Jpeg0();

    CLOGD("[%s:%d]", __func__, __LINE__);

    ret = Jpeg_Uninitialize(jpeg_dev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


int32_t jpeg_start(void)
{
    int32_t ret = 0;
    void *jpeg_dev = Jpeg0();

    CLOGD("[%s:%d]", __func__, __LINE__);

    ret = Jpeg_Start(jpeg_dev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


int32_t jpeg_stop(void)
{
    int32_t ret = 0;
    void *jpeg_dev = Jpeg0();

    CLOGD("[%s:%d]", __func__, __LINE__);

    ret = Jpeg_Stop(jpeg_dev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


void jpeg_reset(void)
{
    CLOGD("[%s:%d]", __func__, __LINE__);

    Jpeg_Reset();

    return;
}

void jpeg_refresh_quantization_table(uint32_t *qtable, uint16_t q_factor)
{
    static uint8_t standard_qt_luma[CSK_JPEG_MCU_PIXEL_NUM] = {    // T.81 appendix K.1,standard luminance quantization table
        16, 11, 10, 16, 24, 40, 51, 61,
        12, 12, 14, 19, 26, 58, 60, 55,
        14, 13, 16, 24, 40, 57, 69, 56,
        14, 17, 22, 29, 51, 87, 80, 62,
        18, 22, 37, 56, 68, 109, 103, 77,
        24, 35, 55, 64, 81, 104, 113, 92,
        49, 64, 78, 87, 103, 121, 120, 101,
        72, 92, 95, 98, 112, 100, 103, 99
    };
    static uint8_t standard_qt_chroma[CSK_JPEG_MCU_PIXEL_NUM] = {    // T.81 appendix K.2,standard chrominance quantization table
        17, 18, 24, 47, 99, 99, 99, 99,
        18, 21, 26, 66, 99, 99, 99, 99,
        24, 26, 56, 99, 99, 99, 99, 99,
        47, 66, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99
    };
    static uint16_t qlook[256] = {    // JPEG-IP quantization table mapping
        0x0000, 0x0001, 0x0400, 0x02ab, 0x0200, 0x0b33, 0x0155, 0x0a49, 0x0100, 0x09c7,
        0x00cd, 0x12e9, 0x0955, 0x093b, 0x1249, 0x0911, 0x0080, 0x08f1, 0x11c7, 0x11af,
        0x08cd, 0x08c3, 0x08ba, 0x08b2, 0x1155, 0x251f, 0x113b, 0x1a5f, 0x1a49, 0x1a35,
        0x1111, 0x2421, 0x0880, 0x23e1, 0x10f1, 0x10ea, 0x19c7, 0x19bb, 0x19af, 0x10d2,
        0x10cd, 0x231f, 0x10c3, 0x197d, 0x10ba, 0x10b6, 0x10b2, 0x22b9, 0x1955, 0x229d,
        0x10a4, 0x2d05, 0x193b, 0x1935, 0x225f, 0x1095, 0x2249, 0x223f, 0x2235, 0x2c57,
        0x1911, 0x2219, 0x1084, 0x1082, 0x1080, 0x18fc, 0x18f8, 0x21e9, 0x18f1, 0x21db,
        0x18ea, 0x2b9b, 0x21c7, 0x21c1, 0x21bb, 0x21b5, 0x21af, 0x2b53, 0x18d2, 0x367b,
        0x18cd, 0x2b29, 0x18c8, 0x218b, 0x18c3, 0x2b03, 0x217d, 0x2af1, 0x18ba, 0x18b8,
        0x18b6, 0x18b4, 0x18b2, 0x2ac1, 0x2ab9, 0x2159, 0x2155, 0x3547, 0x2a9d, 0x214b,
        0x18a4, 0x2a89, 0x2141, 0x189f, 0x213b, 0x189c, 0x2135, 0x34c9, 0x2a5f, 0x2a59,
        0x1895, 0x349d, 0x2a49, 0x1891, 0x2a3f, 0x211d, 0x2a35, 0x188c, 0x2a2b, 0x344d,
        0x2111, 0x343b, 0x2a19, 0x2a15, 0x1884, 0x3419, 0x1882, 0x1881, 0x1880, 0x20fe,
        0x20fc, 0x33e9, 0x20f8, 0x3fb3, 0x29e9, 0x33cb, 0x20f1, 0x33bd, 0x29db, 0x33af,
        0x20ea, 0x29d1, 0x20e7, 0x3395, 0x29c7, 0x20e2, 0x29c1, 0x20df, 0x29bb, 0x20dc,
        0x29b5, 0x20d9, 0x29af, 0x3359, 0x3353, 0x29a7, 0x20d2, 0x3343, 0x299f, 0x20ce,
        0x20cd, 0x2997, 0x3329, 0x20c9, 0x20c8, 0x298d, 0x298b, 0x3311, 0x20c3, 0x3e0f,
        0x3303, 0x3dfd, 0x297d, 0x297b, 0x2979, 0x32ed, 0x20ba, 0x3dc9, 0x20b8, 0x20b7,
        0x20b6, 0x20b5, 0x20b4, 0x20b3, 0x20b2, 0x3d89, 0x20b0, 0x32bd, 0x295d, 0x3d6b,
        0x2959, 0x2957, 0x2955, 0x32a7, 0x20a9, 0x20a8, 0x20a7, 0x3299, 0x294b, 0x3293,
        0x20a4, 0x20a3, 0x3289, 0x2943, 0x2941, 0x3cff, 0x209f, 0x3279, 0x293b, 0x3273,
        0x209c, 0x326d, 0x2935, 0x3ccf, 0x2099, 0x3cc3, 0x325f, 0x2097, 0x3259, 0x3cad,
        0x2095, 0x3251, 0x2927, 0x2093, 0x3249, 0x3c8d, 0x2091, 0x3c83, 0x323f, 0x3c79,
        0x291d, 0x3c6f, 0x3235, 0x3c65, 0x208c, 0x2917, 0x208b, 0x3229, 0x3227, 0x3c49,
        0x2911, 0x2088, 0x290f, 0x3c37, 0x3219, 0x3217, 0x3215, 0x3c25, 0x2084, 0x3c1d,
        0x2083, 0x2905, 0x2082, 0x2903, 0x2081, 0x2901
    };
    uint16_t i = 0;
    uint16_t j = 0;
    uint16_t scaler_factor = 0;
    uint16_t qt_value = 0;

    /* parameter validity check */
    if (CSK_JPEG_QQFACTOR_MIN > q_factor)
        q_factor = CSK_JPEG_QQFACTOR_MIN;
    else if (CSK_JPEG_QQFACTOR_MAX < q_factor)
        q_factor = CSK_JPEG_QQFACTOR_MAX;

    if (50 > q_factor) // low quality,significantly increase the quantization step size(more compressed)
        scaler_factor = 5000 / q_factor;
    else
        scaler_factor = 200 - 2 * q_factor;

    /* luma quantization table */
    for (i = 0; i < CSK_JPEG_MCU_PIXEL_NUM; i++)
    {
        qt_value = (standard_qt_luma[i] * scaler_factor + 50)/100;
        if (1 > qt_value)
            qt_luma[CSK_JPEG_DQT_QV_INDEX+i] = 1;
        else if (255 < qt_value)
            qt_luma[CSK_JPEG_DQT_QV_INDEX+i] = 255;
        else
            qt_luma[CSK_JPEG_DQT_QV_INDEX+i] = qt_value;
        qtable[j++] = qlook[qt_luma[CSK_JPEG_DQT_QV_INDEX+i]];
    }

    /* chroma quantization table */
    for (i = 0; i < CSK_JPEG_MCU_PIXEL_NUM; i++)
    {
        qt_value = (standard_qt_chroma[i] * scaler_factor + 50)/100;
        if (1 > qt_value)
            qt_chroma[CSK_JPEG_DQT_QV_INDEX+i] = 1;
        else if (255 < qt_value)
            qt_chroma[CSK_JPEG_DQT_QV_INDEX+i] = 255;
        else
            qt_chroma[CSK_JPEG_DQT_QV_INDEX+i] = qt_value;
        qtable[j++] = qlook[qt_chroma[CSK_JPEG_DQT_QV_INDEX+i]];
    }

    return;
}

void jpeg_send_huffman_table(const uint32_t *htable, uint32_t size_word)
{
    const uint32_t *src = htable;
    uint32_t *dts = (uint32_t *)Jpeg0_EncodeHuffBuf();
    uint32_t i = 0;

    for (i = 0; i < size_word; i++)
    {
        *dts++ = *src++;
    }
}


void jpeg_send_quantization_table(const uint32_t *qtable, uint32_t size_word)
{
    const uint32_t *src = qtable;
    uint32_t *dts = (uint32_t *)Jpeg0_EncodeQuanBuf();
    uint32_t i = 0;

    for (i = 0; i < size_word; i++)
    {
        *dts++ = *src++;
    }
}


void jpeg_send_base_table(const uint32_t *base_table, uint32_t size_word)
{
    const uint32_t *src = base_table;
    uint32_t *dts = (uint32_t *)Jpeg0_DecodeBaseBuf();
    uint32_t i = 0;

    for (i = 0; i < size_word; i++)
    {
        *dts++ = *src++;
    }
}


void jpeg_send_min_table(const uint32_t *min_table, uint32_t size_word)
{
    const uint32_t *src = min_table;
    uint32_t *dts = (uint32_t *)Jpeg0_DecodeMinBuf();
    uint32_t i = 0;

    for (i = 0; i < size_word; i++)
    {
        *dts++ = *src++;
    }
}


void jpeg_send_symbol_table(const uint32_t *symbol_table, uint32_t size_word)
{
    const uint32_t *src = symbol_table;
    uint32_t *dts = (uint32_t *)Jpeg0_DecodeSymBuf();
    uint32_t i = 0;

    for (i = 0; i < size_word; i++)
    {
        *dts++ = *src++;
    }
}


void jpeg_reg_dump(void)
{
    CLOGD("--------------------jpeg------------------------");
    CLOGD("0x004 CONTRL                  *0x%08x=0x%08x", &IP_JPEG_TOP->REG_CONTRL.all, IP_JPEG_TOP->REG_CONTRL.all);
    CLOGD("0x008 PIXEL_DMA_START         *0x%08x=0x%08x", &IP_JPEG_TOP->REG_PIXEL_DMA_START.all, IP_JPEG_TOP->REG_PIXEL_DMA_START.all);
    CLOGD("0x00C ECS_DMA_START           *0x%08x=0x%08x", &IP_JPEG_TOP->REG_ECS_DMA_START.all, IP_JPEG_TOP->REG_ECS_DMA_START.all);
    CLOGD("0x010 PIXEL_DMA_TRANSFER_SIZE *0x%08x=0x%08x", &IP_JPEG_TOP->REG_PIXEL_DMA_TRANSFER_SIZE.all, IP_JPEG_TOP->REG_PIXEL_DMA_TRANSFER_SIZE.all);
    CLOGD("0x014 ECS_DMA_TRANSFER_SIZE   *0x%08x=0x%08x", &IP_JPEG_TOP->REG_ECS_DMA_TRANSFER_SIZE.all, IP_JPEG_TOP->REG_ECS_DMA_TRANSFER_SIZE.all);
    CLOGD("0x018 SOURCE_DATA_LENGTH      *0x%08x=0x%08x", &IP_JPEG_TOP->REG_SOURCE_DATA_LENGTH.all, IP_JPEG_TOP->REG_SOURCE_DATA_LENGTH.all);
    CLOGD("0x01C RESULT_DATA_LENGTH      *0x%08x=0x%08x", &IP_JPEG_TOP->REG_RESULT_DATA_LENGTH.all, IP_JPEG_TOP->REG_RESULT_DATA_LENGTH.all);
    CLOGD("0x020 DEC_DUMMY               *0x%08x=0x%08x", &IP_JPEG_TOP->REG_DEC_DUMMY.all, IP_JPEG_TOP->REG_DEC_DUMMY.all);
    CLOGD("0x040 INT_ST_CLR              *0x%08x=0x%08x", &IP_JPEG_TOP->REG_INT_ST_CLR.all, IP_JPEG_TOP->REG_INT_ST_CLR.all);
    CLOGD("0x044 INT_MASK                *0x%08x=0x%08x", &IP_JPEG_TOP->REG_INT_MASK.all, IP_JPEG_TOP->REG_INT_MASK.all);
    CLOGD("0x060 SCALING_CTRL            *0x%08x=0x%08x", &IP_JPEG_TOP->REG_SCALING_CTRL.all, IP_JPEG_TOP->REG_SCALING_CTRL.all);
    CLOGD("0x064 ENC_PDMA_START          *0x%08x=0x%08x", &IP_JPEG_TOP->REG_ENC_PDMA_START.all, IP_JPEG_TOP->REG_ENC_PDMA_START.all);
    CLOGD("0x068 ENC_PIC_SIZE            *0x%08x=0x%08x", &IP_JPEG_TOP->REG_ENC_PIC_SIZE.all, IP_JPEG_TOP->REG_ENC_PIC_SIZE.all);
    CLOGD("0x06C DEC_PIC_SIZE            *0x%08x=0x%08x", &IP_JPEG_TOP->REG_DEC_PIC_SIZE.all, IP_JPEG_TOP->REG_DEC_PIC_SIZE.all);
    CLOGD("0x070 RELOAD                  *0x%08x=0x%08x", &IP_JPEG_TOP->REG_RELOAD.all, IP_JPEG_TOP->REG_RELOAD.all);
    CLOGD("0x074 IFCTRL                  *0x%08x=0x%08x", &IP_JPEG_TOP->REG_IFCTRL.all, IP_JPEG_TOP->REG_IFCTRL.all);
    CLOGD("0x800 JCR0                    *0x%08x=0x%08x", &IP_JPEG_TOP->REG_JCR0.all, IP_JPEG_TOP->REG_JCR0.all);
    CLOGD("0x804 JCR1                    *0x%08x=0x%08x", &IP_JPEG_TOP->REG_JCR1.all, IP_JPEG_TOP->REG_JCR1.all);
    CLOGD("0x808 JCR2                    *0x%08x=0x%08x", &IP_JPEG_TOP->REG_JCR2.all, IP_JPEG_TOP->REG_JCR2.all);
    CLOGD("0x80C JCR3                    *0x%08x=0x%08x", &IP_JPEG_TOP->REG_JCR3.all, IP_JPEG_TOP->REG_JCR3.all);
    CLOGD("0x810 JCR4                    *0x%08x=0x%08x", &IP_JPEG_TOP->REG_JCR4.all, IP_JPEG_TOP->REG_JCR4.all);
    CLOGD("0x814 JCR5                    *0x%08x=0x%08x", &IP_JPEG_TOP->REG_JCR5.all, IP_JPEG_TOP->REG_JCR5.all);
    CLOGD("0x818 JCR6                    *0x%08x=0x%08x", &IP_JPEG_TOP->REG_JCR6.all, IP_JPEG_TOP->REG_JCR6.all);
    CLOGD("0x81C JCR7                    *0x%08x=0x%08x", &IP_JPEG_TOP->REG_JCR7.all, IP_JPEG_TOP->REG_JCR7.all);
}


/********************** DMA2D *****************************************************************/
#ifndef CFG_RTOS
static volatile uint32_t jpeg_dma2d_output_finish_cnt = 0;
#else
static SemaphoreHandle_t jpeg_dma2d_output_finish_Semaphore = NULL;
#endif

static void jpeg_dma2d_output_callback(uint32_t event, void* workspace)
{
    CLOGD("[%s:%d] event=%d", __func__, __LINE__, event);
#ifndef CFG_RTOS
    jpeg_dma2d_output_finish_cnt++;
#else
    if (NULL != jpeg_dma2d_output_finish_Semaphore) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(jpeg_dma2d_output_finish_Semaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
#endif
}

uint32_t jpeg_dma2d_output_finish_cnt_get(void)
{
#ifndef CFG_RTOS
    return jpeg_dma2d_output_finish_cnt;
#else
    return 0;
#endif
}

void jpeg_img_format_codec2dma_switch(csk_dma_2d_image_cfg_t *img_cfg, Jpeg_CodecCfg *codec_cfg)
{
    switch (codec_cfg->pixel_format)
    {
        case JPEG_PIXEL_FORMAT_YUV422:
            img_cfg->img_input.img_format = csk_image_format_yuv422_yuyv_packed;
            img_cfg->img_input.img_line_stride = img_cfg->img_input.img_width * 2;
            break;
        case JPEG_PIXEL_FORMAT_YUV444:
            img_cfg->img_input.img_format = csk_image_format_yuv444_packed;
            img_cfg->img_input.img_line_stride = img_cfg->img_input.img_width * 3;
            break;
        case JPEG_PIXEL_FORMAT_RGB888:
            img_cfg->img_input.img_format = csk_image_format_rgb888;
            img_cfg->img_input.img_line_stride = img_cfg->img_input.img_width * 3;
            break;
        case JPEG_PIXEL_FORMAT_RGB565:
            img_cfg->img_input.img_format = csk_image_format_rgb565;
            img_cfg->img_input.img_line_stride = img_cfg->img_input.img_width * 2;
            break;
        case JPEG_PIXEL_FORMAT_GRAY:
            img_cfg->img_input.img_format = csk_image_format_y8;
            img_cfg->img_input.img_line_stride = img_cfg->img_input.img_width * 1;
            break;
        case JPEG_PIXEL_FORMAT_YUV420:
        case JPEG_PIXEL_FORMAT_YUV411:
        default:
            CLOGD("[%s:%d] unsupport pixel format=%d", __func__, __LINE__, codec_cfg->pixel_format);
            break;
    };
    img_cfg->img_output = img_cfg->img_input;

    return;
}

int32_t jpeg_encode_dma2d_init(csk_dma2d_ch_t in_ch, csk_dma2d_ch_t out_ch, Jpeg_CodecCfg *enc_cfg)
{
    int32_t ret;

    csk_dma2d_init_t jpeg_enc_input;
    memset(&jpeg_enc_input, 0, sizeof(jpeg_enc_input));

    jpeg_enc_input.dma_ch = in_ch;
    jpeg_enc_input.tfr_mode = tfr_mode_m2p;
    jpeg_enc_input.src_basic_unit = dma2d_sample_unit_word;
    jpeg_enc_input.dst_basic_unit = dma2d_sample_unit_word;
    jpeg_enc_input.src_inc_mode = inc_mode_increase;
    jpeg_enc_input.dst_inc_mode = inc_mode_fix;
    jpeg_enc_input.src_burst_len = dma2d_burst_len_16spl;
    jpeg_enc_input.dst_burst_len = dma2d_burst_len_8spl;
    jpeg_enc_input.flow_ctrl = dma2d_flow_ctrl_dma;
    jpeg_enc_input.prio_lvl = prio_mode_vhigh;
    jpeg_enc_input.handshake = jpg_p_hs_num5;

     csk_dma2d_init_t jpeg_enc_output;
     memset(&jpeg_enc_output, 0, sizeof(jpeg_enc_output));

    jpeg_enc_output.dma_ch = out_ch;
    jpeg_enc_output.tfr_mode = tfr_mode_p2m;
    jpeg_enc_output.src_basic_unit = dma2d_sample_unit_word;
    jpeg_enc_output.dst_basic_unit = dma2d_sample_unit_word;
    jpeg_enc_output.src_inc_mode = inc_mode_fix;
    jpeg_enc_output.dst_inc_mode = inc_mode_increase;
    jpeg_enc_output.src_burst_len = dma2d_burst_len_8spl;
    jpeg_enc_output.dst_burst_len = dma2d_burst_len_16spl;
    jpeg_enc_output.flow_ctrl = dma2d_flow_ctrl_peripheral;
    jpeg_enc_output.prio_lvl = prio_mode_vhigh;
    jpeg_enc_output.handshake = jpg_e_hs_num4;

#ifndef CFG_RTOS
    jpeg_dma2d_output_finish_cnt = 0;
#else
    if (NULL == jpeg_dma2d_output_finish_Semaphore && NULL == (jpeg_dma2d_output_finish_Semaphore = xSemaphoreCreateBinary()))
    {
        CLOGD("[%s:%d]create jpeg_dma2d_output_finish_Semaphore failed", __func__, __LINE__);
        return CSK_DRIVER_ERROR;
    }
#endif

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&jpeg_enc_input, NULL, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    if (NULL != enc_cfg)
    {
        csk_dma_2d_image_cfg_t jpeg_enc_input_img;
        memset(&jpeg_enc_input_img, 0, sizeof(jpeg_enc_input_img));

        jpeg_enc_input_img.img_jpeg_codec_mode = csk_jpeg_encode;
        jpeg_enc_input_img.img_input.img_width = enc_cfg->width;
        jpeg_enc_input_img.img_input.img_height = enc_cfg->height;
        jpeg_img_format_codec2dma_switch(&jpeg_enc_input_img, enc_cfg);
        ret = DMA2D_Image_Config_Extend(in_ch, &jpeg_enc_input_img);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    }

    ret = DMA2D_Config(&jpeg_enc_output, jpeg_dma2d_output_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}

int32_t jpeg_encode_dma2d_start(csk_dma2d_ch_t in_ch, void *in_buf, uint32_t in_size, csk_dma2d_ch_t out_ch, void *out_buf, uint32_t out_size)
{
    int32_t ret;

    CHECK_POINT_NOT_NULL(in_buf);
    CHECK_POINT_NOT_NULL(out_buf);

#ifndef CFG_RTOS
    jpeg_dma2d_output_finish_cnt = 0;
#endif

    ret = DMA2D_Start_Normal(out_ch, (uint32_t*)Jpeg0_ECSBuf(), (uint32_t*)out_buf, out_size);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Start_Normal(in_ch, (uint32_t*)in_buf, (uint32_t*)Jpeg0_PixelBuf(), in_size);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}

int32_t jpeg_encode_dma2d_stop(csk_dma2d_ch_t in_ch, csk_dma2d_ch_t out_ch)
{
    int32_t ret;

    ret = DMA2D_Stop(in_ch);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Stop(out_ch);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}

int32_t jpeg_decode_dma2d_init(csk_dma2d_ch_t in_ch, csk_dma2d_ch_t out_ch, Jpeg_CodecCfg *dec_cfg)
{
    int32_t ret = CSK_DRIVER_OK;

    csk_dma2d_init_t jpeg_dec_input;
    memset(&jpeg_dec_input, 0, sizeof(jpeg_dec_input));

    jpeg_dec_input.dma_ch = in_ch;
    jpeg_dec_input.tfr_mode = tfr_mode_m2p;
    jpeg_dec_input.src_basic_unit = dma2d_sample_unit_word;
    jpeg_dec_input.dst_basic_unit = dma2d_sample_unit_word;
    jpeg_dec_input.src_inc_mode = inc_mode_increase;
    jpeg_dec_input.dst_inc_mode = inc_mode_fix;
    jpeg_dec_input.src_burst_len = dma2d_burst_len_16spl;
    jpeg_dec_input.dst_burst_len = dma2d_burst_len_8spl;
    jpeg_dec_input.flow_ctrl = dma2d_flow_ctrl_dma;
    jpeg_dec_input.prio_lvl = prio_mode_vhigh;
    jpeg_dec_input.handshake = jpg_e_hs_num4;

    csk_dma2d_init_t jpeg_dec_output;
    memset(&jpeg_dec_output, 0, sizeof(jpeg_dec_output));

    jpeg_dec_output.dma_ch = out_ch;
    jpeg_dec_output.tfr_mode = tfr_mode_p2m;
    jpeg_dec_output.src_basic_unit = dma2d_sample_unit_word;
    jpeg_dec_output.dst_basic_unit = dma2d_sample_unit_word;
    jpeg_dec_output.src_inc_mode = inc_mode_fix;
    jpeg_dec_output.dst_inc_mode = inc_mode_increase;
    jpeg_dec_output.src_burst_len = dma2d_burst_len_8spl;
    jpeg_dec_output.dst_burst_len = dma2d_burst_len_16spl;
    jpeg_dec_output.flow_ctrl = dma2d_flow_ctrl_dma;
    jpeg_dec_output.prio_lvl = prio_mode_vhigh;
    jpeg_dec_output.handshake = jpg_p_hs_num5;

#ifndef CFG_RTOS
    jpeg_dma2d_output_finish_cnt = 0;
#else
    if (NULL == jpeg_dma2d_output_finish_Semaphore && NULL == (jpeg_dma2d_output_finish_Semaphore = xSemaphoreCreateBinary()))
    {
        CLOGD("[%s:%d]create jpeg_dma2d_output_finish_Semaphore failed", __func__, __LINE__);
        return CSK_DRIVER_ERROR;
    }
#endif

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&jpeg_dec_input, NULL, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&jpeg_dec_output, jpeg_dma2d_output_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    if (NULL != dec_cfg)
    {
        csk_dma_2d_image_cfg_t jpeg_dec_input_img;
        memset(&jpeg_dec_input_img, 0, sizeof(jpeg_dec_input_img));

        jpeg_dec_input_img.img_jpeg_codec_mode = csk_jpeg_decode;
        jpeg_dec_input_img.img_input.img_width = dec_cfg->width;
        jpeg_dec_input_img.img_input.img_height = dec_cfg->height;
        jpeg_img_format_codec2dma_switch(&jpeg_dec_input_img, dec_cfg);
        ret = DMA2D_Image_Config_Extend(out_ch, &jpeg_dec_input_img);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    }

    return ret;
}

int32_t jpeg_decode_dma2d_start(csk_dma2d_ch_t in_ch, void *in_buf, uint32_t in_size, csk_dma2d_ch_t out_ch, void *out_buf, uint32_t out_size)
{
    int32_t ret = CSK_DRIVER_OK;

    CHECK_POINT_NOT_NULL(in_buf);
    CHECK_POINT_NOT_NULL(out_buf);

#ifndef CFG_RTOS
    jpeg_dma2d_output_finish_cnt = 0;
#endif

    ret = DMA2D_Start_Normal(in_ch, (uint32_t*)in_buf, (uint32_t*)Jpeg0_ECSBuf(), in_size);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Start_Normal(out_ch, (uint32_t*)Jpeg0_PixelBuf(), (uint32_t*)out_buf, out_size);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}

int32_t jpeg_decode_dma2d_stop(csk_dma2d_ch_t in_ch, csk_dma2d_ch_t out_ch)
{
    int32_t ret = CSK_DRIVER_OK;

    ret = DMA2D_Stop(in_ch);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Stop(out_ch);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}

uint32_t jpeg_image_size(uint16_t width, uint16_t height, Jpeg_emFormatIn format)
{
    uint32_t size;

    switch(format)
    {
        case JPEG_DECODE_IN_FORMAT_YUV444:
            size = width * height * 3;
            break;

        case JPEG_DECODE_IN_FORMAT_YUV422:
            size = width * height * 2;
            break;

        case JPEG_DECODE_IN_FORMAT_YUV420:
        case JPEG_DECODE_IN_FORMAT_YUV411:
            size = width * height * 3 / 2;
            break;

        case JPEG_DECODE_IN_FORMAT_GRAY:
            size = width * height;
            break;

        default:
            size = width * height * 3 / 2;
            break;
    }

    return size;
}

#if 0
uint32_t jpeg_encoder_jpeg_build(const uint8_t *in_buf, uint32_t in_size, uint8_t *out_buf, uint32_t *out_size, Jpeg_InitTypeDef *jpeg_enc_cfg)
{
    uint32_t i = 0;
    int32_t ret = CSK_DRIVER_OK;
    uint32_t jpeg_index = 0;
    uint16_t segment_len = 0;
    uint8_t app0_jfif[18] = {0xff,0xe0,0x00,0x10,0x4a,0x46,0x49,0x46,0x00,0x01,0x01,0x01,0x00,0x78,0x00,0x78,0x00,0x00};
//    uint8_t qt0[69] = {0xff,0xdb,0x00,0x43,0x00,0x08,0x06,0x06,0x07,0x06,0x05,0x08,0x07,0x07,0x07,0x09,0x09,0x08,0x0a,0x0c,0x14,0x0d,0x0c,0x0b,0x0b,0x0c,0x19,0x12,0x13,0x0f,0x14,0x1d,0x1a,0x1f,0x1e,0x1d,0x1a,
//                       0x1c,0x1c,0x20,0x24,0x2e,0x27,0x20,0x22,0x2c,0x23,0x1c,0x1c,0x28,0x37,0x29,0x2c,0x30,0x31,0x34,0x34,0x34,0x1f,0x27,0x39,0x3d,0x38,0x32,0x3c,0x2e,0x33,0x34,0x32};
//    uint8_t qt1[69] = {0xff,0xdb,0x00,0x43,0x01,0x09,0x09,0x09,0x0c,0x0b,0x0c,0x18,0x0d,0x0d,0x18,0x32,0x21,0x1c,0x21,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,
//                       0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32};
    uint8_t huffm_dc0[33]={0xff,0xc4,0x00,0x1f,0x00,0x00,0x01,0x05,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b};
    uint8_t huffm_dc1[33]={0xff,0xc4,0x00,0x1f,0x01,0x00,0x03,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b};
    uint8_t huffm_ac0[183]={0xff,0xc4,0x00,0xb5,0x10,0x00,0x02,0x01,0x03,0x03,0x02,0x04,0x03,0x05,0x05,0x04,0x04,0x00,0x00,0x01,0x7d,0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,
                            0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,0x23,0x42,0xb1,0xc1,0x15,0x52,0xd1,0xf0,0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28,
                            0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
                            0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
                            0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,
                            0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa};
    uint8_t huffm_ac1[183]={0xff,0xc4,0x00,0xb5,0x11,0x00,0x02,0x01,0x02,0x04,0x04,0x03,0x04,0x07,0x05,0x04,0x04,0x00,0x01,0x02,0x77,0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
                            0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xa1,0xb1,0xc1,0x09,0x23,0x33,0x52,0xf0,0x15,0x62,0x72,0xd1,0x0a,0x16,0x24,0x34,0xe1,0x25,0xf1,0x17,0x18,0x19,0x1a,0x26,
                            0x27,0x28,0x29,0x2a,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,
                            0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,
                            0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,
                            0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa};
    uint8_t sof0_gray[13] = {0xff,0xc0,0x00,0x0b,0x08,0x00,0x00,0x00,0x00,0x01,0x01,0x11,0x00};
    uint8_t sof0_yuv[19] = {0xff,0xc0,0x00,0x11,0x08,0x00,0x00,0x00,0x00,0x03,0x01,0x22,0x00,0x02,0x11,0x01,0x03,0x11,0x01};
    uint8_t sos_gray[10] = {0xff,0xda,0x00,0x08,0x01,0x01,0x00,0x00,0x3f,0x00};
    uint8_t sos_yuv[14] = {0xff,0xda,0x00,0x0c,0x03,0x01,0x00,0x02,0x11,0x03,0x11,0x00,0x3f,0x00};

    /* JPEG Header */
    /* SOI */
    out_buf[jpeg_index++] = 0xff;
    out_buf[jpeg_index++] = 0xd8;

    /* APP* */
    /* APP0-JFIF Info */
    memcpy(&out_buf[jpeg_index], app0_jfif, sizeof(app0_jfif));
    jpeg_index += sizeof(app0_jfif);

    /* DQT */
    /* DQT0 */
    memcpy(&out_buf[jpeg_index], qt_luma, sizeof(qt_luma));
    jpeg_index += sizeof(qt_luma);

    /* DQT1 */
    memcpy(&out_buf[jpeg_index], qt_chroma, sizeof(qt_chroma));
    jpeg_index += sizeof(qt_chroma);

    /* SOF */
    /* SOF0 */
    if (JPEG_DECODE_IN_FORMAT_GRAY == jpeg_enc_cfg->format_in)
    {
        sof0_gray[5] = (jpeg_enc_cfg->img_height>>8)&0xff;
        sof0_gray[6] = jpeg_enc_cfg->img_height&0xff;
        sof0_gray[7] = (jpeg_enc_cfg->img_width>>8)&0xff;
        sof0_gray[8] = jpeg_enc_cfg->img_width&0xff;
        memcpy(&out_buf[jpeg_index], sof0_gray, sizeof(sof0_gray));
        jpeg_index += sizeof(sof0_gray);
    }
    else
    {
        sof0_yuv[5] = (jpeg_enc_cfg->img_height>>8)&0xff;
        sof0_yuv[6] = jpeg_enc_cfg->img_height&0xff;
        sof0_yuv[7] = (jpeg_enc_cfg->img_width>>8)&0xff;
        sof0_yuv[8] = jpeg_enc_cfg->img_width&0xff;
        if (JPEG_DECODE_IN_FORMAT_YUV444 == jpeg_enc_cfg->format_in)
            sof0_yuv[11] = 0x11;
        else if (JPEG_DECODE_IN_FORMAT_YUV422 == jpeg_enc_cfg->format_in)
            sof0_yuv[11] = 0x21;
        memcpy(&out_buf[jpeg_index], sof0_yuv, sizeof(sof0_yuv));
        jpeg_index += sizeof(sof0_yuv);
    }

    /* DHT */
    /* DC0 */
    memcpy(&out_buf[jpeg_index], huffm_dc0, sizeof(huffm_dc0));
    jpeg_index += sizeof(huffm_dc0);
    /* DC1 */
    memcpy(&out_buf[jpeg_index], huffm_dc1, sizeof(huffm_dc1));
    jpeg_index += sizeof(huffm_dc1);
    /* AC0 */
    memcpy(&out_buf[jpeg_index], huffm_ac0, sizeof(huffm_ac0));
    jpeg_index += sizeof(huffm_ac0);
    /* AC1 */
    memcpy(&out_buf[jpeg_index], huffm_ac1, sizeof(huffm_ac1));
    jpeg_index += sizeof(huffm_ac1);

    /* SOS */
    if (JPEG_DECODE_IN_FORMAT_GRAY == jpeg_enc_cfg->format_in)
    {
        memcpy(&out_buf[jpeg_index], sos_gray, sizeof(sos_gray));
        jpeg_index += sizeof(sos_gray);
    }
    else
    {
        memcpy(&out_buf[jpeg_index], sos_yuv, sizeof(sos_yuv));
        jpeg_index += sizeof(sos_yuv);
    }
    /* ECS */
    memcpy(&out_buf[jpeg_index], in_buf, in_size);
    jpeg_index += in_size;

    /* EOI */
    out_buf[jpeg_index++] = 0xff;
    out_buf[jpeg_index++] = 0xd9;

    *out_size = jpeg_index;

    return ret;
}
#endif

uint32_t jpeg_encoder_jpeg_build_ext(uint8_t *out_buf, Jpeg_InitTypeDef *jpeg_enc_cfg)
{
    static uint8_t header_gray[CSK_JPEG_GRAY_HEADER_SIZE] = {0xff,0xd8,0xff,0xe0,0x00,0x10,0x4a,0x46,0x49,0x46,0x00,0x01,0x01,0x01,0x00,0x78,0x00,0x78,0x00,0x00,0xff,0xdb,0x00,0x43,0x00,0x08,0x06,0x06,0x07,0x06,0x05,0x08,
                                                             0x07,0x07,0x07,0x09,0x09,0x08,0x0a,0x0c,0x14,0x0d,0x0c,0x0b,0x0b,0x0c,0x19,0x12,0x13,0x0f,0x14,0x1d,0x1a,0x1f,0x1e,0x1d,0x1a,0x1c,0x1c,0x20,0x24,0x2e,0x27,0x20,
                                                             0x22,0x2c,0x23,0x1c,0x1c,0x28,0x37,0x29,0x2c,0x30,0x31,0x34,0x34,0x34,0x1f,0x27,0x39,0x3d,0x38,0x32,0x3c,0x2e,0x33,0x34,0x32,0xff,0xdb,0x00,0x43,0x01,0x09,0x09,
                                                             0x09,0x0c,0x0b,0x0c,0x18,0x0d,0x0d,0x18,0x32,0x21,0x1c,0x21,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,
                                                             0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0xff,0xc0,
                                                             0x00,0x0b,0x08,0x00,0x00,0x00,0x00,0x01,0x01,0x11,0x00,0xff,0xc4,0x00,0x1f,0x00,0x00,0x01,0x05,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                                                             0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0xff,0xc4,0x00,0x1f,0x01,0x00,0x03,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,
                                                             0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0xff,0xc4,0x00,0xb5,0x10,0x00,0x02,0x01,0x03,0x03,0x02,0x04,0x03,0x05,0x05,0x04,0x04,0x00,0x00,
                                                             0x01,0x7d,0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,0x23,0x42,0xb1,0xc1,0x15,0x52,
                                                             0xd1,0xf0,0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28,0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,
                                                             0x48,0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x83,0x84,0x85,0x86,0x87,
                                                             0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,
                                                             0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,
                                                             0xf7,0xf8,0xf9,0xfa,0xff,0xc4,0x00,0xb5,0x11,0x00,0x02,0x01,0x02,0x04,0x04,0x03,0x04,0x07,0x05,0x04,0x04,0x00,0x01,0x02,0x77,0x00,0x01,0x02,0x03,0x11,0x04,0x05,
                                                             0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xa1,0xb1,0xc1,0x09,0x23,0x33,0x52,0xf0,0x15,0x62,0x72,0xd1,0x0a,0x16,0x24,
                                                             0x34,0xe1,0x25,0xf1,0x17,0x18,0x19,0x1a,0x26,0x27,0x28,0x29,0x2a,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x53,0x54,0x55,0x56,0x57,
                                                             0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x92,0x93,0x94,0x95,
                                                             0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,
                                                             0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xff,0xff,0xff,0xff,0xff,
                                                             0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xda,0x00,0x08,0x01,0x01,0x00,0x00,0x3f,0x00};

    static uint8_t header_yuv[CSK_JPEG_YUV_HEADER_SIZE] = {0xff,0xd8,0xff,0xe0,0x00,0x10,0x4a,0x46,0x49,0x46,0x00,0x01,0x01,0x01,0x00,0x78,0x00,0x78,0x00,0x00,0xff,0xdb,0x00,0x43,0x00,0x08,0x06,0x06,0x07,0x06,0x05,0x08,
                                                           0x07,0x07,0x07,0x09,0x09,0x08,0x0a,0x0c,0x14,0x0d,0x0c,0x0b,0x0b,0x0c,0x19,0x12,0x13,0x0f,0x14,0x1d,0x1a,0x1f,0x1e,0x1d,0x1a,0x1c,0x1c,0x20,0x24,0x2e,0x27,0x20,
                                                           0x22,0x2c,0x23,0x1c,0x1c,0x28,0x37,0x29,0x2c,0x30,0x31,0x34,0x34,0x34,0x1f,0x27,0x39,0x3d,0x38,0x32,0x3c,0x2e,0x33,0x34,0x32,0xff,0xdb,0x00,0x43,0x01,0x09,0x09,
                                                           0x09,0x0c,0x0b,0x0c,0x18,0x0d,0x0d,0x18,0x32,0x21,0x1c,0x21,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,
                                                           0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0xff,0xc0,
                                                           0x00,0x11,0x08,0x00,0x00,0x00,0x00,0x03,0x01,0x22,0x00,0x02,0x11,0x01,0x03,0x11,0x01,0xff,0xc4,0x00,0x1f,0x00,0x00,0x01,0x05,0x01,0x01,0x01,0x01,0x01,0x01,0x00,
                                                           0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0xff,0xc4,0x00,0x1f,0x01,0x00,0x03,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
                                                           0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0xff,0xc4,0x00,0xb5,0x10,0x00,0x02,0x01,0x03,0x03,0x02,0x04,0x03,
                                                           0x05,0x05,0x04,0x04,0x00,0x00,0x01,0x7d,0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,
                                                           0x23,0x42,0xb1,0xc1,0x15,0x52,0xd1,0xf0,0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28,0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,
                                                           0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,
                                                           0x7a,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,
                                                           0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,
                                                           0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xff,0xc4,0x00,0xb5,0x11,0x00,0x02,0x01,0x02,0x04,0x04,0x03,0x04,0x07,0x05,0x04,0x04,0x00,0x01,0x02,0x77,0x00,
                                                           0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xa1,0xb1,0xc1,0x09,0x23,0x33,0x52,0xf0,0x15,
                                                           0x62,0x72,0xd1,0x0a,0x16,0x24,0x34,0xe1,0x25,0xf1,0x17,0x18,0x19,0x1a,0x26,0x27,0x28,0x29,0x2a,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
                                                           0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x82,0x83,0x84,0x85,0x86,0x87,0x88,
                                                           0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,
                                                           0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,
                                                           0xfa,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xda,0x00,0x0c,0x03,0x01,0x00,0x02,0x11,0x03,0x11,0x00,0x3f,0x00};

    if (JPEG_DECODE_IN_FORMAT_GRAY == jpeg_enc_cfg->format_in)
    {
        /* DQT0 & DQT1 */
        memcpy(&header_gray[CSK_JPEG_HEADER_DQT0_INDEX], qt_luma, sizeof(qt_luma));
        memcpy(&header_gray[CSK_JPEG_HEADER_DQT1_INDEX], qt_chroma, sizeof(qt_chroma));

        /* SOF0 */
        header_gray[CSK_JPEG_HEADER_SOF0_INDEX+5] = (jpeg_enc_cfg->img_height>>8)&0xff;
        header_gray[CSK_JPEG_HEADER_SOF0_INDEX+6] = jpeg_enc_cfg->img_height&0xff;
        header_gray[CSK_JPEG_HEADER_SOF0_INDEX+7] = (jpeg_enc_cfg->img_width>>8)&0xff;
        header_gray[CSK_JPEG_HEADER_SOF0_INDEX+8] = jpeg_enc_cfg->img_width&0xff;

        memcpy(out_buf, header_gray, CSK_JPEG_GRAY_HEADER_SIZE);
        HAL_FlushDCache_by_Addr((uint32_t*)out_buf, CSK_JPEG_GRAY_HEADER_SIZE);
        return CSK_JPEG_GRAY_HEADER_SIZE;
    }
    else
    {
        /* DQT0 & DQT1 */
        memcpy(&header_yuv[CSK_JPEG_HEADER_DQT0_INDEX], qt_luma, sizeof(qt_luma));
        memcpy(&header_yuv[CSK_JPEG_HEADER_DQT1_INDEX], qt_chroma, sizeof(qt_chroma));

        /* SOF0 */
        header_yuv[CSK_JPEG_HEADER_SOF0_INDEX+5] = (jpeg_enc_cfg->img_height>>8)&0xff;
        header_yuv[CSK_JPEG_HEADER_SOF0_INDEX+6] = jpeg_enc_cfg->img_height&0xff;
        header_yuv[CSK_JPEG_HEADER_SOF0_INDEX+7] = (jpeg_enc_cfg->img_width>>8)&0xff;
        header_yuv[CSK_JPEG_HEADER_SOF0_INDEX+8] = jpeg_enc_cfg->img_width&0xff;
        if (JPEG_DECODE_IN_FORMAT_YUV444 == jpeg_enc_cfg->format_in)
            header_yuv[CSK_JPEG_HEADER_SOF0_INDEX+11] = 0x11;
        else if (JPEG_DECODE_IN_FORMAT_YUV422 == jpeg_enc_cfg->format_in)
            header_yuv[CSK_JPEG_HEADER_SOF0_INDEX+11] = 0x21;

        memcpy(out_buf, header_yuv, CSK_JPEG_YUV_HEADER_SIZE);
        HAL_FlushDCache_by_Addr((uint32_t*)out_buf, CSK_JPEG_YUV_HEADER_SIZE);
        return CSK_JPEG_YUV_HEADER_SIZE;
    }
}

uint32_t jpeg_encoder_mcu_extract_gray(const uint8_t *in_buf, uint8_t **mcu_buf, uint8_t **mcu_buf_ori, Jpeg_CodecCfg *enc_cfg, Jpeg_InitTypeDef *jpeg_enc_cfg)
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t w = 0;
    int32_t h = 0;
    int32_t ret = CSK_DRIVER_OK;
    uint8_t *band = NULL;
    uint8_t *wp = NULL;
    uint8_t *wp_last = NULL;
    uint32_t pix_index = 0;
    uint32_t mcu_index = 0;
    uint8_t *mcu_data = NULL;
    uint8_t *mcu_data_ori = NULL;
    uint16_t padding_v = jpeg_enc_cfg->img_height_align - jpeg_enc_cfg->img_height;

    /* Allocate memory for lines buffer */
    if (NULL == (band = (uint8_t *)malloc(jpeg_enc_cfg->sampling_v*jpeg_enc_cfg->img_width_align)))
    {
        CLOGD("[%s:%d]malloc failed:size=%u", __func__, __LINE__);
        return CSK_DRIVER_ERROR;
    }

    if (NULL == (mcu_data_ori = (uint8_t*)malloc(jpeg_enc_cfg->pixel_size + CSK_JPEG_DMA2D_ADDR_ALIGNMENT - 1)))
    {
        free(band);
        CLOGD("[%s:%d]malloc failed:size=%u", __func__, __LINE__, jpeg_enc_cfg->pixel_size);
        return CSK_DRIVER_ERROR;
    }
    mcu_data = (uint8_t*)(((size_t)mcu_data_ori + CSK_JPEG_DMA2D_ADDR_ALIGNMENT - 1) & ~(CSK_JPEG_DMA2D_ADDR_ALIGNMENT - 1));

    /* Process 8 lines at a time */
    for(h = 0; h < jpeg_enc_cfg->img_height_align; h += jpeg_enc_cfg->sampling_v)
    {
        /* Read and colour convert */
        wp = band;
        for(i = 0; i < jpeg_enc_cfg->sampling_v;)
        {
            for(w = 0; w < jpeg_enc_cfg->img_width_align; w++)
            {
                *wp++ = in_buf[pix_index];
                if (w < jpeg_enc_cfg->img_width)
                {
                    pix_index++;
                }
            }
            i++;
            if (padding_v && (h + i) >= jpeg_enc_cfg->img_height)
            {
                wp_last = wp - jpeg_enc_cfg->img_width_align;
                for (j = 0; j < padding_v; j++)
                {
                    memcpy(wp+j*jpeg_enc_cfg->img_width_align, wp_last, jpeg_enc_cfg->img_width_align);
                }
                break;
            }
        }

        /* Write as 8X8 blocks (Y) */
        for(w = 0; w < jpeg_enc_cfg->img_width_align; w += jpeg_enc_cfg->sampling_h)
        {
            /* Grab a 8X8 block for each component */
            wp = band + w;
            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    mcu_data[mcu_index++] = *wp++;
                }
                wp += (jpeg_enc_cfg->img_width_align - jpeg_enc_cfg->sampling_h);
            }
        }
    }

    HAL_FlushDCache_by_Addr((uint32_t*)mcu_data, jpeg_enc_cfg->pixel_size);
    *mcu_buf = mcu_data;
    *mcu_buf_ori = mcu_data_ori;
    free(band);

    return ret;
}

uint32_t jpeg_encoder_mcu_extract_rgb(const uint8_t *in_buf, uint8_t **mcu_buf, uint8_t **mcu_buf_ori, Jpeg_CodecCfg *enc_cfg, Jpeg_InitTypeDef *jpeg_enc_cfg)
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t w = 0;
    int32_t h = 0;
    uint8_t pixel_bw = 3;
    int32_t ret = CSK_DRIVER_OK;
    uint8_t *band = NULL;
    uint8_t *wp = NULL;
    uint8_t *wp_last = NULL;
    int32_t yb[16][16],cbb[16][16],crb[16][16];
    int32_t y = 0;
    int32_t cb = 0;
    int32_t cr = 0;
    int32_t *c0 = NULL;
    int32_t *c1 = NULL;
    int32_t *c2 = NULL;
    uint32_t pix_index = 0;
    uint32_t mcu_index = 0;
    uint8_t *mcu_data = NULL;
    uint8_t *mcu_data_ori = NULL;
    uint32_t linesize = jpeg_enc_cfg->img_width_align*pixel_bw;
    uint32_t block_offset = (jpeg_enc_cfg->img_width_align - jpeg_enc_cfg->sampling_h)*pixel_bw;
    uint16_t padding_v = jpeg_enc_cfg->img_height_align - jpeg_enc_cfg->img_height;

    /* Allocate memory for lines buffer */
    if (NULL == (band = (uint8_t *)malloc(jpeg_enc_cfg->sampling_v*linesize)))
    {
        CLOGD("[%s:%d]malloc failed:size=%u", __func__, __LINE__);
        return CSK_DRIVER_ERROR;
    }

    if (NULL == (mcu_data_ori = (uint8_t*)malloc(jpeg_enc_cfg->pixel_size + CSK_JPEG_DMA2D_ADDR_ALIGNMENT - 1)))
    {
        free(band);
        CLOGD("[%s:%d]malloc failed:size=%u", __func__, __LINE__, jpeg_enc_cfg->pixel_size);
        return CSK_DRIVER_ERROR;
    }
    mcu_data = (uint8_t*)(((size_t)mcu_data_ori + CSK_JPEG_DMA2D_ADDR_ALIGNMENT - 1) & ~(CSK_JPEG_DMA2D_ADDR_ALIGNMENT - 1));

    /* Process 16 lines at a time */
    for(h = 0; h < jpeg_enc_cfg->img_height_align; h += jpeg_enc_cfg->sampling_v)
    {
        /* Read and colour convert */
        wp = band;
        for(i = 0; i < jpeg_enc_cfg->sampling_v;)
        {
            for(w = 0; w < jpeg_enc_cfg->img_width_align; w++)
            {
                if (w < jpeg_enc_cfg->img_width)
                {
                    y = 19595*in_buf[pix_index]+38470*in_buf[pix_index+1]+7471*in_buf[pix_index+2];
                    y += 32768;
                    y /= 65536;
                    if (y < 0)
                        y = 0;
                    else if (y > 255)
                        y = 255;
                    cb = -11056*in_buf[pix_index]-21712*in_buf[pix_index+1]+32768*in_buf[pix_index+2]+8388608;
                    cb += 32768;
                    cb /= 65536;
                    if (cb < 0)
                        cb=0;
                    else if (cb > 255)
                        cb=255;
                    cr = 32768*in_buf[pix_index]-27440*in_buf[pix_index+1]-5328*in_buf[pix_index+2]+8388608;
                    cr += 32768;
                    cr /= 65536;
                    if (cr < 0)
                        cr=0;
                    else if (cr > 255)
                        cr=255;
                    pix_index += pixel_bw;
                }
                *wp++=(unsigned char)y;
                *wp++=(unsigned char)cb;
                *wp++=(unsigned char)cr;
            }
            i++;
            if (padding_v && (h + i) >= jpeg_enc_cfg->img_height)
            {
                wp_last = wp - linesize;
                for (j = 0; j < padding_v; j++)
                {
                    memcpy(wp+j*linesize, wp_last, linesize);
                }
                break;
            }
        }

        /* Write as 8X8 blocks (4Y+Cb+Cr) */
        for(w = 0; w < jpeg_enc_cfg->img_width_align; w += jpeg_enc_cfg->sampling_h)
        {
            /* Grab a 16X16 block for each component */
            wp = band + pixel_bw * w;
            c0 = &yb[0][0];
            c1 = &cbb[0][0];
            c2 = &crb[0][0];
            for(i = 0; i < 16; i++)
            {
                for(j = 0; j < 16; j++)
                {
                    *c0++ = (int32_t)*wp++;
                    *c1++ = (int32_t)*wp++;
                    *c2++ = (int32_t)*wp++;
                }
                wp += block_offset;
            }

            /* Output 4 Y blocks */
            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    mcu_data[mcu_index++] = yb[i][j];
                }
            }
            for(i = 0; i < 8; i++)
            {
                for(j = 8; j < 16; j++)
                {
                    mcu_data[mcu_index++] = yb[i][j];
                }
            }
            for(i = 8; i < 16; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    mcu_data[mcu_index++] = yb[i][j];
                }
            }
            for(i = 8; i < 16; i++)
            {
                for(j = 8; j < 16; j++)
                {
                    mcu_data[mcu_index++] = yb[i][j];
                }
            }

            /* Output Cb block */
            for(i = 0; i < 16; i += 2)
            {
                for(j = 0; j < 16; j += 2)
                {
                    cb = cbb[i][j] + cbb[i][j+1] + cbb[i+1][j] + cbb[i+1][j+1] + 2;
                    cb >>= 2;
                    mcu_data[mcu_index++] = cb;
                }
            }

            /* Output Cr block */
            for(i=0;i<16;i+=2)
            {
                for(j=0;j<16;j+=2)
                {
                    cr = crb[i][j]+crb[i][j+1]+crb[i+1][j]+crb[i+1][j+1]+2;
                    cr >>= 2;
                    mcu_data[mcu_index++] = cr;
                }
            }
        }
    }

    HAL_FlushDCache_by_Addr((uint32_t*)mcu_data, jpeg_enc_cfg->pixel_size);
    *mcu_buf = mcu_data;
    *mcu_buf_ori = mcu_data_ori;
    free(band);

    return ret;
}

uint32_t jpeg_encoder_mcu_extract_yuv(const uint8_t *in_buf, uint8_t **mcu_buf, uint8_t **mcu_buf_ori, Jpeg_CodecCfg *enc_cfg, Jpeg_InitTypeDef *jpeg_enc_cfg)
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t w = 0;
    int32_t h = 0;
    uint8_t pixel_bw = (JPEG_PIXEL_FORMAT_YUV422 == enc_cfg->pixel_format) ? 2 : 3;
    int32_t ret = CSK_DRIVER_OK;
    int32_t yb[8][16],cbb[8][8],crb[8][8];
    int32_t *c0 = NULL;
    int32_t *c1 = NULL;
    int32_t *c2 = NULL;
    uint32_t pix_index = 0;
    uint32_t mcu_index = 0;
    uint8_t *mcu_data = NULL;
    uint8_t *mcu_data_ori = NULL;
    uint32_t block_offset = (jpeg_enc_cfg->img_width - jpeg_enc_cfg->sampling_h)*pixel_bw;
    uint16_t padding_v = jpeg_enc_cfg->img_height_align - jpeg_enc_cfg->img_height;

    if (NULL == (mcu_data_ori = (uint8_t*)malloc(jpeg_enc_cfg->pixel_size + CSK_JPEG_DMA2D_ADDR_ALIGNMENT - 1)))
    {
        CLOGD("[%s:%d]malloc failed:size=%u", __func__, __LINE__, jpeg_enc_cfg->pixel_size);
        return CSK_DRIVER_ERROR;
    }
    mcu_data = (uint8_t*)(((size_t)mcu_data_ori + CSK_JPEG_DMA2D_ADDR_ALIGNMENT - 1) & ~(CSK_JPEG_DMA2D_ADDR_ALIGNMENT - 1));

    /* Process 8/16 lines at a time */
    for(h = 0; h < jpeg_enc_cfg->img_height_align; h += jpeg_enc_cfg->sampling_v)
    {
        /* Write as 8X8/8x16 blocks (Y+Cb+Cr/2Y+Cb+Cr) */
        for(w = 0; w < jpeg_enc_cfg->img_width_align; w += jpeg_enc_cfg->sampling_h)
        {
            pix_index = (h * jpeg_enc_cfg->img_width + w) * pixel_bw;

            /* Grab a 8x8/8X16 block for each component */
            for(i = 0; i < jpeg_enc_cfg->sampling_v;)
            {
                c0 = &yb[i][0];
                c1 = &cbb[i][0];
                c2 = &crb[i][0];
                for(j = 0; j < jpeg_enc_cfg->sampling_h; j++)
                {
                    *c0++ = in_buf[pix_index];
                    if (3 == pixel_bw)
                    {
                        *c1++ = in_buf[pix_index+1];
                        *c2++ = in_buf[pix_index+2];
                    }
                    else
                    {
                        if (0 == j%2)
                            *c1++ = in_buf[pix_index+1];
                        else
                            *c2++ = in_buf[pix_index+1];
                    }
                    if (w < jpeg_enc_cfg->img_width)
                    {
                        pix_index += pixel_bw;
                    }
                }
                i++;
                if (padding_v && (h + i) >= jpeg_enc_cfg->img_height)
                {
                    for (; i < jpeg_enc_cfg->sampling_v; i++)
                    {
                        memcpy(&yb[i][0], &yb[i-1][0], sizeof(yb[0]));
                        memcpy(&cbb[i][0], &cbb[i-1][0], sizeof(cbb[0]));
                        memcpy(&crb[i][0], &crb[i-1][0], sizeof(crb[0]));
                    }
                    break;
                }
                pix_index += block_offset;
            }

            /* Output Y blocks */
            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    mcu_data[mcu_index++] = yb[i][j];
                }
            }
            if (JPEG_PIXEL_FORMAT_YUV422 == enc_cfg->pixel_format)
            {
                for(i = 0; i < 8; i++)
                {
                    for(j = 8; j < 16; j++)
                    {
                        mcu_data[mcu_index++] = yb[i][j];
                    }
                }
            }

            /* Output Cb block */
            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    mcu_data[mcu_index++] = cbb[i][j];
                }
            }

            /* Output Cr block */
            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    mcu_data[mcu_index++] = crb[i][j];
                }
            }
        }
    }

    HAL_FlushDCache_by_Addr((uint32_t*)mcu_data, jpeg_enc_cfg->pixel_size);
    *mcu_buf = mcu_data;
    *mcu_buf_ori = mcu_data_ori;

    return ret;
}

uint32_t jpeg_encoder_mcu_extract(const uint8_t *in_buf, uint8_t **mcu_buf, uint8_t **mcu_buf_ori, Jpeg_CodecCfg *enc_cfg, Jpeg_InitTypeDef *jpeg_enc_cfg)
{
    if (JPEG_PIXEL_FORMAT_GRAY == enc_cfg->pixel_format)
        return jpeg_encoder_mcu_extract_gray(in_buf, mcu_buf, mcu_buf_ori, enc_cfg, jpeg_enc_cfg);
    else if (JPEG_PIXEL_FORMAT_RGB888 == enc_cfg->pixel_format)
        return jpeg_encoder_mcu_extract_rgb(in_buf, mcu_buf, mcu_buf_ori, enc_cfg, jpeg_enc_cfg);
    else
        return jpeg_encoder_mcu_extract_yuv(in_buf, mcu_buf, mcu_buf_ori, enc_cfg, jpeg_enc_cfg);
}

void jpeg_img_format_codec2jpeg_switch(Jpeg_CodecCfg *codec_cfg, Jpeg_InitTypeDef *img_cfg)
{
    switch (codec_cfg->pixel_format)
    {
        case JPEG_PIXEL_FORMAT_YUV422:
            img_cfg->format_in = JPEG_DECODE_IN_FORMAT_YUV422;
            img_cfg->sampling_h = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
            img_cfg->sampling_v = CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
            break;
        case JPEG_PIXEL_FORMAT_YUV444:
            img_cfg->format_in = JPEG_DECODE_IN_FORMAT_YUV444;
            img_cfg->sampling_h = CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
            img_cfg->sampling_v = CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
            break;
        case JPEG_PIXEL_FORMAT_GRAY:
            img_cfg->format_in = JPEG_DECODE_IN_FORMAT_GRAY;
            img_cfg->sampling_h = CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
            img_cfg->sampling_v = CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
            break;
        case JPEG_PIXEL_FORMAT_YUV420:
            img_cfg->format_in = JPEG_DECODE_IN_FORMAT_YUV420;
            img_cfg->sampling_h = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
            img_cfg->sampling_v = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
            break;
        case JPEG_PIXEL_FORMAT_YUV411:
            img_cfg->format_in = JPEG_DECODE_IN_FORMAT_YUV411;
            img_cfg->sampling_h = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
            img_cfg->sampling_v = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
            break;
        case JPEG_PIXEL_FORMAT_RGB888:
        case JPEG_PIXEL_FORMAT_RGB565:
        default:
            CLOGD("[%s:%d] unsupport pixel format=%d", __func__, __LINE__, codec_cfg->pixel_format);
            break;
    };

    return;
}

void jpeg_img_format_jpeg2codec_switch(Jpeg_InitTypeDef *img_cfg, Jpeg_CodecCfg *codec_cfg)
{
    switch (img_cfg->format_in)
    {
        case JPEG_DECODE_IN_FORMAT_YUV422:
            codec_cfg->pixel_format = JPEG_PIXEL_FORMAT_YUV422;
            break;
        case JPEG_DECODE_IN_FORMAT_YUV444:
            codec_cfg->pixel_format = JPEG_PIXEL_FORMAT_YUV444;
            break;
        case JPEG_DECODE_IN_FORMAT_GRAY:
            codec_cfg->pixel_format = JPEG_PIXEL_FORMAT_GRAY;
            break;
        case JPEG_DECODE_IN_FORMAT_YUV420:
            codec_cfg->pixel_format = JPEG_PIXEL_FORMAT_YUV420;
            break;
        case JPEG_DECODE_IN_FORMAT_YUV411:
            codec_cfg->pixel_format = JPEG_PIXEL_FORMAT_YUV411;
            break;
        default:
            CLOGD("[%s:%d] unsupport pixel format=%d", __func__, __LINE__, img_cfg->format_in);
            break;
    };

    return;
}

uint32_t jpeg_encoder(void *in_buf, void *out_buf, uint32_t *out_size, Jpeg_CodecCfg *enc_cfg)
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t timeout = 0;
    uint8_t *mcu_data = NULL;
    uint8_t *mcu_data_ori = NULL;
    uint32_t ecs_len = 0;
    uint32_t jpeg_header_len = 0;
    Jpeg_InitTypeDef jpeg_enc_cfg = {
            .mode = JPEG_MODE_ENCODE,
            .format_in = JPEG_DECODE_IN_FORMAT_YUV420,
            .format_out = JPEG_DECODE_IN_FORMAT_YUV420,
            .ecs_size = 0,
            .rst_enable = 0,
            .rst_num = 0,
            .sampling_h = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .sampling_v = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .qt_index = {0x00,0x01,0x01,0x00},
            .ht_index = {0x00,0x11,0x11,0x00}
    };

    /* switch image to mcu block */
    jpeg_img_format_codec2jpeg_switch(enc_cfg, &jpeg_enc_cfg);
    jpeg_enc_cfg.img_width = enc_cfg->width;
    jpeg_enc_cfg.img_height = enc_cfg->height;
    jpeg_enc_cfg.img_width_align = CSK_JPEG_DATA_ALIGN(jpeg_enc_cfg.img_width, jpeg_enc_cfg.sampling_h);
    jpeg_enc_cfg.img_height_align = CSK_JPEG_DATA_ALIGN(jpeg_enc_cfg.img_height, jpeg_enc_cfg.sampling_v);
    jpeg_enc_cfg.pixel_size = jpeg_image_size(jpeg_enc_cfg.img_width_align, jpeg_enc_cfg.img_height_align, jpeg_enc_cfg.format_in);
    jpeg_enc_cfg.src_size = jpeg_enc_cfg.pixel_size;
    jpeg_encoder_mcu_extract(in_buf, &mcu_data, &mcu_data_ori, enc_cfg, &jpeg_enc_cfg);

    jpeg_init(&jpeg_enc_cfg);
    jpeg_send_huffman_table(jpeg_enc_htable_golden, sizeof(jpeg_enc_htable_golden)/sizeof(uint32_t));
    jpeg_send_quantization_table(jpeg_enc_qtable_golden, sizeof(jpeg_enc_qtable_golden)/sizeof(uint32_t));
    jpeg_start();

    jpeg_header_len = jpeg_encoder_jpeg_build_ext(out_buf, &jpeg_enc_cfg);
    jpeg_encode_dma2d_init(dma_2d_ch2, dma_2d_ch1, NULL);
    jpeg_encode_dma2d_start(dma_2d_ch2, (void*)mcu_data, jpeg_enc_cfg.pixel_size, dma_2d_ch1, (void*)((uint8_t*)out_buf+jpeg_header_len), jpeg_enc_cfg.pixel_size);

    /* wait done */
#ifndef CFG_RTOS
    timeout = 1000000; // us
    CHECK_EQ_TIMEOUT_EXIT(jpeg_dma2d_output_finish_cnt_get(), 0, timeout, error0);
#else
    if (NULL != jpeg_dma2d_output_finish_Semaphore)
        xSemaphoreTake(jpeg_dma2d_output_finish_Semaphore, portMAX_DELAY);
#endif
    ecs_len = IP_JPEG_TOP->REG_RESULT_DATA_LENGTH.bit.RESULT_LEN;

    /* FlushInvalidate: flush header tail data, then invalidate for DMA result */
    HAL_InvalidateDCache_by_Addr((uint32_t*)((uint8_t*)out_buf+jpeg_header_len), ecs_len);
    *((uint8_t*)out_buf+(jpeg_header_len+ecs_len)) = 0xff;
    *((uint8_t*)out_buf+(jpeg_header_len+ecs_len+1)) = 0xd9;
    *out_size = jpeg_header_len+ecs_len+2;

error0:
    dma2d_reg_dump(dma_2d_ch2);
    dma2d_reg_dump(dma_2d_ch1);
    jpeg_encode_dma2d_stop(dma_2d_ch2, dma_2d_ch1);
error1:
    jpeg_reg_dump();
    jpeg_stop();
    jpeg_deinit();

    CLOGD("jpeg_enc_input_buffer:  0x%08x", in_buf);
    CLOGD("jpeg_enc_pixin_buffer:  0x%08x,0x%08x", mcu_data, mcu_data_ori);
    CLOGD("jpeg_enc_ecsout_buffer: 0x%08x,len=%#x", out_buf+jpeg_header_len, ecs_len);
    CLOGD("jpeg_enc_out_buffer:    0x%08x,len=%#x", out_buf, *out_size);

    if(ret == 0)
        CLOGD("[%s:%d] test SUCCESS", __func__, __LINE__);
    else
        CLOGD("[%s:%d] test FAILED", __func__, __LINE__);

    /* release mem */
    if (mcu_data_ori)
    {
        free(mcu_data_ori);
        mcu_data = NULL;
        mcu_data_ori = NULL;
    }

    return ret;
}

#if 0
uint32_t jpeg_encoder_ext(void *in_buf, void *out_buf, uint32_t *out_size, Jpeg_CodecCfg *enc_cfg)
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t timeout = 0;
    uint8_t *ecs_output_buffer = NULL;
    uint8_t *ecs_output_buffer_ori = NULL;
    uint32_t ecs_len = 0;
    Jpeg_InitTypeDef jpeg_enc_cfg = {
            .mode = JPEG_MODE_ENCODE,
            .format_in = JPEG_DECODE_IN_FORMAT_YUV420,
            .format_out = JPEG_DECODE_IN_FORMAT_YUV420,
            .ecs_size = 0,
            .rst_enable = 0,
            .rst_num = 0,
            .sampling_h = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .sampling_v = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .qt_index = {0x00,0x01,0x01,0x00},
            .ht_index = {0x00,0x11,0x11,0x00}
    };

    /* switch image to mcu block */
    jpeg_img_format_codec2jpeg_switch(enc_cfg, &jpeg_enc_cfg);
    jpeg_enc_cfg.img_width = enc_cfg->width;
    jpeg_enc_cfg.img_height = enc_cfg->height;
    jpeg_enc_cfg.img_width_align = CSK_JPEG_DATA_ALIGN(jpeg_enc_cfg.img_width, jpeg_enc_cfg.sampling_h);
    jpeg_enc_cfg.img_height_align = CSK_JPEG_DATA_ALIGN(jpeg_enc_cfg.img_height, jpeg_enc_cfg.sampling_v);
    jpeg_enc_cfg.pixel_size = jpeg_image_size(jpeg_enc_cfg.img_width_align, jpeg_enc_cfg.img_height_align, jpeg_enc_cfg.format_in);
    jpeg_enc_cfg.src_size = jpeg_enc_cfg.pixel_size;
    jpeg_init(&jpeg_enc_cfg);
    jpeg_send_huffman_table(jpeg_enc_htable_golden, sizeof(jpeg_enc_htable_golden)/sizeof(uint32_t));
    jpeg_refresh_quantization_table(jpeg_enc_qtable_golden, enc_cfg->q_factor);
    jpeg_send_quantization_table(jpeg_enc_qtable_golden, sizeof(jpeg_enc_qtable_golden)/sizeof(uint32_t));
    jpeg_start();

    ecs_len = jpeg_enc_cfg.pixel_size/10;
    if (ecs_len > JPEG_ECS_BUF_SIZE) {
        goto error1;
    }
    ecs_output_buffer = jpeg_ecs_static_buf;
    ecs_output_buffer_ori = NULL;
    jpeg_encode_dma2d_init(dma_2d_ch2, dma_2d_ch1, enc_cfg);
    jpeg_encode_dma2d_start(dma_2d_ch2, in_buf, jpeg_enc_cfg.pixel_size, dma_2d_ch1, (void*)ecs_output_buffer, ecs_len);

    /* wait done */
#ifndef CFG_RTOS
    timeout = 1000000; // us
    CHECK_EQ_TIMEOUT_EXIT(jpeg_dma2d_output_finish_cnt_get(), 0, timeout, error0);
#else
    if (NULL != jpeg_dma2d_output_finish_Semaphore)
        xSemaphoreTake(jpeg_dma2d_output_finish_Semaphore, portMAX_DELAY);
#endif
    ecs_len = IP_JPEG_TOP->REG_RESULT_DATA_LENGTH.bit.RESULT_LEN;

/* FlushInvalidate: flush header tail data, then invalidate for DMA result */
    HAL_InvalidateDCache_by_Addr((uint32_t*)ecs_output_buffer, ecs_len);
    jpeg_encoder_jpeg_build(ecs_output_buffer, ecs_len, out_buf, out_size, &jpeg_enc_cfg);

error0:
    //dma2d_reg_dump(dma_2d_ch2);
    //dma2d_reg_dump(dma_2d_ch1);
    jpeg_encode_dma2d_stop(dma_2d_ch2, dma_2d_ch1);
error1:
    //jpeg_reg_dump();
    jpeg_stop();
    jpeg_deinit();

    CLOGD("jpeg_enc_input_buffer:  0x%08x", in_buf);
    CLOGD("jpeg_enc_ecsout_buffer: 0x%08x,0x%08x,len=%#x", ecs_output_buffer, ecs_output_buffer_ori, ecs_len);
    CLOGD("jpeg_enc_out_buffer:    0x%08x,len=%#x", out_buf, *out_size);

    if(ret == 0)
        CLOGD("[%s:%d] test SUCCESS", __func__, __LINE__);
    else
        CLOGD("[%s:%d] test FAILED", __func__, __LINE__);

    /* no dynamic alloc to free - using static PSRAM buffer */

    return ret;
}
#else
uint32_t jpeg_encoder_ext(void *in_buf, void *out_buf, uint32_t *out_size, Jpeg_CodecCfg *enc_cfg)
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t timeout = 0;
    uint32_t ecs_len = 0;
    uint32_t jpeg_header_len = 0;
    Jpeg_InitTypeDef jpeg_enc_cfg = {
            .mode = JPEG_MODE_ENCODE,
            .format_in = JPEG_DECODE_IN_FORMAT_YUV420,
            .format_out = JPEG_DECODE_IN_FORMAT_YUV420,
            .ecs_size = 0,
            .rst_enable = 0,
            .rst_num = 0,
            .sampling_h = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .sampling_v = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .qt_index = {0x00,0x01,0x01,0x00},
            .ht_index = {0x00,0x11,0x11,0x00}
    };

    /* switch image to mcu block */
    jpeg_img_format_codec2jpeg_switch(enc_cfg, &jpeg_enc_cfg);
    jpeg_enc_cfg.img_width = enc_cfg->width;
    jpeg_enc_cfg.img_height = enc_cfg->height;
    jpeg_enc_cfg.img_width_align = CSK_JPEG_DATA_ALIGN(jpeg_enc_cfg.img_width, jpeg_enc_cfg.sampling_h);
    jpeg_enc_cfg.img_height_align = CSK_JPEG_DATA_ALIGN(jpeg_enc_cfg.img_height, jpeg_enc_cfg.sampling_v);
    jpeg_enc_cfg.pixel_size = jpeg_image_size(jpeg_enc_cfg.img_width_align, jpeg_enc_cfg.img_height_align, jpeg_enc_cfg.format_in);
    jpeg_enc_cfg.src_size = jpeg_enc_cfg.pixel_size;
    jpeg_init(&jpeg_enc_cfg);
    jpeg_send_huffman_table(jpeg_enc_htable_golden, sizeof(jpeg_enc_htable_golden)/sizeof(uint32_t));
    jpeg_refresh_quantization_table(jpeg_enc_qtable_golden, enc_cfg->q_factor);
    jpeg_send_quantization_table(jpeg_enc_qtable_golden, sizeof(jpeg_enc_qtable_golden)/sizeof(uint32_t));
    jpeg_start();

    jpeg_header_len = jpeg_encoder_jpeg_build_ext(out_buf, &jpeg_enc_cfg);
    jpeg_encode_dma2d_init(dma_2d_ch2, dma_2d_ch1, enc_cfg);
    jpeg_encode_dma2d_start(dma_2d_ch2, in_buf, jpeg_enc_cfg.pixel_size, dma_2d_ch1, (void*)((uint8_t*)out_buf+jpeg_header_len), jpeg_enc_cfg.pixel_size/10);

    /* wait done */
#ifndef CFG_RTOS
    timeout = 1000000; // us
    CHECK_EQ_TIMEOUT_EXIT(jpeg_dma2d_output_finish_cnt_get(), 0, timeout, error0);
#else
    if (NULL != jpeg_dma2d_output_finish_Semaphore)
        xSemaphoreTake(jpeg_dma2d_output_finish_Semaphore, portMAX_DELAY);
#endif
    ecs_len = IP_JPEG_TOP->REG_RESULT_DATA_LENGTH.bit.RESULT_LEN;

    /* FlushInvalidate: flush header tail data, then invalidate for DMA result */
    HAL_InvalidateDCache_by_Addr((uint32_t*)((uint8_t*)out_buf+jpeg_header_len), ecs_len);
    *((uint8_t*)out_buf+(jpeg_header_len+ecs_len)) = 0xff;
    *((uint8_t*)out_buf+(jpeg_header_len+ecs_len+1)) = 0xd9;
    *out_size = jpeg_header_len+ecs_len+2;

error0:
    //dma2d_reg_dump(dma_2d_ch2);
    //dma2d_reg_dump(dma_2d_ch1);
    jpeg_encode_dma2d_stop(dma_2d_ch2, dma_2d_ch1);
error1:
    //jpeg_reg_dump();
    jpeg_stop();
    jpeg_deinit();

    CLOGD("jpeg_enc_input_buffer:  0x%08x", in_buf);
    CLOGD("jpeg_enc_ecsout_buffer: 0x%08x,len=%#x", out_buf+jpeg_header_len, ecs_len);
    CLOGD("jpeg_enc_out_buffer:    0x%08x,len=%#x", out_buf, *out_size);

    if(ret == 0)
        CLOGD("[%s:%d] test SUCCESS", __func__, __LINE__);
    else
        CLOGD("[%s:%d] test FAILED", __func__, __LINE__);

    return ret;
}
#endif

uint32_t jpeg_decoder_pixel_build_gray(const uint8_t *in_buf, uint8_t *out_buf, Jpeg_CodecCfg *dec_cfg, Jpeg_InitTypeDef *jpeg_dec_cfg)
{
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t i = 0;
    uint16_t j = 0;
    uint8_t *block_buf_ptr = NULL;
    uint8_t *block_buf[8] = {NULL};
    uint32_t linesize = 0;
    uint32_t linesize_align = 0;
    uint32_t pix_index = 0;
    uint32_t line_index = 0;
    int32_t ret = CSK_DRIVER_OK;
    uint8_t pixel_bw = 1;

    /* Allocate memory for pixel block lines buffer */
    linesize = pixel_bw*jpeg_dec_cfg->img_width;
    linesize_align = pixel_bw*jpeg_dec_cfg->img_width_align;
    if (NULL == (block_buf_ptr = (uint8_t *)malloc(jpeg_dec_cfg->sampling_v*linesize_align)))
    {
        CLOG("[%s:%d]malloc failed:size=%u", __func__, __LINE__, jpeg_dec_cfg->sampling_v*linesize_align);
        return CSK_DRIVER_ERROR;
    }
    for (i = 0; i < jpeg_dec_cfg->sampling_v; i++)
    {
        block_buf[i] = block_buf_ptr + i * linesize_align;
    }

    for(y = 0; y < jpeg_dec_cfg->img_height_align; y += jpeg_dec_cfg->sampling_v)
    {
        for(x = 0; x < jpeg_dec_cfg->img_width_align; x += jpeg_dec_cfg->sampling_h)
        {
            /* Input Y blocks */
            line_index = pixel_bw*x;
            for(i = 0; i < jpeg_dec_cfg->sampling_v; i++)
            {
                for(j = 0; j < jpeg_dec_cfg->sampling_h; j++)
                {
                    block_buf[i][line_index+j*pixel_bw] = in_buf[pix_index++];
                }
            }
        }

        /* Output the line buffer */
        uint16_t v_tmp = jpeg_dec_cfg->sampling_v;
        if ((y + jpeg_dec_cfg->sampling_v) > jpeg_dec_cfg->img_height)
        {
            v_tmp -= (jpeg_dec_cfg->img_height_align - jpeg_dec_cfg->img_height);
        }
        for (i = 0; i < v_tmp; i++)
        {
            memcpy(out_buf, block_buf[i], linesize);
            out_buf += linesize;
        }
    }

    dec_cfg->pixel_format = JPEG_PIXEL_FORMAT_GRAY;

    free(block_buf_ptr);

    return ret;
}


uint32_t jpeg_decoder_pixel_build_yuv(const uint8_t *in_buf, uint8_t *out_buf, Jpeg_CodecCfg *dec_cfg, Jpeg_InitTypeDef *jpeg_dec_cfg)
{
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t i = 0;
    uint16_t j = 0;
    int32_t r = 0;
    int32_t g = 0;
    int32_t b = 0;
    int32_t yb[16][16],cbb[16][16],crb[16][16];
    uint8_t *block_buf_ptr = NULL;
    uint8_t *block_buf[16] = {NULL};
    uint32_t linesize = 0;
    uint32_t linesize_align = 0;
    uint32_t pix_index = 0;
    uint32_t line_index = 0;
    int32_t ret = CSK_DRIVER_OK;
    uint8_t pixel_bw = 3;

    /* Allocate memory for pixel block lines buffer */
    linesize = pixel_bw*jpeg_dec_cfg->img_width;
    linesize_align = pixel_bw*jpeg_dec_cfg->img_width_align;
    if (NULL == (block_buf_ptr = (uint8_t *)malloc(jpeg_dec_cfg->sampling_v*linesize_align)))
    {
        CLOGD("[%s:%d]malloc failed:size=%u", __func__, __LINE__, jpeg_dec_cfg->sampling_v*linesize_align);
        return CSK_DRIVER_ERROR;
    }
    for (i = 0; i < jpeg_dec_cfg->sampling_v; i++)
    {
        block_buf[i] = block_buf_ptr + i * linesize_align;
    }

    for(y = 0; y < jpeg_dec_cfg->img_height_align; y += jpeg_dec_cfg->sampling_v)
    {
        for(x = 0; x < jpeg_dec_cfg->img_width_align; x += jpeg_dec_cfg->sampling_h)
        {
            if (CSK_JPEG_BLOCK_BASIC_PIXEL_NUM == jpeg_dec_cfg->sampling_h && CSK_JPEG_BLOCK_BASIC_PIXEL_NUM == jpeg_dec_cfg->sampling_v) /* 0x11 */
            {
                /* Input Y blocks */
                for(i = 0; i < 8; i++)
                {
                    for(j = 0; j < 8; j++)
                    {
                        yb[i][j] = in_buf[pix_index++];
                    }
                }

                /* Input Cb block */
                for(i = 0; i < 8; i++)
                {
                    for(j = 0; j < 8; j++)
                    {
                        cbb[i][j] = in_buf[pix_index++];
                    }
                }

                /* Input Cr block */
                for(i = 0; i < 8; i++)
                {
                    for(j = 0; j < 8; j++)
                    {
                        crb[i][j] = in_buf[pix_index++];
                    }
                }
            }
            else if ((2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM) == jpeg_dec_cfg->sampling_h && (2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM) == jpeg_dec_cfg->sampling_v) /* 0x22 */
            {
                /* Input 4 Y blocks */
                for(i = 0; i < 8; i++)
                {
                    for(j = 0; j < 8; j++)
                    {
                        yb[i][j] = in_buf[pix_index++];
                    }
                }
                for(i = 0; i < 8; i++)
                {
                    for(j = 8; j < 16; j++)
                    {
                        yb[i][j] = in_buf[pix_index++];
                    }
                }
                for(i = 8; i < 16; i++)
                {
                    for(j = 0; j < 8; j++)
                    {
                        yb[i][j] = in_buf[pix_index++];
                    }
                }
                for(i = 8; i < 16; i++)
                {
                    for(j = 8; j < 16; j++)
                    {
                        yb[i][j] = in_buf[pix_index++];
                    }
                }

                /* Input Cb block */
                for(i = 0; i < 16; i += 2)
                {
                    for(j = 0; j < 16; j += 2)
                    {
                        cbb[i][j] = in_buf[pix_index++];
                    }
                }
                /* Up interpolation */
                for(i = 0; i < 16; i += 2)
                {
                    for(j = 1; j < 15; j += 2)
                    {
                        cbb[i][j] = (cbb[i][j-1]+cbb[i][j+1])/2;
                    }
                    cbb[i][15] = cbb[i][14];
                }
                for(i = 1; i < 15; i += 2)
                {
                    for(j = 0; j < 16; j++)
                    {
                        cbb[i][j] = (cbb[i-1][j]+cbb[i+1][j])/2;
                    }
                }
                for(j = 0; j < 16; j++)
                {
                    cbb[15][j] = cbb[14][j];
                }

                /* Input Cr block */
                for(i = 0; i < 16; i += 2)
                {
                    for(j = 0; j < 16; j += 2)
                    {
                        crb[i][j] = in_buf[pix_index++];
                    }
                }
                /* Up interpolation */
                for(i = 0; i < 16; i += 2)
                {
                    for(j = 1; j < 15; j += 2)
                    {
                        crb[i][j] = (crb[i][j-1]+crb[i][j+1])/2;
                    }
                    crb[i][15] = crb[i][14];
                }
                for(i = 1; i < 15; i += 2)
                {
                    for(j = 0; j < 16; j++)
                    {
                        crb[i][j] = (crb[i-1][j]+crb[i+1][j])/2;
                    }
                }
                for(j = 0; j < 16; j++)
                {
                    crb[15][j] = crb[14][j];
                }
            }
            else /* 0x21 */
            {
                /* Input 4 Y blocks */
                for(i = 0; i < 8; i++)
                {
                    for(j = 0; j < 8; j++)
                    {
                        yb[i][j] = in_buf[pix_index++];
                    }
                }
                for(i = 0; i < 8; i++)
                {
                    for(j = 8; j < 16; j++)
                    {
                        yb[i][j] = in_buf[pix_index++];
                    }
                }

                /* Input Cb block */
                for(i = 0; i < 8; i++)
                {
                    for(j = 0; j < 16; j += 2)
                    {
                        cbb[i][j] = in_buf[pix_index++];
                    }
                }
                /* Up interpolation */
                for(i = 0; i < 8; i++)
                {
                    for(j = 1; j < 15; j += 2)
                    {
                        cbb[i][j] = (cbb[i][j-1]+cbb[i][j+1])/2;
                    }
                    cbb[i][15] = cbb[i][14];
                }

                /* Input Cr block */
                for(i = 0; i < 8; i++)
                {
                    for(j = 0; j < 16; j += 2)
                    {
                        crb[i][j] = in_buf[pix_index++];
                    }
                }
                /* Up interpolation */
                for(i = 0; i < 8; i++)
                {
                    for(j = 1; j < 15; j += 2)
                    {
                        crb[i][j] = (crb[i][j-1]+crb[i][j+1])/2;
                    }
                    crb[i][15] = crb[i][14];
                }
            }

            /* Grab a pixel block for each component */
            line_index = pixel_bw*x;
            for(i = 0; i < jpeg_dec_cfg->sampling_v; i++)
            {
                for(j = 0; j < jpeg_dec_cfg->sampling_h; j++)
                {
                    r = ((yb[i][j]<<16) + 91881*(crb[i][j]-128))>>16;
                    if(r < 0)
                        r = 0;
                    else if(r > 255)
                        r = 255;
                    g = ((yb[i][j]<<16) - 22554*(cbb[i][j]-128) - 46802*(crb[i][j]-128))>>16;
                    if(g < 0)
                        g = 0;
                    else if(g > 255)
                        g = 255;
                    b = ((yb[i][j]<<16) +116130*(cbb[i][j]-128))>>16;
                    if(b < 0)
                        b = 0;
                    else if(b > 255)
                        b = 255;
                    block_buf[i][line_index+j*pixel_bw] = (uint8_t)r;
                    block_buf[i][line_index+j*pixel_bw+1] = (uint8_t)g;
                    block_buf[i][line_index+j*pixel_bw+2] = (uint8_t)b;
                }
            }
        }

        /* Output the line buffer */
        uint16_t v_tmp = jpeg_dec_cfg->sampling_v;
        if ((y + jpeg_dec_cfg->sampling_v) > jpeg_dec_cfg->img_height)
        {
            v_tmp -= (jpeg_dec_cfg->img_height_align - jpeg_dec_cfg->img_height);
        }
        for (i = 0; i < v_tmp; i++)
        {
            memcpy(out_buf, block_buf[i], linesize);
            out_buf += linesize;
        }
    }

    dec_cfg->pixel_format = JPEG_PIXEL_FORMAT_RGB888;

    free(block_buf_ptr);

    return ret;
}

uint32_t jpeg_decoder_pixel_build(const uint8_t *in_buf, uint8_t *out_buf, Jpeg_CodecCfg *dec_cfg, Jpeg_InitTypeDef *jpeg_dec_cfg)
{
    if (JPEG_DECODE_IN_FORMAT_GRAY == jpeg_dec_cfg->format_in)
        return jpeg_decoder_pixel_build_gray(in_buf, out_buf, dec_cfg, jpeg_dec_cfg);
    else
        return jpeg_decoder_pixel_build_yuv(in_buf, out_buf, dec_cfg, jpeg_dec_cfg);
}

uint32_t jpeg_decoder_parse_jpg(const uint8_t *in_buf, uint32_t in_size, uint8_t **ecs_buf, uint8_t **ecs_buf_ori, Jpeg_InitTypeDef *jpeg_dec_cfg, uint32_t *qtable, uint32_t *huffmin, uint32_t *huffbase, uint32_t *huffsymbol)
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t index = 0;
    uint8_t *jpeg_data = (uint8_t*)in_buf;
    uint8_t *ecs_data = NULL;
    uint8_t *ecs_data_ori = NULL;
    uint8_t color_num;                    /* color component num */
    uint8_t sampling_factor[4];           /* sampling factor */
    uint32_t huffmin_tmp[16] = {0};
    uint32_t l[16] = {0};
    uint32_t abase = 0;
    uint32_t bbase = 0;
    uint32_t dc = 0;
    uint32_t ht = 0;
    uint32_t hbi = 0;
    uint32_t hmi = 0;
    uint32_t v = 0;
    uint32_t code = 0;
    uint16_t lh = 0;
    uint32_t start_index = 0;

    while(index < in_size)
    {
        if (0xff != jpeg_data[index])
        {
            index++;
            continue;
        }

        /* Found a potential marker */
        while(0xff == jpeg_data[++index]);

        if (0x0 == jpeg_data[index])
        {
            index++;
            continue;
        }

        /* Which marker */
        marker:
        switch(jpeg_data[index])
        {
            case 0xc0 : /* SOF0 */
            {
                index += 4; /* lf-16bit  p-8bit  y-16bit  x-16bit nf-8bit */
                jpeg_dec_cfg->img_height = (jpeg_data[index]<<8)|jpeg_data[index+1];
                index += 2;
                jpeg_dec_cfg->img_width = (jpeg_data[index]<<8)|jpeg_data[index+1];
                index += 2;
                color_num = jpeg_data[index];
                for(i = 0; i < color_num; i++)
                {
                    sampling_factor[i] = jpeg_data[index+2];
                    jpeg_dec_cfg->qt_index[i] = jpeg_data[index+3];
                    index += 3;
                }
                jpeg_dec_cfg->sampling_v = (sampling_factor[0]&0xf)*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
                jpeg_dec_cfg->sampling_h = ((sampling_factor[0]>>4)&0xf)*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
                if (!(CSK_JPEG_BLOCK_BASIC_PIXEL_NUM == jpeg_dec_cfg->sampling_h || (2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM) == jpeg_dec_cfg->sampling_h) || !(CSK_JPEG_BLOCK_BASIC_PIXEL_NUM == jpeg_dec_cfg->sampling_v || (2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM) == jpeg_dec_cfg->sampling_v))
                {
                    CLOG("[%s:%d]sampling factor value error(%u-%u)", __func__, __LINE__, jpeg_dec_cfg->sampling_h, jpeg_dec_cfg->sampling_v);
                    return CSK_DRIVER_ERROR;
                }

                if (1 == color_num)
                {
                    jpeg_dec_cfg->format_in = JPEG_DECODE_IN_FORMAT_GRAY;
                }
                else
                {
                    if (CSK_JPEG_BLOCK_BASIC_PIXEL_NUM == jpeg_dec_cfg->sampling_h && CSK_JPEG_BLOCK_BASIC_PIXEL_NUM == jpeg_dec_cfg->sampling_v) /* 0x11 */
                    {
                        jpeg_dec_cfg->format_in = JPEG_DECODE_IN_FORMAT_YUV444;
                    }
                    else if ((2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM) == jpeg_dec_cfg->sampling_h && (2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM) == jpeg_dec_cfg->sampling_v) /* 0x22 */
                    {
                        jpeg_dec_cfg->format_in = JPEG_DECODE_IN_FORMAT_YUV420;
                    }
                    else /* 0x21 */
                    {
                        jpeg_dec_cfg->format_in = JPEG_DECODE_IN_FORMAT_YUV422;
                    }
                }
                jpeg_dec_cfg->img_width_align = CSK_JPEG_DATA_ALIGN(jpeg_dec_cfg->img_width, jpeg_dec_cfg->sampling_h);
                jpeg_dec_cfg->img_height_align = CSK_JPEG_DATA_ALIGN(jpeg_dec_cfg->img_height, jpeg_dec_cfg->sampling_v);
            }break;

            case 0xda : /* SOS */
            {
                index += 3; /* Ls-16bit  Ns-8bit */
                color_num = jpeg_data[index];
                for(i = 0; i < color_num; i++) /* Cs-8bit  Td-4bit  Ta-4bit */
                {
                    jpeg_dec_cfg->ht_index[i] = jpeg_data[index+2];
                    index += 2;
                }
                index += 4; /* Ss-8bit  Se-8bit  Ah-4bit Ai-4bit  ecs[0] */
                jpeg_dec_cfg->ecs_size = in_size - index;
                if (NULL == (ecs_data_ori = (uint8_t*)malloc(jpeg_dec_cfg->ecs_size + CSK_JPEG_DMA2D_ADDR_ALIGNMENT - 1)))
                {
                    CLOGD("[%s:%d]malloc failed:size=%u", __func__, __LINE__, jpeg_dec_cfg->ecs_size);
                    return CSK_DRIVER_ERROR;
                }
                ecs_data = (uint8_t*)(((size_t)ecs_data_ori + CSK_JPEG_DMA2D_ADDR_ALIGNMENT - 1) & ~(CSK_JPEG_DMA2D_ADDR_ALIGNMENT - 1));
                memcpy(ecs_data, in_buf + index, jpeg_dec_cfg->ecs_size);
                HAL_FlushDCache_by_Addr((uint32_t*)ecs_data, jpeg_dec_cfg->ecs_size);
                index = in_size;
            }break;

            case 0xdd : /* DRI */
            {
                if (0 != (jpeg_dec_cfg->rst_num = (jpeg_data[index+3]<<8)|jpeg_data[index+4]))
                    jpeg_dec_cfg->rst_enable = 1;
                index += ((jpeg_data[index+1]<<8|jpeg_data[index+2])+1); /* segment length + segment data */
            }break;

            case 0xdb : /* DQT */
            {
                index += 3; /* Lq-16bit  Pq-4bit  Tq-4bit */
                uint8_t pq = (jpeg_data[index]>>4)&0xF;
                uint8_t tq_offset = ((0 == (jpeg_data[index]&0xF)) ? 0 : 1) * 64;   /* hardware support max 2 qtable */
                uint8_t high_byte = 0;
                for(i = 0; i < 64; i++)
                {
                    if (0 == pq) {
                        *(qtable + tq_offset + i) = jpeg_data[++index];
                    } else {
                        high_byte = jpeg_data[++index];
                        *(qtable + tq_offset + i) = (high_byte << 8) | jpeg_data[++index];
                    }
                }
            }break;

            case 0xc4 : /* DHT */
            {
                uint8_t high_byte = jpeg_data[++index];
                lh = ((high_byte << 8) | jpeg_data[++index]) - 2;/* Lh-16bit  Tc-4bit  Th-4bit */
                start_index = index;
                do
                {
                    abase = (0 != (jpeg_data[++index]>>4)) ? 0 : 1;
                    dc = abase;
                    ht = jpeg_data[index]&0x0F;
                    abase |= ht<<1;
                    switch(abase)
                    {
                        case 1:
                        case 3:
                            bbase = 162;
                            break;
                        case 2:
                            bbase = 174;
                            break;
                        default:
                            bbase = 0;
                            break;
                    }
                    hbi = abase<<4;
                    /* Get the number of codes for each lenght */
                    for(i = 0; i < 16; i++)
                    {
                        l[i] = jpeg_data[++index];
                    }
                    code = 0;
                    for(i = 0; i < 16; i++)
                    {
                        huffmin_tmp[i] = code;
                        huffbase[hbi+i] = (bbase - code)&0x1FF;
                        if(l[i])
                        {
                            for(j = 0; j < l[i]; j++,bbase++)
                            {
                                v = jpeg_data[++index];
                                if(dc)
                                {
                                    if (ht) /* dc1 */
                                        huffsymbol[bbase] = (huffsymbol[bbase]&0xF)|((v&0xF)<<4);
                                    else /* dc0 */
                                        huffsymbol[bbase] = (huffsymbol[bbase]&0xF0)|(v&0xF);
                                }
                                else
                                {
                                    huffsymbol[bbase] = v&0xFF;
                                }
                                code++;
                            }
                        }
                        code <<= 1;
                    }
                    hmi = abase<<2;
                    huffmin[hmi+3] = ((((huffmin_tmp[0]&0x1)<<2)|(huffmin_tmp[1]&0x3))<<3)|(huffmin_tmp[2]&0x7);
                    huffmin[hmi+2] = (((((((((huffmin[hmi+3]<<4)|(huffmin_tmp[3]&0xf))<<5)|(huffmin_tmp[4]&0x1f))<<6)|(huffmin_tmp[5]&0x3f))<<7)|(huffmin_tmp[6]&0x7f))<<8)|(huffmin_tmp[7]&0xff);
                    huffmin[hmi+3] >>= 2;
                    huffmin[hmi+1] = ((((((huffmin_tmp[8]&0xff)<<8)|(huffmin_tmp[9]&0xff))<<8)|(huffmin_tmp[10]&0xff))<<8)|(huffmin_tmp[11]&0xff);
                    huffmin[hmi+0] = ((((((huffmin_tmp[12]&0xff)<<8)|(huffmin_tmp[13]&0xff))<<8)|(huffmin_tmp[14]&0xff))<<8)|(huffmin_tmp[15]&0xff);
                }while(lh > (index - start_index));
            }break;

            case 0xc8 : /* JPEG reserved */
            case 0xcc : /* DAC */
            case 0xd0 : /* RST */
            case 0xd1 :
            case 0xd2 :
            case 0xd3 :
            case 0xd4 :
            case 0xd5 :
            case 0xd6 :
            case 0xd7 :
            case 0xd8 : /* SOI */
            case 0xd9 : /* EOI */
                break;

            case 0xc1 : /* SOF* */
            case 0xc2 :
            case 0xc3 :
            case 0xc5 :
            case 0xc6 :
            case 0xc7 :
            case 0xc9 :
            case 0xca :
            case 0xcb :
            case 0xcd :
            case 0xce :
            case 0xcf :
            case 0xe0 : /* APP reserved */
            case 0xe1 :
            case 0xe2 :
            case 0xe3 :
            case 0xe4 :
            case 0xe5 :
            case 0xe6 :
            case 0xe7 :
            case 0xe8 :
            case 0xe9 :
            case 0xea :
            case 0xeb :
            case 0xec :
            case 0xed :
            case 0xee :
            case 0xef :
            case 0xf0 : /* JPEG reserved */
            case 0xf1 :
            case 0xf2 :
            case 0xf3 :
            case 0xf4 :
            case 0xf5 :
            case 0xf6 :
            case 0xf7 :
            case 0xf8 :
            case 0xf9 :
            case 0xfa :
            case 0xfb :
            case 0xfc :
            case 0xfd :
            case 0xfe :
            {
                index += ((jpeg_data[index+1]<<8|jpeg_data[index+2])+1); /* segment length + segment data */
            }break;

            default :
            {
                CLOGD("Unknown marker %x !", jpeg_data[index]);
            }break;
        }
    }

    *ecs_buf = ecs_data;
    *ecs_buf_ori = ecs_data_ori;

    CLOGD("[%s:%d]width=%u,height=%u,format=%u,rst_enable=%u,rst_num=%u,sampling=(%u-%u),align(%u-%u),qt(%#x-%#x-%#x-%#x),ht(%#x-%#x-%#x-%#x)", __func__, __LINE__, jpeg_dec_cfg->img_width, jpeg_dec_cfg->img_height, jpeg_dec_cfg->format_in, jpeg_dec_cfg->rst_enable,
         jpeg_dec_cfg->rst_num, jpeg_dec_cfg->sampling_h, jpeg_dec_cfg->sampling_v, jpeg_dec_cfg->img_width_align, jpeg_dec_cfg->img_height_align, jpeg_dec_cfg->qt_index[0], jpeg_dec_cfg->qt_index[1], jpeg_dec_cfg->qt_index[2], jpeg_dec_cfg->qt_index[3],
         jpeg_dec_cfg->ht_index[0], jpeg_dec_cfg->ht_index[1], jpeg_dec_cfg->ht_index[2], jpeg_dec_cfg->ht_index[3]);

    return ret;
}

uint32_t jpeg_decoder(const uint8_t *in_buf, uint32_t in_size, uint8_t *out_buf, Jpeg_CodecCfg *dec_cfg)
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t timeout = 0;
    uint8_t *ecs_data = NULL;
    uint8_t *ecs_data_ori = NULL;
    uint8_t *pix_output_buffer = NULL;
    uint8_t *pix_output_buffer_ori = NULL;
    Jpeg_InitTypeDef jpeg_dec_cfg;
    memset(&jpeg_dec_cfg, 0, sizeof(jpeg_dec_cfg));

    /* parsing jpeg data,get ecs/qt/ */
    if (CSK_DRIVER_OK != (ret = jpeg_decoder_parse_jpg(in_buf, in_size, &ecs_data, &ecs_data_ori, &jpeg_dec_cfg, jpeg_dec_qtable_golden, jpeg_dec_min_table_golden, jpeg_dec_base_table_golden, jpeg_dec_symbol_table_golden)))
    {
        CLOGD("[%s:%d]jpeg_decoder_parse_jpg failed,ret=%d", __func__, __LINE__, ret);
        return ret;
    }
    jpeg_dec_cfg.mode = JPEG_MODE_DECODE;
    jpeg_dec_cfg.pixel_size = jpeg_image_size(jpeg_dec_cfg.img_width_align, jpeg_dec_cfg.img_height_align, jpeg_dec_cfg.format_in);
    jpeg_dec_cfg.src_size = jpeg_dec_cfg.ecs_size;

    jpeg_init(&jpeg_dec_cfg);
    jpeg_send_base_table(jpeg_dec_base_table_golden, sizeof(jpeg_dec_base_table_golden)/sizeof(uint32_t));
    jpeg_send_min_table(jpeg_dec_min_table_golden, sizeof(jpeg_dec_min_table_golden)/sizeof(uint32_t));
    jpeg_send_symbol_table(jpeg_dec_symbol_table_golden, sizeof(jpeg_dec_symbol_table_golden)/sizeof(uint32_t));
    jpeg_send_quantization_table(jpeg_dec_qtable_golden, sizeof(jpeg_dec_qtable_golden)/sizeof(uint32_t) / 2);
    jpeg_start();

    if (NULL == (pix_output_buffer_ori = (uint8_t*)malloc(jpeg_dec_cfg.pixel_size + CSK_JPEG_DMA2D_ADDR_ALIGNMENT - 1)))
    {
        CLOGD("[%s:%d]malloc pix_output_buffer failed:size=%u", __func__, __LINE__, jpeg_dec_cfg.pixel_size);
        goto error1;
    }
    pix_output_buffer = (uint8_t*)(((size_t)pix_output_buffer_ori + CSK_JPEG_DMA2D_ADDR_ALIGNMENT - 1) & ~(CSK_JPEG_DMA2D_ADDR_ALIGNMENT - 1));
    jpeg_decode_dma2d_init(dma_2d_ch2, dma_2d_ch1, NULL);
    jpeg_decode_dma2d_start(dma_2d_ch2, (void*)ecs_data, CSK_JPEG_DATA_ALIGN(jpeg_dec_cfg.ecs_size, 4), dma_2d_ch1, (void*)pix_output_buffer, jpeg_dec_cfg.pixel_size);

    /* wait done */
#ifndef CFG_RTOS
    timeout = 1000000;    // us
    CHECK_EQ_TIMEOUT_EXIT(jpeg_dma2d_output_finish_cnt_get(), 0, timeout, error0);
#else
    if (NULL != jpeg_dma2d_output_finish_Semaphore)
        xSemaphoreTake(jpeg_dma2d_output_finish_Semaphore, portMAX_DELAY);
#endif

    /* FlushInvalidate: flush header tail data, then invalidate for DMA result */
    HAL_InvalidateDCache_by_Addr((uint32_t*)pix_output_buffer, jpeg_dec_cfg.pixel_size);

    dec_cfg->width = jpeg_dec_cfg.img_width;
    dec_cfg->height = jpeg_dec_cfg.img_height;
    jpeg_img_format_jpeg2codec_switch(&jpeg_dec_cfg, dec_cfg);
    jpeg_decoder_pixel_build(pix_output_buffer, out_buf, dec_cfg, &jpeg_dec_cfg);

error0:
    dma2d_reg_dump(dma_2d_ch2);
    dma2d_reg_dump(dma_2d_ch1);
    jpeg_decode_dma2d_stop(dma_2d_ch2, dma_2d_ch1);
error1:
    jpeg_reg_dump();
    jpeg_stop();
    jpeg_deinit();

    CLOGD("jpeg_dec_jpeg_buffer:         0x%08x", in_buf);
    CLOGD("jpeg_dec_ecs_buffer:          0x%08x,0x%08x,size=%u", ecs_data, ecs_data_ori, jpeg_dec_cfg.ecs_size);
    CLOGD("jpeg_dec_pixout_buffer:       0x%08x,0x%08x", pix_output_buffer, pix_output_buffer_ori);
    CLOGD("jpeg_dec_out_buffer:          0x%08x", out_buf);
    CLOGD("qtable=0x%08x,base=0x%08x,min=0x%08x,symbol=0x%08x", jpeg_dec_qtable_golden, jpeg_dec_base_table_golden, jpeg_dec_min_table_golden, jpeg_dec_symbol_table_golden);

    if(ret == 0)
        CLOGD("[%s:%d] test SUCCESS", __func__, __LINE__);
    else
        CLOGD("[%s:%d] test FAILED", __func__, __LINE__);

    /* release mem */
    if (ecs_data_ori)
    {
        free(ecs_data_ori);
        ecs_data = NULL;
        ecs_data_ori = NULL;
    }
    if (pix_output_buffer_ori)
    {
        free(pix_output_buffer_ori);
        pix_output_buffer = NULL;
        pix_output_buffer_ori = NULL;
    }

    return ret;
}

uint32_t jpeg_decoder_ext(const uint8_t *in_buf, uint32_t in_size, uint8_t *out_buf, Jpeg_CodecCfg *dec_cfg)
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t timeout = 0;
    uint8_t *ecs_data = NULL;
    uint8_t *ecs_data_ori = NULL;
    Jpeg_InitTypeDef jpeg_dec_cfg;
    memset(&jpeg_dec_cfg, 0, sizeof(jpeg_dec_cfg));

    /* parsing jpeg data,get ecs/qt/ */
    if (CSK_DRIVER_OK != (ret = jpeg_decoder_parse_jpg(in_buf, in_size, &ecs_data, &ecs_data_ori, &jpeg_dec_cfg, jpeg_dec_qtable_golden, jpeg_dec_min_table_golden, jpeg_dec_base_table_golden, jpeg_dec_symbol_table_golden)))
    {
        CLOGD("[%s:%d]jpeg_decoder_parse_jpg failed,ret=%d", __func__, __LINE__, ret);
        return ret;
    }
    jpeg_dec_cfg.mode = JPEG_MODE_DECODE;
    jpeg_dec_cfg.pixel_size = jpeg_image_size(jpeg_dec_cfg.img_width_align, jpeg_dec_cfg.img_height_align, jpeg_dec_cfg.format_in);
    jpeg_dec_cfg.src_size = jpeg_dec_cfg.ecs_size;

    jpeg_init(&jpeg_dec_cfg);
    jpeg_send_base_table(jpeg_dec_base_table_golden, sizeof(jpeg_dec_base_table_golden)/sizeof(uint32_t));
    jpeg_send_min_table(jpeg_dec_min_table_golden, sizeof(jpeg_dec_min_table_golden)/sizeof(uint32_t));
    jpeg_send_symbol_table(jpeg_dec_symbol_table_golden, sizeof(jpeg_dec_symbol_table_golden)/sizeof(uint32_t));
    jpeg_send_quantization_table(jpeg_dec_qtable_golden, sizeof(jpeg_dec_qtable_golden)/sizeof(uint32_t) / 2);
    jpeg_start();

    dec_cfg->width = jpeg_dec_cfg.img_width;
    dec_cfg->height = jpeg_dec_cfg.img_height;
    jpeg_img_format_jpeg2codec_switch(&jpeg_dec_cfg, dec_cfg);

    jpeg_decode_dma2d_init(dma_2d_ch1, dma_2d_ch2, dec_cfg);
    jpeg_decode_dma2d_start(dma_2d_ch1, (void*)ecs_data, CSK_JPEG_DATA_ALIGN(jpeg_dec_cfg.ecs_size, 4), dma_2d_ch2, (void*)out_buf, jpeg_dec_cfg.pixel_size);

    /* wait done */
#ifndef CFG_RTOS
    timeout = 1000000;    // us
    CHECK_EQ_TIMEOUT_EXIT(jpeg_dma2d_output_finish_cnt_get(), 0, timeout, error0);
#else
    if (NULL != jpeg_dma2d_output_finish_Semaphore)
        xSemaphoreTake(jpeg_dma2d_output_finish_Semaphore, portMAX_DELAY);
#endif

    /* FlushInvalidate: flush header tail data, then invalidate for DMA result */
    HAL_InvalidateDCache_by_Addr((uint32_t*)out_buf, jpeg_dec_cfg.pixel_size);

error0:
    dma2d_reg_dump(dma_2d_ch1);
    dma2d_reg_dump(dma_2d_ch2);
    jpeg_decode_dma2d_stop(dma_2d_ch1, dma_2d_ch2);
error1:
    jpeg_reg_dump();
    jpeg_stop();
    jpeg_deinit();

    CLOGD("jpeg_dec_jpeg_buffer:         0x%08x", in_buf);
    CLOGD("jpeg_dec_ecs_buffer:          0x%08x,0x%08x,size=%u", ecs_data, ecs_data_ori, jpeg_dec_cfg.ecs_size);
    CLOGD("jpeg_dec_out_buffer:          0x%08x", out_buf);
    CLOGD("qtable=0x%08x,base=0x%08x,min=0x%08x,symbol=0x%08x", jpeg_dec_qtable_golden, jpeg_dec_base_table_golden, jpeg_dec_min_table_golden, jpeg_dec_symbol_table_golden);

    if(ret == 0)
        CLOGD("[%s:%d] test SUCCESS", __func__, __LINE__);
    else
        CLOGD("[%s:%d] test FAILED", __func__, __LINE__);

    /* release mem */
    if (ecs_data_ori)
    {
        free(ecs_data_ori);
        ecs_data = NULL;
        ecs_data_ori = NULL;
    }

    return ret;
}

