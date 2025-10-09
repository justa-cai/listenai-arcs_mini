#include <string.h>
#include <stdint.h>
#include "arcs_ap.h"
#include "jpeg.h"
#include "log_print.h"


#define DEBUG_LOG   1
#if DEBUG_LOG
#define DEV_LOG(format, ...)   CLOGD(format, ##__VA_ARGS__)
#else
#define DEV_LOG(format, ...)
#endif // DEBUG_LOG


//#define SYSCTRL_CFG         ((SYSCFG_RegDef*) CMN_SYSCTRL_BASE)

// driver version
#define CSK_JPEG_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)

static const
CSK_DRIVER_VERSION jpeg_driver_version = { CSK_JPEG_API_VERSION, CSK_JPEG_DRV_VERSION };

//------------------------------------------------------------------------------------------


_FAST_DATA_VI static Jpeg_DEV jpeg0_dev = {
        (IP_JPEG),
        {0},
};


//------------------------------------------------------------------------------------

static void ap_cfg_video_clk_enable(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIDEO_CLK = 0x1;  // bit15
    IP_AP_CFG->REG_CLK_CFG0.all |= (1<<15);
}

static void ap_cfg_jpeg_clk_enable(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.ENA_JPEG_CLK = 0x1;  // bit31
    IP_AP_CFG->REG_CLK_CFG1.all |= (1<<31);
}

static void ap_cfg_jpeg_reset(void)
{
    //IP_AP_CFG->REG_SW_RESET.bit.JPEG_RESET = 0x1;       // bit14
    IP_AP_CFG->REG_SW_RESET.all |= (1<<14);
}

static void Jpeg_Reset(void)
{
    ap_cfg_video_clk_enable();
    ap_cfg_jpeg_clk_enable();
    ap_cfg_jpeg_reset();
    return;
}


static Jpeg_DEV * safe_jpeg_dev(void *jpeg_dev)
{
    //TODO: safe check of Jpeg device parameter
    if (jpeg_dev == Jpeg0()) {
        if (((Jpeg_DEV *)jpeg_dev)->Instance != (IP_JPEG)) {
            DEV_LOG("[%s:%d] Error jpeg_dev=%#x ", __func__, __LINE__, jpeg_dev);
            return NULL;
        }
    } else {
        DEV_LOG("[%s:%d] Error jpeg_dev=%#x ", __func__, __LINE__, jpeg_dev);
        return NULL;
    }

    return (Jpeg_DEV *)jpeg_dev;
}


/**
  * @brief  Handles JPEG interrupt request.
  * @retval None
  */
_FAST_FUNC_RO void Jpeg_IRQ_Handler()
{
    Jpeg_DEV *jpeg_dev = (Jpeg_DEV *)Jpeg0();
    uint32_t status = jpeg_dev->Instance->REG_INT_ST_CLR.all;
    //DEV_LOG("[%s:%d] status=0x%x", __func__, __LINE__, status);

    /* irq clear */
    jpeg_dev->Instance->REG_INT_ST_CLR.all = status;

    /* Coding/decoding done interrupt status bit" */
    if((status & JPEG_INT_ST_CLR_CODEC_DONE_Msk) == JPEG_INT_ST_CLR_CODEC_DONE_Msk)
    {
        if(jpeg_dev->cb_event)
            jpeg_dev->cb_event(JPEG_IRQ_EVENT_CODEC_DONE, 0);
    }

    /* DMA P channel transfer done */
    if((status & JPEG_INT_ST_CLR_PDMA_DONE_Msk) == JPEG_INT_ST_CLR_PDMA_DONE_Msk)
    {
        if(jpeg_dev->cb_event)
            jpeg_dev->cb_event(JPEG_IRQ_EVENT_PDMA_DONE, 0);
    }

    /* DMA E channel transfer done */
    if((status & JPEG_INT_ST_CLR_EDMA_DONE_Msk) == JPEG_INT_ST_CLR_EDMA_DONE_Msk)
    {
        if(jpeg_dev->cb_event)
            jpeg_dev->cb_event(JPEG_IRQ_EVENT_EDMA_DONE, 0);
    }
}


/* Exported functions --------------------------------------------------------*/
/**
  * @brief  Return Jpeg driver version.
  *
  * @return CSK_DRIVER_VERSION
  */
CSK_DRIVER_VERSION
Jpeg_GetVersion(void)
{
    return jpeg_driver_version;
}


/**
  * @brief  Return Jpeg instance.
  *
  * @return Instance of Jpeg
  */
void* Jpeg0(void)
{
    return &jpeg0_dev;
}

/**
  * @brief  Return addr:
  *
  * @return Jpeg_EncodeIn_DecodeOut_Address
  */
uint32_t Jpeg0_PixelBuf(void)
{
    return JPEG_PIXEL_BUF;
}

/**
  * @brief  Return Addr:
  *
  * @return Jpeg_DecodeIn_EncodeOut_Address
  */
uint32_t Jpeg0_ECSBuf(void)
{
    return JPEG_ECS_BUF;
}

/**
  * @brief  Return Addr:
  *
  * @return Jpeg_EncodeQuanTable_Address
  */
uint32_t Jpeg0_EncodeQuanBuf(void)
{
    return JPEG_QM_BUF;
}

/**
  * @brief  Return Addr:
  *
  * @return Jpeg_EncodeHuffTable_Address
  */
uint32_t Jpeg0_EncodeHuffBuf(void)
{
    return JPEG_HENC_BUF;
}

/**
  * @brief  Return Addr:
  *
  * @return Jpeg_DecodeMinTable_Address
  */
uint32_t Jpeg0_DecodeMinBuf(void)
{
    return JPEG_HM_BUF;
}

/**
  * @brief  Return Addr:
  *
  * @return Jpeg_DecodeBaseTable_Address
  */
uint32_t Jpeg0_DecodeBaseBuf(void)
{
    return JPEG_HB_BUF;
}

/**
  * @brief  Return Addr:
  *
  * @return Jpeg_DecodeSymTable_Address
  */
uint32_t Jpeg0_DecodeSymBuf(void)
{
    return JPEG_HS_BUF;
}


/**
 * @brief Initialize the Jpeg device.
 *
 * @param pJpegDev A pointer to the Jpeg device structure.
 * @param pCallback The callback function for Jpeg events.
 * @param pCfg The configuration structure for the Jpeg device.
 * @return int32_t Returns CSK_DRIVER_OK if initialization is successful, otherwise returns an error code.
 */
int32_t Jpeg_Initialize(void *pJpegDev, void *pCallback, Jpeg_InitTypeDef *pCfg)
{
    Jpeg_DEV *jpeg_dev = safe_jpeg_dev(pJpegDev);

    /* Check the Jpeg instance */
    if(NULL == jpeg_dev)
    {
        DEV_LOG("[%s:%d] Error input: pJpegDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(NULL == pCfg)
    {
        DEV_LOG("[%s:%d] Error input: pCfg is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    Jpeg_Reset();

    memcpy(&jpeg_dev->Init, pCfg, sizeof(Jpeg_InitTypeDef));

    jpeg_dev->Instance->REG_IFCTRL.bit.XMODE= 0x0;             // 0x74 bit1=0 xmode

    switch(pCfg->mode)
    {
        case JPEG_MODE_ENCODE:
            jpeg_dev->Instance->REG_CONTRL.bit.MODE = 0x0;             // 0x04  bit0=0:encoder; 1:decoder
            //jpeg_dev->Instance->REG_JCR1.all = 0x6;                    // 0x803 bit3=0:encoder; 1:decoder
            jpeg_dev->Instance->REG_JCR1.bit.JCR_DE = 0x0;     // 0x804 bit3=0:encoder; 1:decoder
            break;

        case JPEG_MODE_DECODE:
            jpeg_dev->Instance->REG_CONTRL.bit.MODE = 0x1;             // 0x04  bit0=0:encoder; 1:decoder
            //jpeg_dev->Instance->REG_JCR1.all = 0xe;                    // 0x803 bit3=0:encoder; 1:decoder
            jpeg_dev->Instance->REG_JCR1.bit.JCR_DE = 0x1;     // 0x804 bit3=0:encoder; 1:decoder
            break;

        default:
            CLOG("[%s:%d] mode=%d is error", __func__, __LINE__, pCfg->mode);
            return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (pCfg->rst_enable)
        jpeg_dev->Instance->REG_JCR1.bit.JCR_RE = 0x1;           // 0x804 bit2=0:no restart marker insert; 1:hardware insert restart marker
    else
        jpeg_dev->Instance->REG_JCR1.bit.JCR_RE = 0x0;           // 0x804 bit2=0:no restart marker insert; 1:hardware insert restart marker

    switch(pCfg->format_in)
    {
        case JPEG_DECODE_IN_FORMAT_YUV422:
            jpeg_dev->Instance->REG_CONTRL.bit.DEC_IN_FORMAT = 0x0;    // 0x04 bit[2:1] = 0:422; 1:420; 2:444; 3:411 dec_in_format
            jpeg_dev->Instance->REG_JCR1.bit.JCR_NOL = 0x2;            // 0x804 bit0~1=color components number minus one
            jpeg_dev->Instance->REG_JCR4.all = ((pCfg->ht_index[0]>>4)&0x1)|((pCfg->ht_index[0]&0x1)<<1)|((pCfg->qt_index[0]&0x1)<<2)|(0x1<<4); // 0x810 bit0:jcr_HD0  bit1:jcr_HA0  bit[3:2]:jcr_QT0  bit[7:4]:jcr_Nblock0
            jpeg_dev->Instance->REG_JCR5.all = ((pCfg->ht_index[1]>>4)&0x1)|((pCfg->ht_index[1]&0x1)<<1)|((pCfg->qt_index[1]&0x1)<<2)|(0x0<<4); // 0x814 bit0:jcr_HD1  bit1:jcr_HA1  bit[3:2]:jcr_QT1  bit[7:4]:jcr_Nblock1
            jpeg_dev->Instance->REG_JCR6.all = ((pCfg->ht_index[2]>>4)&0x1)|((pCfg->ht_index[2]&0x1)<<1)|((pCfg->qt_index[2]&0x1)<<2)|(0x0<<4); // 0x818 bit0:jcr_HD2  bit1:jcr_HA2  bit[3:2]:jcr_QT2  bit[7:4]:jcr_Nblock2
            break;

        case JPEG_DECODE_IN_FORMAT_YUV420:
            jpeg_dev->Instance->REG_CONTRL.bit.DEC_IN_FORMAT = 0x1;    // 0x04 bit[2:1] = 0:422; 1:420; 2:444; 3:411 dec_in_format
            jpeg_dev->Instance->REG_JCR1.bit.JCR_NOL = 0x2;            // 0x804 bit0~1=color components number minus one
            jpeg_dev->Instance->REG_JCR4.all = ((pCfg->ht_index[0]>>4)&0x1)|((pCfg->ht_index[0]&0x1)<<1)|((pCfg->qt_index[0]&0x1)<<2)|(0x3<<4); // 0x810 bit0:jcr_HD0  bit1:jcr_HA0  bit[3:2]:jcr_QT0  bit[7:4]:jcr_Nblock0
            jpeg_dev->Instance->REG_JCR5.all = ((pCfg->ht_index[1]>>4)&0x1)|((pCfg->ht_index[1]&0x1)<<1)|((pCfg->qt_index[1]&0x1)<<2)|(0x0<<4); // 0x814 bit0:jcr_HD1  bit1:jcr_HA1  bit[3:2]:jcr_QT1  bit[7:4]:jcr_Nblock1
            jpeg_dev->Instance->REG_JCR6.all = ((pCfg->ht_index[2]>>4)&0x1)|((pCfg->ht_index[2]&0x1)<<1)|((pCfg->qt_index[2]&0x1)<<2)|(0x0<<4); // 0x818 bit0:jcr_HD2  bit1:jcr_HA2  bit[3:2]:jcr_QT2  bit[7:4]:jcr_Nblock2
            break;

        case JPEG_DECODE_IN_FORMAT_YUV444:
            jpeg_dev->Instance->REG_CONTRL.bit.DEC_IN_FORMAT = 0x2;    // 0x04 bit[2:1] = 0:422; 1:420; 2:444; 3:411 dec_in_format
            jpeg_dev->Instance->REG_JCR1.bit.JCR_NOL = 0x2;            // 0x804 bit0~1=color components number minus one
            jpeg_dev->Instance->REG_JCR4.all = ((pCfg->ht_index[0]>>4)&0x1)|((pCfg->ht_index[0]&0x1)<<1)|((pCfg->qt_index[0]&0x1)<<2)|(0x0<<4); // 0x810 bit0:jcr_HD0  bit1:jcr_HA0  bit[3:2]:jcr_QT0  bit[7:4]:jcr_Nblock0
            jpeg_dev->Instance->REG_JCR5.all = ((pCfg->ht_index[1]>>4)&0x1)|((pCfg->ht_index[1]&0x1)<<1)|((pCfg->qt_index[1]&0x1)<<2)|(0x0<<4); // 0x814 bit0:jcr_HD1  bit1:jcr_HA1  bit[3:2]:jcr_QT1  bit[7:4]:jcr_Nblock1
            jpeg_dev->Instance->REG_JCR6.all = ((pCfg->ht_index[2]>>4)&0x1)|((pCfg->ht_index[2]&0x1)<<1)|((pCfg->qt_index[2]&0x1)<<2)|(0x0<<4); // 0x818 bit0:jcr_HD2  bit1:jcr_HA2  bit[3:2]:jcr_QT2  bit[7:4]:jcr_Nblock2
            break;

        case JPEG_DECODE_IN_FORMAT_YUV411:
            jpeg_dev->Instance->REG_CONTRL.bit.DEC_IN_FORMAT = 0x3;    // 0x04 bit[2:1] = 0:422; 1:420; 2:444; 3:411 dec_in_format
            jpeg_dev->Instance->REG_JCR1.bit.JCR_NOL = 0x2;            // 0x804 bit0~1=color components number minus one
            jpeg_dev->Instance->REG_JCR4.all = ((pCfg->ht_index[0]>>4)&0x1)|((pCfg->ht_index[0]&0x1)<<1)|((pCfg->qt_index[0]&0x1)<<2)|(0x3<<4); // 0x810 bit0:jcr_HD0  bit1:jcr_HA0  bit[3:2]:jcr_QT0  bit[7:4]:jcr_Nblock0
            jpeg_dev->Instance->REG_JCR5.all = ((pCfg->ht_index[1]>>4)&0x1)|((pCfg->ht_index[1]&0x1)<<1)|((pCfg->qt_index[1]&0x1)<<2)|(0x0<<4); // 0x814 bit0:jcr_HD1  bit1:jcr_HA1  bit[3:2]:jcr_QT1  bit[7:4]:jcr_Nblock1
            jpeg_dev->Instance->REG_JCR6.all = ((pCfg->ht_index[2]>>4)&0x1)|((pCfg->ht_index[2]&0x1)<<1)|((pCfg->qt_index[2]&0x1)<<2)|(0x0<<4); // 0x818 bit0:jcr_HD2  bit1:jcr_HA2  bit[3:2]:jcr_QT2  bit[7:4]:jcr_Nblock2
            break;

        case JPEG_DECODE_IN_FORMAT_GRAY:
            jpeg_dev->Instance->REG_JCR1.bit.JCR_NOL = 0x0;            // 0x804 bit0~1=color components number minus one
            jpeg_dev->Instance->REG_JCR4.all = ((pCfg->ht_index[0]>>4)&0x1)|((pCfg->ht_index[0]&0x1)<<1)|((pCfg->qt_index[0]&0x1)<<2)|(0x0<<4); // 0x810 bit0:jcr_HD0  bit1:jcr_HA0  bit[3:2]:jcr_QT0  bit[7:4]:jcr_Nblock0
            jpeg_dev->Instance->REG_JCR5.all = 0x0;                    // 0x814 bit0:jcr_HD1  bit1:jcr_HA1  bit[3:2]:jcr_QT1  bit[7:4]:jcr_Nblock1
            jpeg_dev->Instance->REG_JCR6.all = 0x0;                    // 0x818 bit0:jcr_HD2  bit1:jcr_HA2  bit[3:2]:jcr_QT2  bit[7:4]:jcr_Nblock2
            break;

        default:
            CLOG("[%s:%d] format_in=%d is error", __func__, __LINE__, pCfg->format_in);
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    switch(pCfg->format_out)
    {
        case JPEG_DECODE_OUT_FORMAT_YUV422:
            jpeg_dev->Instance->REG_CONTRL.bit.DEC_OU_FORMAT = 0x0;    // 0x04 bit[3]   = 0:YUV422; 1:RGB dec_ou_format
            break;

        case JPEG_DECODE_OUT_FORMAT_RGB:
            jpeg_dev->Instance->REG_CONTRL.bit.DEC_OU_FORMAT = 0x1;    // 0x04 bit[3]   = 0:YUV422; 1:RGB dec_ou_format
            break;

        default:
            CLOG("[%s:%d] format_out=%d is error", __func__, __LINE__, pCfg->format_out);
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    jpeg_dev->Instance->REG_PIXEL_DMA_TRANSFER_SIZE.bit.PDMA_SIZE = pCfg->pixel_size;    // 0x10 bit[19:0] Pixel data length in a DMA transfer. (not used)
    jpeg_dev->Instance->REG_ECS_DMA_TRANSFER_SIZE.bit.EDMA_SIZE = pCfg->ecs_size;    // 0x14 bit[19:0] ECS data length in a DMA transfer.
    jpeg_dev->Instance->REG_SOURCE_DATA_LENGTH.bit.SRC_LEN = pCfg->src_size;           // 0x18 bit[23:0] The total length of incoming data of a process.
    //jpeg_dev->Instance->REG_RESULT_DATA_LENGTH.bit.RESULT_LEN = 0x800;          // 0x1C bit[23:0] The length of encoder ECS output data reported by JPEG interface.

    jpeg_dev->Instance->REG_ENC_PIC_SIZE.bit.ARCS_ENC_WIDTH = pCfg->img_width;    // 0x68 bit[27:16]: pic width
    jpeg_dev->Instance->REG_ENC_PIC_SIZE.bit.ARCS_ENC_HEIGHT= pCfg->img_height;    // 0x68 bit[10:0]: pic height
    jpeg_dev->Instance->REG_DEC_PIC_SIZE.bit.ARCS_DEC_WIDTH = pCfg->img_width;    // 0x68 bit[27:16]: pic width
    jpeg_dev->Instance->REG_DEC_PIC_SIZE.bit.ARCS_DEC_HEIGHT= pCfg->img_height;    // 0x68 bit[10:0]: pic height

    // 0x804 bit[1:0]: Number of color components in the image data minus one.
    // 0x804 bit2 = 0: no restart marker insert   1: hardware insert restart marker
    // 0x804 bit3 = 0: encoder   1: decoder
    //jpeg_dev->Instance->REG_JCR1.all = 0x6;
    //jpeg_dev->Instance->REG_JCR2.all = 0x3f;       // 0x808 bit[25:0]: Number of MCUs that will be encoded minus one.
    jpeg_dev->Instance->REG_JCR2.all = (pCfg->img_width_align/pCfg->sampling_h)*(pCfg->img_height_align/pCfg->sampling_v)-1;       // 0x808 bit[25:0]: Number of MCUs that will be encoded minus one.
    jpeg_dev->Instance->REG_JCR3.all = pCfg->rst_num ? (pCfg->rst_num-1) : 0;// 0x80C bit[15:0]: Number of MCUs between tow Restart Markers minus 1.
    //jpeg_dev->Instance->REG_JCR4.all = 0x30;       // 0x810 bit0:jcr_HD0  bit1:jcr_HA0  bit[3:2]:jcr_QT0  bit[7:4]:jcr_Nblock0
    //jpeg_dev->Instance->REG_JCR5.all = 0x7;        // 0x814 bit0:jcr_HD1  bit1:jcr_HA1  bit[3:2]:jcr_QT1  bit[7:4]:jcr_Nblock1
    //jpeg_dev->Instance->REG_JCR6.all = 0x7;        // 0x818 bit0:jcr_HD2  bit1:jcr_HA2  bit[3:2]:jcr_QT2  bit[7:4]:jcr_Nblock2
    jpeg_dev->Instance->REG_JCR7.all = 0x0;        // 0x81C bit0:jcr_HD3  bit1:jcr_HA3  bit[3:2]:jcr_QT3  bit[7:4]:jcr_Nblock3

    // 0x20 dec_dummy bit[1:0]= 00: Normal   01: 1 dummy byte   10: 2 dummy bytes   11: 3 dummy bytes
    // 0x40 int_st_clr W1C  bit3:edma_done   bit2:pdma_done   bit0:codec_done
    // 0x44 int_mask        bit3:edma_done   bit2:pdma_done   bit0:codec_done
    // 0x60 scaling_ctrl

    jpeg_dev->Instance->REG_INT_ST_CLR.all = (1 << 3) | (1 << 2) | (1 << 0);
    jpeg_dev->Instance->REG_INT_MASK.all = 0x0;

    jpeg_dev->cb_event = (Jpeg_SignalEvent_t)pCallback;

    /* Init the low level hardware and interrupt */
    register_ISR(IRQ_JPG_VECTOR, (ISR)Jpeg_IRQ_Handler, NULL);
    clear_IRQ(IRQ_JPG_VECTOR);
    enable_IRQ(IRQ_JPG_VECTOR);

    return CSK_DRIVER_OK;
}


/**
 * @brief Uninitializes the Jpeg device.
 *
 * This function uninitializes the Jpeg device by performing a reset and disabling the VI.
 *
 * @param pJpegDev A pointer to the Jpeg device structure.
 * @return CSK_DRIVER_OK if the operation is successful, otherwise returns an error code.
 */
int32_t Jpeg_Uninitialize(void *pJpegDev)
{
    Jpeg_DEV *jpeg_dev = safe_jpeg_dev(pJpegDev);

    /* Check the Jpeg instance */
    if(NULL == jpeg_dev)
    {
        DEV_LOG("[%s:%d] Error input: pJpegDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    Jpeg_Stop(jpeg_dev);

    Jpeg_Reset();

    memset(&jpeg_dev->Init, 0, sizeof(Jpeg_InitTypeDef));

    disable_IRQ(IRQ_JPG_VECTOR);
    register_ISR(IRQ_JPG_VECTOR, NULL, NULL);

    return CSK_DRIVER_OK;
}


/**
 * @brief Start the Jpeg to capture video frames.
 *
 * @param pJpegDev A pointer to the Jpeg device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t Jpeg_Start(void *pJpegDev)
{
    Jpeg_DEV *jpeg_dev = safe_jpeg_dev(pJpegDev);

    /* Check the Jpeg instance */
    if(NULL == jpeg_dev)
    {
        DEV_LOG("[%s:%d] Error input: pJpegDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    jpeg_dev->Instance->REG_JCR0.bit.JCR_START_STOP = 0x1;             // 0x800 bit0: Start or stop the encoding or decoding process
    jpeg_dev->Instance->REG_RELOAD.bit.RELOAD = 0x1;                   // 0x70 bit0: reload  W1P
    jpeg_dev->Instance->REG_PIXEL_DMA_START.bit.PDMA_START = 0x1;      // 0x08 bit0: Write 1 to start pixel DMA transfer. (not used)
    jpeg_dev->Instance->REG_ENC_PDMA_START.bit.ARCS_PDMA_START = 0x1;  // 0x64 bit0: pixel dma start  W1P
    jpeg_dev->Instance->REG_ECS_DMA_START.bit.EDMA_START = 0x1;        // 0x0C bit0: Write 1 to start ECS DMA transfer.
    jpeg_dev->Instance->REG_IFCTRL.bit.JPEG_ENABLE= 0x1;               // 0x74 bit0: jpeg_enable

    /* Return function status */
    return CSK_DRIVER_OK;
}


/**
 * @brief Stops the Jpeg and releases its resources.
 *
 * @param pJpegDev A pointer to the Jpeg device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t Jpeg_Stop(void *pJpegDev)
{
    Jpeg_DEV *jpeg_dev = safe_jpeg_dev(pJpegDev);

    /* Check the Jpeg instance */
    if(NULL == jpeg_dev)
    {
        DEV_LOG("[%s:%d] Error input: pJpegDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    jpeg_dev->Instance->REG_JCR0.bit.JCR_START_STOP = 0x0;             // 0x800 bit0: Start or stop the encoding or decoding process
    jpeg_dev->Instance->REG_IFCTRL.bit.JPEG_ENABLE= 0x0;               // 0x74 bit0: jpeg_enable

    return CSK_DRIVER_OK;
}



